/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday.
 * @date updated 4th of March 2026, Wednesday.
 */


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/neoconnection.h"




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief constructor
 */
NeoConnection::NeoConnection(const std::string& urls, BoltValue* pauth, BoltValue* pextras)
    : encoder(write_buf), decoder(read_buf), pauth(pauth), pextras(pextras), is_done(false)
{
    // defaults
    client_id = -1;
    tran_count = 0;
    current_msg_len = 0;
    unconsumed_count = 0;

    is_open = false;
    recv_paused = false;

    // set the url
    size_t pos = urls.find_first_of("://");
    std::string temp{ urls };
    if (pos != std::string::npos)
    {
        if (!urls.substr(0, pos).compare("bolt+s"))
            Enable_SSL();

        temp = urls.substr(pos + 3, urls.length() - (pos + 3));
    } // end if pos

    // split into host and port part
    std::vector<std::string> url{ Utils::Split_String(temp, ':') };
    if (url.size() == 2) Set_Host_Address(url[0], url[1]);
} // end NeoConnection


/**
 * @brief destructor
 */
NeoConnection::~NeoConnection() {}


/**
 * @brief performs version negotiation as specified by the bolt protocol. It uses
 *  v5.7+ manifest negotiation style to allow the server respond with version
 *  numbers supported. If server does not support manifest, it simply starts
 *  with version 4 or less. However, the function uses kooked up version numbers,
 *  anyone interested in supporting more versions can simply add them to the 
 *  versions array or have it read from a config file or something. The 
 *  function then picks the highest version supported by the server and sends it 
 *  back to the server to complete the negotiation.
 *
 * @return LB_0K on success alas status errors; LB_FAIL on version negotiation
 *  fail or LB_RETRY on network/ssl fail
 */
LBStatus NeoConnection::Negotiate_Version()
{
    LBStatus rc;    // return status codes
    tasks.Clear();  // make certain no false moves here

    int buf_len = 128;
    u8 versions[128]{
        0x60, 0x60, 0xB0, 0x17,         // neo4j magic number
        0x00, 0x00, 0x01, 0xFF,         // manifest v1
        0x00, 0x00, 0x04, 0x04,         // if not try version 4
        0x00, 0x00, 0x00, 0x03,         // version 3 and ...
        0x00, 0x00, 0x00, 0x02          // version 2 (last two are not supported)
    };

    int len = 20;    // length of versions to send
    rc = Send(versions, len);
    if (!LB_OK(rc)) return LB_Add_Stage(rc, LBStage::LB_STAGE_HANDSHAKE);  // LB_RETRY

    rc = Recv((char*)versions, buf_len);
    if (!LB_OK(rc)) return LB_Add_Stage(rc, LBStage::LB_STAGE_HANDSHAKE);  // LB_RETRY

    u8* ptr = versions;
    u64 nums;           // stores count of supported versions

    // are we decoding v5.7+ VarInt spec?
    if (ntohl(*(u32*)ptr) == 0x000001FF)
    {
        ptr += 4;           // start of length of addresses
        int count = 0;      // tracks current offset

        // test the first round and loop based on that
        if (!(ptr[0] & 0x80))
        {
            nums = (ptr[0] & 0x7F);
            ++ptr;
        } // end if first filter

        while ((ptr[0] & 0x80) && count < 8)
        {
            nums |= ((ptr[0] & 0x7F) << (count << 3));
            ++ptr; ++count;
        } // end while cont
    } // end if v5.7+ spec
    else if (ntohl(*(u32*)ptr) == 0)
    {
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT,
            LBStage::LB_STAGE_HANDSHAKE,
            LBCode::LB_CODE_VERSION);
    } // end else if unsupported version
    else nums = 1;

    // pick the higest version
    iZero(&supported_version, sizeof(supported_version));
    u32* alias = reinterpret_cast<u32*>(ptr);
    for (int i = 0; i < nums; i++)
    {
        Neo4jVerInfo* v = reinterpret_cast<Neo4jVerInfo*>(ptr);
        if ((supported_version.major < v->major) ||
            (supported_version.major == v->major && supported_version.minor < v->minor))
        {
            supported_version = *v;
            alias = reinterpret_cast<u32*>(ptr);
        } // end if supported

        ptr += sizeof(u32);
    } // end for

    // send to server; and echo back whatever the server caps are
    len = 5;
    return Send(alias, len);
} // end Negotiate_Version


/**
 * @brief connects to neo4j server using its latesest (as of this writing) v6.0 HELLO
 *  handshake message. The message/payload consists mainly creds and other extra
 *  stuff user could add that is based on the bolt protocol v6.0 spec. The v6.0
 *  spec consists of two steps like v5.x, a basic hello and a logon message 
 *  after version negotiation, thus LightningBolt implements both steps here as 
 *  states of the driver tasks.On successful compeletion its sets the task state to LOGON
 *  to signal the next state.
 *
 * @return LB_OK on success, alas LB_RETRY pretaining to network/kernel/ssl errors
 */
LBStatus NeoConnection::Send_Hellov5(TaskState& state)
{
    // reject false calls as invalid states
    if (state != TaskState::Hello)
    {
        return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_STATE,
            LBStage::LB_STAGE_HELLO, LBCode::LB_CODE_TASKSTATE, 0);
    } // end if bad task

    BoltMessage hello;
    float version = supported_version.Get_Version();

    // memorize pool position
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

    // extract out any extra parameters from conn_params, supplied from the
    //  user during connection string
    struct Param_Helper
    {
        std::string key;
        BoltValue val;
        float active_version;
        float removed_version;
    };
    std::vector<Param_Helper> param_list{
        {USER_AGENT_STRING, (pextras && pextras[0][USER_AGENT_STRING].type != BoltType::Unk) ?
            pextras[0][USER_AGENT_STRING] :
            BoltValue(("LB/v" + std::to_string(client_id + 1) + ".0").c_str()), 1.0, 100},

        {PATCH_BOLT_STRING, (pextras ? pextras[0][PATCH_BOLT_STRING] : BoltValue::Make_Unknown()), 4.3, 4.4},
        {ROUTES_STRING, (pextras ? pextras[0][ROUTES_STRING] : BoltValue::Make_Unknown()), 4.1, 100},
        {NOTIF_MIN_SEVERITY_STRING, (pextras ? pextras[0][NOTIF_MIN_SEVERITY_STRING]
            : BoltValue::Make_Unknown()), 5.2, 100},
        {NOTIF_DISABLED_CATS_STRING,
            (pextras ? pextras[0][NOTIF_DISABLED_CATS_STRING] : BoltValue::Make_Unknown()), 5.2, 5.4}
    };

    LBStatus rc = 0;
    hello.msg = BoltValue::Make_Struct(BOLT_HELLO);
    BoltValue bmp = BoltValue::Make_Map();
    for (auto& param : param_list)
    {
        if (param.val.type != BoltType::Unk &&
            (param.active_version <= version && param.removed_version > version))
        {
            bmp.Insert_Map(param.key.c_str(), param.val);
        } // end if not unknown
    } // end for params

    // add bolt agent info
    if (version >= 5.3)
    {
        bmp.Insert_Map(BOLT_AGENT_STRING, BoltValue({
                mp(PRODUCT_STRING, PRODUCT_VALUE),
                mp(PLATFORM_STRING, PLATFORM_VALUE),
                mp(LANGUAGE_STRING, LANGUAGE_VALUE),
            }));
    } // end sversion

    hello.msg.Insert_Struct(bmp);
    rc = encoder.Encode(hello);
    if (!LB_OK(rc))
    {
        rc = Retry_Encode(hello);
        if (!LB_OK(rc))
        {
            Release_Pool<BoltValue>(offset);
            return rc;
        } // end if still bad
    } // end if wasn't good

    rc = Flush();
    Release_Pool<BoltValue>(offset);
    return rc;
} // end Send_Hellov5


/**
 * @brief start's a hello signal after a successful version negotiation using v4.xx
 *  of the bolt protocol. The message/payload consists of:
 *      scheme:     a key value that defines the authentication method
 *      user_agent: a key value pair identifer that conforms to Name/Version
 *      principal:  the user name for neo4j database
 *      credentials:the password for neo4j
 *      routing:    an optional routing context defined as a dictionary type
 *
 * Because the driver uses minimal parameter count it could also be used
 *  for legacy version handshake.
 *
 * @return LB_OK on success, alas LB_RETRY pretaining to network/kernel/ssl errors
 */
LBStatus NeoConnection::Send_Hellov4(TaskState& state)
{
    // reject false calls as invalid states
    if (state != TaskState::Hello)
    {
        return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_STATE,
            LBStage::LB_STAGE_HELLO, LBCode::LB_CODE_TASKSTATE, 0);
    } // end if bad task

    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    std::string uagent{ ("LB/" + std::to_string(client_id + 1) + ".0").c_str() };

    BoltValue map = pauth[0];
    map.Insert_Map(USER_AGENT_STRING, BoltValue(uagent.c_str()));
    BoltMessage hello(BoltValue(BOLT_HELLO, {
        map
        }));

    LBStatus rc = encoder.Encode(hello);
    if (!LB_OK(rc))
    {
        rc = Retry_Encode(hello);
        if (!LB_OK(rc))
        {
            Release_Pool<BoltValue>(offset);
            return rc;
        } // end if still bad
    } // end if wasn't good

    rc = Flush();
    Release_Pool<BoltValue>(offset);
    return rc;
} // end Send_Hello


/**
 * @brief sends LOGON message after a successful HELLO in v5.1+ of bolt. The function
 *  exits early should the current task state doesn't match TaskState::LOGON. The
 *  state is only checked for explicit control and prevent calls that should never 
 *  happen as at the current stage state is implicit.
 *
 * @return LB_OK on success, LB_FAIL and LB_RETRY on fail.
 */
LBStatus NeoConnection::Logon(TaskState& state)
{
    // reject false calls as invalid states
    if (state != TaskState::Logon)
    {
        return LB_Make(
            LBAction::LB_FAIL, 
            LBDomain::LB_DOM_STATE,
            LBStage::LB_STAGE_AUTH, 
            LBCode::LB_CODE_TASKSTATE
        );
    } // end if bad task

    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage logon = (BoltValue(BOLT_LOGON, { pauth[0] }));
    LBStatus rc = Encode_And_Flush(TaskState::Logon, logon);
    
    Release_Pool<BoltValue>(offset);
    return rc;
} // end Logon


/**
 * @brief runs a cypher query against the connected neo4j database. The 
 *  function appends a PULL/ALL message after the RUN message to begin 
 *  fetching results asap.
 *
 * @param cypher the cypher query string
 * @param params optional parameters for the cypher query
 * @param extras optional extra parameters for the cypher query (see bolt specs)
 * @param n optional the numbe r of chunks to request, i.e. 1000 records
 * @param cb optional callback for async results
 *
 * @return 0 on success and -2 on application error
 */
LBStatus NeoConnection::Run(const char* cypher, 
    const BoltValue& params, 
    const BoltValue& extras,
    const int n,
    std::function<void(BoltResult&)> cb)
{
    // protect pool
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

    // encode and flush cypher query, append a pull too
    BoltMessage run(
        BoltValue(BOLT_RUN, {
            cypher,
            params,
            extras
            })
        );

    if (!tasks.Enqueue({ TaskState::Run, cb }))
    {
        Release_Pool<BoltValue>(offset);
        return LB_Make(
            LBAction::LB_FAIL,
            LBDomain::LB_DOM_STATE,
            LBStage::LB_STAGE_QUERY,
            LBCode::LB_CODE_STATE_QUEUE_MEM
        );
    } // end if enqueue error

    LBStatus rc = encoder.Encode(run);
    if (!LB_OK(rc))
    {
        rc = Retry_Encode(run);
        if (!LB_OK(rc))
        {
            Release_Pool<BoltValue>(offset);
            return rc;
        } // end if still bad
    } // end if wasn't good
    Encode_Pull(n);

    rc = Flush();
    Release_Pool<BoltValue>(offset);
    return rc;
} // end Run_Query


/**
 * @brief Begins a transaction with the database, this is a manual transaction
 *  that requires commit or rollback to finish.
 *
 * @param options optional parameters for the transaction
 *
 * @return LB_OK status on success or LB_FAIL on send fails
 */
LBStatus NeoConnection::Begin(const BoltValue& options)
{
    if (tran_count++ != 0)
        return 0;       // LB_OK

    // keep track of pool, from here on we allocate from it
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage begin(
        BoltValue(BOLT_BEGIN, {
            options
            })
        );

    LBStatus rc = Encode_And_Flush(TaskState::Begin, begin);
    if (!LB_OK(rc))
    {
        Release_Pool<BoltValue>(offset);
        return rc;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return rc;
} // end Begin_Transaction


/**
 * @brief Commits the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 *
 * @param options optional parameters for the commit
 *
 * @return LB_OK on success. LB_FAIL on failures.
 */
LBStatus NeoConnection::Commit(const BoltValue& options)
{
    if (tran_count > 0)
    {
        --tran_count;
        return 0;       // got a few more
    } // end if

    // keep track of pool, from here on we allocate from it
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage commit(
        BoltValue(BOLT_COMMIT, {
            options
            })
        );

    LBStatus rc = Encode_And_Flush(TaskState::Commit, commit);
    if (!LB_OK(rc))
    {
        Release_Pool<BoltValue>(offset);
        return rc;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return rc;
} // end Commit_Transaction


/**
 * @brief Rolls back the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 *
 * @param options optional parameters for the rollback
 *
 * @return LB_OK on success. Alas LB_FAIL on flush error
 */
LBStatus NeoConnection::Rollback(const BoltValue& options)
{
    if (tran_count > 0)
    {
        --tran_count;
        return 0;       // got a few more
    } // end if

    // keep track of pool, from here on we allocate from it
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage rollback(
        BoltValue(BOLT_ROLLBACK, {
            options
            })
    );

    LBStatus rc = Encode_And_Flush(TaskState::Rollback, rollback);
    if (!LB_OK(rc))
    {
        Release_Pool<BoltValue>(offset);
        return rc;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return rc;
} // end Rollback_Transaction


/*@brief encodes a PULL message and sends it. Useful during reactive style fetch
*
* @param n the number of chunks to to fetch at once, defaulted to -1 to fetch
* everything.
*
* @return 0 on success -1 on sys error, left for caller to decide its fate
*/
int NeoConnection::Pull(const int n)
{
    Encode_Pull(n);
    if (!Flush())
        return -1;

    return 0;
} // end Pull


/**
 * @brief encodes a DISCARD message and sends it. Useful during reactive style fetch
 *
 * @param n the number of chunks to to discard at once, defaulted to -1 to discard
 *
 * @return 0 on success -1 on sys error, left for caller to decide its fate
 */
int NeoConnection::Discard(const int n)
{
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage discard(BoltValue(BOLT_DISCARD, {
        BoltValue({
            mp("n", n),
            mp("qid", client_id)
            })
        }));

    if (!Encode_And_Flush(TaskState::Discard, discard))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Discard


/**
 * @brief sends a TELEMETRY message to server with the api level used by client.
 *  This is a v5.1+ feature.
 *
 * @param api the api level used by client
 *
 * @return 0 on success, -1 on sys error
 */
int NeoConnection::Telemetry(const int api)
{
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage tel(BoltValue(BOLT_TELEMETRY, { api }));

    if (!Encode_And_Flush(TaskState::Telemetry, tel))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Telemetry


/**
 * @brief resets the current active connection should it be in FAILED state.
 *  The client connection then must reset its state to READY before accepting
 *  any more new requests. The state of the connection must be in ERROR state
 *  prior to calling this function.
 *
 * @return 0 on success always
 */
LBStatus NeoConnection::Reset()
{
    // memorize the last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage reset(BoltValue(BOLT_RESET, {}));

    LBStatus rc = Encode_And_Flush(TaskState::Reset, reset);
    if (!LB_OK(rc))
    {
        Release_Pool<BoltValue>(offset);
        return rc;
    } // end if no flush
    
    Release_Pool<BoltValue>(offset);
    return rc;
} // end Reset


/**
 * @brief sends a LOGOFF message to server to gracefully logoff from the
 *  current active connection. This is a v5.1+ feature.
 *
 * @return 0 on success always
 */
LBStatus NeoConnection::Logoff()
{
    LBStatus rc;

    // get the last offset in the pool to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage off(BoltValue(BOLT_LOGOFF, {}));

    rc = Encode_And_Flush(TaskState::Logoff, off);
    if (!LB_OK(rc))
    {
        Release_Pool<BoltValue>(offset);
        return rc;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return rc;
} // end Logoff


/**
 * @brief sends a GOODBYE message to server to gracefully close the current
 *  active connection.
 *
 * @return 0 on success, -1 on sys error
 */
int NeoConnection::Goodbye()
{
    // memorize last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage gb(BoltValue(BOLT_GOODBYE, {}));

    if (!Encode_And_Flush(TaskState::Logoff, gb))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Goodbye


/**
 * @brief this is an old skool way of error handling in neo4j bolt protocol. It
 *  basically acknowledges the last failure message sent by server and resets
 * I wrote it for completeness.
 *
 * @eturn 0 on success, -1 on sys error
 */
int NeoConnection::Ack_Failure()
{
    // memorize last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage ack(BoltValue(BOLT_ACK_FAILURE, {}));

    if (!Encode_And_Flush(TaskState::Ack_Failure, ack))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Failure


/**
 * @brief sends a ROUTE message to the server to get routing table information.
 *  The function adapts to the supported version of the server and sends the
 *  appropriate ROUTE message.
 *
 * @param routing the routing context map
 * @param bookmarks optional bookmarks list
 * @param database optional database name, defaulted to "neo4j"
 * @param extra optional extra map for v4.3+
 *
 * @return 0 on success, -1 on sys error
 */
int NeoConnection::Route(BoltValue routing,
    BoltValue bookmarks,
    const std::string& database,
    BoltValue extra)
{
    // keep track of pool, from here on we allocate from it
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

    // what version are we running?
    BoltMessage route;
    float version = static_cast<float>(supported_version.major +
        (static_cast<float>(supported_version.minor) / 10.f));

    if (version >= 4.3)
    {
        // v4.3 and above
        route = Routev43(routing, bookmarks, database, extra);
    } // end if v4.3+
    else if (version >= 4.2)
    {
        // v4.2
        route = Routev42(routing, bookmarks, database);
    } // end else if v4.2
    else
    {
        // legacy route
        route = Route_Legacy(routing);
    } // end else

    if (!Encode_And_Flush(TaskState::Route, route))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Route


/**
 * @brief termiates the active connection via Disconnect() member and sets the
 *  boolean member is_open to false.
 */
void NeoConnection::Terminate()
{
    Goodbye();
    Disconnect();
    is_open = false;
    read_buf.Reset();
    write_buf.Reset();
} // end Terminate


/**
 * @brief sets the client id from the parameter provided should it
 *  be different from the current client id. This is useful for 
 *  connection pooling
 *
 * @param cli_id the new client id to set
 */
void NeoConnection::Set_ClientID(const int cli_id)
{
    if (client_id != cli_id) client_id = cli_id;
} // end Set_ClientID


/**
 * @brief sets the host address and port for later use during reconnection
 *
 * @param host the hostname or ip address to set
 * @param port the port number as string to set
 */
void NeoConnection::Set_Host_Address(const std::string& host, const std::string& port)
{
    hostname = host;
    this->port = port;
} // end Set_HostAddress



//===============================================================================|
/**
 * @brief sends the contents of write buffer to the connected peer.
 *
 * @return LBStatus codes with LB_OK being successful.
 */
LBStatus NeoConnection::Poll_Writable()
{
    LBStatus rc = Send(write_buf.Read_Ptr(), write_buf.Size());
    if (LB_OK(rc)) write_buf.Consume(LB_Aux(rc));

    return rc;
} // end Poll_Writable


/**
 * @brief waits on recieve /recv system call on non-blocking mode. Once done
 *  it must be reset back for the loop to continue. Should the buffer lack space
 *  to recv chunks it grows to accomoditate more.
 *
 * Optionally it can be controlled to shrink back inside of a pool based on some
 *  statistically collected traffic data.
 *
 * @param cell pointer to neocell class used to invoke the method
 * @param fn_decoder pointer to a decoder function/callback
 * @param has_more a reference to sentinel that controls the loop
 *
 * @return LB_OK on with packed bytes read on success, alas LB_RETRY or LB_FAIL
 */
LBStatus NeoConnection::Poll_Readable()
{
    LBStatus rc = 0;        // store's return value

    // have we run out of space?
    if (read_buf.Writable_Size() == 0)
    {
        if ((read_buf.Grow()) < 0)
        {
            // outta memory, try and compact first if we can,
            //   alas pause recv, consume buffer and try again
            if (!read_buf.Compact())
            {
                if (++unconsumed_count > 100)
                {
                    return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_STATE,
                        LBStage::LB_STAGE_DECODE, LBCode::LB_CODE_STATE_MEM);
                } // end if

                recv_paused = true;
            } // end if no compaction
            else
            {
                unconsumed_count = 0;
                recv_paused = false;
            } // end else
        } // end if no room to grow
    } // end if

    if (!recv_paused)
    {
        rc = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
        if (!LB_OK(rc))
            return rc; // fail, retry or wait

        int bytes = LB_Aux(rc);
        read_buf.Advance(bytes);
        return LBOK_INFO(bytes);
    } // end if can recv
    else 
        return LB_Make();  // its compacted anyhow
} // end Poll_Readable


/**
 * @brief persuming that data has been fully received from Poll_Readable(), the
 *  function decodes the bolt encoded responses (message).
 */
LBStatus NeoConnection::Decode_One(DecoderTask& task)
{
	LBStatus rc;    // holds return value
    u8 s = static_cast<u8>(task.state);
    u8 tag = task.view.cursor[3];

    switch (tag)
    {
    case BOLT_SUCCESS:
        rc = (this->*success_handler[s])(task);
        break;

    case BOLT_FAILURE:
        rc = Handle_Failure(task);
        break;

    case BOLT_RECORD:
        rc = Handle_Record(task);
        break;

    case BOLT_IGNORED:
        rc = Handle_Ignored();
        break;

    default:
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT,
            LBStage::LB_STAGE_DECODEING_TASK, LBCode::LB_CODE_PROTO, tag);
    } // end switch

    return rc;
} // end Decode_Response


/**
 * @brief determines if a compelete bolt packet has been receved. The requirement
 *  of more data is left to the calling routine. The method only checks for
 *  a complete bolt packet;
 *  i.e. header_size + 2 + 2 (should trailing 0 exist) <= bytes_remain
 *
 * @return a true if a complete bolt packet is recieved
 */
LBStatus NeoConnection::Can_Decode(u8* view, const u32 bytes_remain)
{
    u16 temp;  // vars

    if (bytes_remain <= 4)
        return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT);  

    // get the message size from the header
    memcpy(&temp, view, sizeof(u16));
    current_msg_len = ntohs(temp) + 2;      // message size + header size

    // padding?
    if ((current_msg_len + 2) <= bytes_remain)
    {
        if (reinterpret_cast<u16*>(view + current_msg_len)[0] == 0)
            current_msg_len += 2;
    } // end if enough bytes for padding too
    else
        return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT);

    // sweet, now make sure this is a proper bolt packet by
    //  checking the signature byte, if not then it's a protocol error
    if ((0xB0 & view[2]) != 0xB0)
    {
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT,
            LBStage::LB_STAGE_DECODEING_TASK, LBCode::LB_CODE_PROTO);
    } // end if not valid

    // we can decode it
    return LBOK_INFO(current_msg_len);
} // end Can_Decode


/**
 * @brief return's the client id for this driver
 */
int NeoConnection::Get_Client_ID() const
{
    return client_id;
} // end client_id


/**
 * @brief makes sure all the contents of the write buffer has been written to the
 *  sending buffer and kernel is probably sending it. The function also resets
 *  the send buffer once data is fully uploaded.
 *
 * @return LBStatus with LB_OK being a succesful call.
 */
LBStatus NeoConnection::Flush()
{
    LBStatus rc;
    while (!write_buf.Empty())
    {
        rc = Poll_Writable();
        if (!LB_OK(rc)) break;   // has to be a syscall error always

        // Optional: safety guard
        if (write_buf.Size() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } // end while

    write_buf.Reset();
    return rc;
} // end Flush


/**
 * @brief determines if the record streaming is done based on the presence of
 *  "has_more" key in the summary message map.
 *
 * @param cursor pointer to the current position in the buffer
 * @param v reference to boltvalue to decode into
 *
 * @return true if record streaming is done else false
 */
bool NeoConnection::Is_Record_Done(BoltMessage& summary)
{
    if (summary.msg(0).type == BoltType::Map &&
        summary.msg(0)["has_more"].type != BoltType::Unk &&
        summary.msg(0)["has_more"].bool_val == true)
    {
        //t.state = TaskState::Streaming;    // still streaming
        return false;   // not done
    } // end if
    else
    {
        //t.state = TaskState::Done;
        return true;
    } // end else done
} // end Is_Record_Done


/**
 * @brief encodes the boltvalue reference and flushes it to peer after it saved
 *  its state into the query_states queue for later use during response decoding.
 *
 * @param s task state used along side requests and responses
 * @param msg the BoltValue to encode and send
 *
 * @return true on success alas false on sys error
 */
LBStatus NeoConnection::Encode_And_Flush(TaskState s, BoltMessage& msg)
{
    tasks.Enqueue({ s });
    LBStatus rc = encoder.Encode(msg);
    if (!LB_OK(rc))
    {
        rc = Retry_Encode(msg);
        if (!LB_OK(rc))
            return rc;
	} // end if wasn't good

	rc = Flush();
    return rc;
} // end Encode_And_Flush


/**
 * @brief a dummy function
 */
inline LBStatus NeoConnection::Success_None(DecoderTask& task)
{
    tasks.Dequeue();
    return LBOK_INFO(task.view.size);
} // end if None


/**
 * @brief First message received after authentication with Neo4j graphdb server.
 *  We expect two major modes of connections v4.x and v5.x (latest). In v5.x we deal
 *  with this function twice HELLO + LOGON. Once successfuly authenticated it
 *  sets the driver state to ready.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK for success or LB_HASMORE if pending more
 */
inline LBStatus NeoConnection::Success_Hello(DecoderTask& task)
{
    if (supported_version.Get_Version() >= 5.1 && task.state == TaskState::Hello)
    {
		task.state = TaskState::Logon;
        LBStatus rc = Logon(task.state);     // log on message
        if (!LB_OK(rc)) return rc;
        return LBOK_INFO(task.view.size);
    } // end if

    BoltResult r;
    r.done = true;  // not gonna care about meta
    results.Enqueue(std::move(r));
    tasks.Dequeue();
    Wake();         // unhalt the waiting process

    return LBOK_INFO(task.view.size);
} // end Success_Hello


/**
 * @brief this handles a successful run query message; we save the
 *  metadata returned for the next records inside the view field_names member.
 *  Sets the state to pull to inidcate we expect records next.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Success_Run(DecoderTask& task)
{
    BoltResult result;
    task.state = TaskState::Pull;

    // parse and save the field names; until its needed and guranteed to exist
    //  as long as we are streaming the result. 
    // on success returns LB_OK_INFO with aux # of bytes to skip buffer
    LBStatus rc = decoder.Decode(task.view.cursor, result.fields);
    if (!LB_OK(rc))
        return rc;

    result.pdec = &decoder;
	result.start_offset = (task.view.cursor + LB_Aux(rc)) - read_buf.Data();
    results.Enqueue(std::move(result));

    return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
        LBStage::LB_STAGE_QUERY,
        LBCode::LB_CODE_NONE, LB_Aux(rc));
} // end Success_Run


/**
 * @brief handles the success summary message sent after the completion of each
 *  record streaming. If the summary message contains "has_more" key and is set
 *  to true then it persumes not done and sets the state back to PULL and returns
 *  OK has more to continue receiving.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Success_Record(DecoderTask& task)
{
    auto result = results.Front();
    LBStatus rc = decoder.Decode(task.view.cursor, result->get().summary);
    if (!LB_OK(rc))
        return rc;

    if (!Is_Record_Done(result->get().summary))
        return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
            LBStage::LB_STAGE_QUERY,
            LBCode::LB_CODE_NONE, LB_Aux(rc));

    tasks.Dequeue();
    Wake();
    return LBOK_INFO(LB_Aux(rc));  // should be LB_OK_INFO
} // end Success_Record


/**
 * @brief handles the success reset message sent after a RESET command is sent
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Success_Reset(DecoderTask& task)
{
    read_buf.Reset();
    return LBOK_INFO(task.view.size);
} // end Success_Reset


/**
 * @brief this is called immidiately after Successful Run, and it simply sets the
 *  member view to point at the next bytes and sets has_more to false to make
 *  sure the recv loop won't run again unless required through Fecth(). It updates
 *  the state to streaming to indicate driver is now consuming buffer.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Handle_Record(DecoderTask& task)
{
    auto result = results.Front();
    task.state = TaskState::Record;
    result->get().message_count++;
	result->get().total_bytes += current_msg_len;

    return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
        LBStage::LB_STAGE_QUERY,
        LBCode::LB_CODE_NONE, current_msg_len);
} // end Success_Record


/**
 * @brief decodes the stream containing the error, sets the connection
 *  error string and takes appropriate action based on the current state.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Handle_Failure(DecoderTask& task)
{
    LBDomain domain = LBDomain::LB_DOM_NEO4J;
    LBAction action;

    BoltResult r;
    r.pdec = &decoder;
    r.message_count = 1;
    r.error = true;
    r.start_offset = task.view.cursor - read_buf.Data();
    r.total_bytes += current_msg_len;
    results.Enqueue(std::move(r));

    TaskState qs = task.state;
    switch (qs)
    {
    case TaskState::Hello:
    case TaskState::Logon:
    case TaskState::Logoff:
        action = LBAction::LB_FAIL;
        tasks.Dequeue();
        Wake();
        break;

    case TaskState::Run:
        /*if (!std::string("Neo.ClientError.Cluster.NotALeader").compare(task.result.err.msg(0)["neo4j_code"].ToString()) ||
            !std::string("Neo.ClientError.General.ForbiddenOnReadOnlyDatabase").compare(task.result.err.msg(0)["neo4j_code"].ToString()))
            action = LBAction::LB_REROUTE;
        else if (!std::string("Neo.TransientError.General.DatabaseUnavailable").compare(task.result.err.msg(0)["neo4j_code"].ToString()) ||
            !std::string("Neo.TransientError.Transaction.DeadlockDetected").compare(task.result.err.msg(0)["neo4j_code"].ToString()))
            action = LBAction::LB_RETRY;
        else action = LBAction::LB_FAIL;*/
        action = LBAction::LB_FAIL;
        break;

    }; // end switch

    return LB_Make(action, domain, 
        LBStage::LB_STAGE_NONE,  // not yet inferred
        LBCode::LB_CODE_NONE, current_msg_len);
} // end Handle_Failure


/**
 * @brief handles the IGNORED message from server; it simply terminates the
 *  connection as it indicates a serious state out of sync.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Handle_Ignored()
{
    auto task = tasks.Dequeue();
    if (tasks.Is_Empty())
        return Reset();

    return LBOK_INFO(task->view.size);
} // end Handle_Ignored


/**
 * @brief encodes a PULL message after a RUN command to fetch all results.
 *
 * @param n the number of chunks to to fetch at once, defaulted to -1 to fetch
 *  everything.
 */
void NeoConnection::Encode_Pull(const int n)
{
    // memorize last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage pull(BoltValue(
        BOLT_PULL, {
            {mp("n", n), mp("qid",-1)}
        }));

    encoder.Encode(pull);
    Release_Pool<BoltValue>(offset);
} // end Send_Pull


/**
 * @brief waits completion of the next streaming. When the atomic is_done is
 *  set to true it breaks the loop and terminates. It also wakes/notifies a waiting
 *  process to continue processing. On exit resets is_done back to false so that
 *  the next caller can wait till done if needed.
 */
void NeoConnection::Wait_Task()
{
    while (1)
    {
        bool prev = is_done.load(std::memory_order_acquire);
        if (prev) break;

        is_done.wait(prev);
    } // end while

    is_done.store(false, std::memory_order_release);    // reset
} // end Wait_Result


/**
 * @brief wakes a waiting process viz its atomic is_done. Function sets is_done to
 *  true and sends notifications to wake a waiting thread.
 */
void NeoConnection::Wake()
{
    is_done.store(true, std::memory_order_release);
    is_done.notify_one();
} // end Wake



/**
 * @brief encodes a ROUTE message for v4.3+ servers
 *
 * @param routing the routing context map
 * @param bookmarks optional bookmarks list
 * @param database optional database name, defaulted to "neo4j"
 * @param extra optional extra map for v4.3+
 *
 * @return the encoded ROUTE message
 */
BoltMessage NeoConnection::Routev43(const BoltValue& routing,
    const BoltValue& bookmarks,
    const std::string& database,
    const BoltValue& extra)
{
    BoltMessage route(
        BoltValue(BOLT_ROUTE, {
            routing,
            bookmarks,
            database,
            extra
            })
    );

    return route;
} // end Routev43


/**
 * @brief encodes a ROUTE message for v4.2 servers
 *
 * @param routing the routing context map
 * @param bookmarks optional bookmarks list
 * @param database optional database name, defaulted to "neo4j"
 *
 * @return the encoded ROUTE message
 */
BoltMessage NeoConnection::Routev42(const BoltValue& routing,
    const BoltValue& bookmarks,
    const std::string& database)
{
    BoltMessage route(
        BoltValue(BOLT_ROUTE, {
            routing,
            bookmarks,
            database
            })
    );

    return route;
} // end Routev42


/**
 * @brief encodes a ROUTE message for legacy servers (v4.1 and below)
 *
 * @param routing the routing context map
 *
 * @return the encoded ROUTE message
 */
BoltMessage NeoConnection::Route_Legacy(const BoltValue& routing)
{
    BoltMessage route;
    return route;
} // end Route_Legacy


/**
 * @brief an encoder could fail only if it runs out of memory to encode with, before
 *  encoding it checks the size of newdata or bolt message could fit into the
 *  buffer remaining space, if not this routines gets called flushes the remainig
 *  bytes inorder to create space for the last failed to be encoded data.
 * 
 * @param dat a reference to BoltMessage structure
 * 
 * @return LBStatus code with LB_OK as success
 */
LBStatus NeoConnection::Retry_Encode(BoltMessage& dat)
{
    LBStatus rc = Flush();
    if (!LB_OK(rc)) return rc;

    // encode it back
    rc = encoder.Encode(dat);
    if (!LB_OK(rc))
    {
        return LB_Make(
            LBAction::LB_FAIL, 
            LBDomain::LB_DOM_STATE,
            LBStage::LB_STAGE_NONE, 
            LBCode::LB_CODE_ENCODER
        );
    } // end if still no encode

    return LB_Make();
} // end Retry_Encode