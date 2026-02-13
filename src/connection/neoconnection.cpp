/**
 * @brief implementation detials for NeoConnection neo4j bolt based driver
 *
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 18th of January 2026, Sunday
 */


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/neoconnection.h"




//===============================================================================|
/**
 * @brief helper function to grow buffers when needed by doubling their capacity
 *
 * @param buf reference to buffer to grow
 *
 * @return true on success, false on failure
 */
static bool Grow_Buffers(BoltBuf& buf)
{
    size_t new_capacity = buf.Capacity() << 1;
    if (buf.Grow(new_capacity) < 0)
        return false;

    return true;
} // end Grow_Buffers


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
    bytes_recvd = 0;
    is_open = false;

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
 * @brief a wrapper to Reconnect function and sets the client id from the 
 *  parameter passed on successful connection.
 * 
 * @param cli_id optional id to set for this connection
 *
 * @return 0k on success. LB_Status codes on failure
 */
LBStatus NeoConnection::Init(const int cli_id)
{
    LBStatus rc = Reconnect();
    if (LB_OK(rc)) Set_ClientID(cli_id);

    return rc;
} // end Init


/**
 * @brief starts a connection to a server at address provided by string members
 *  or attributes if you like, hostname & port. It then negotiates the version
 *  and sends bolt HELLO messages based on the version negotiated. On successful
 *  connection it setts the boolean attribute is_open to true to flag open connection.
 *
 * @return LB_Status codes
 */
LBStatus NeoConnection::Reconnect()
{
    // reset previous query states
    tasks.Clear();

    LBStatus rc = Connect();
    if (!LB_OK(rc))
        return rc;

    rc = Negotiate_Version();
    if (LB_OK(rc))
    {
        // play by non-blocking rules from henceforth
        Enable_NonBlock();
        Enable_Keepalive();

        // push a state into the queue
        tasks.Enqueue({ QueryState::Connection });
        if (supported_version.major >= 5)       // use version 6/5 hello
			rc = Send_Hellov5();
        else if (supported_version.major >= 1)  // version 4 and below 
            rc = Send_Hellov4();
        else
        {
            return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT,
                LBCode::LB_CODE_VERSION);
        } // end else

        is_open = true;
    } // end if Negotiate ok

    return rc;
} // end Reconnect


/**
 * @brief runs a cypher query against the connected neo4j database. The driver
 *  must be in ready state to accept queries. The function appends a PULL/ALL
 *  message after the RUN message to begin fetching results.
 *
 * @param cypher the cypher query string
 * @param params optional parameters for the cypher query
 * @param extras optional extra parameters for the cypher query (see bolt specs)
 * @param n optional the numbe r of chunks to request, i.e. 1000 records
 *
 * @return 0 on success and -2 on application error
 */
int NeoConnection::Run(const char* cypher, BoltValue params, BoltValue extras,
    const int n, std::function<void(BoltResult&)> rscb)
{
    // update the state and number of queries piped, also set the poll flag
    //  to signal recv to wait on block for incoming data
    DecoderTask ts;
    ts.state = QueryState::Run;
    ts.cb = rscb;

    tasks.Enqueue(std::move(ts));

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

    encoder.Encode(run);
    Encode_Pull(n);
    if (!Flush())
    {
        tasks.Dequeue();   // remove the run state
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Run_Query


/**
 * @brief fetches the next record from the server after a RUN/PULL command.
 *  The function decodes into a BoltMessage object that can be inspected
 *  for record data or summary meta info.
 *
 * @param out the output message object to decode into
 *
 * @return 0 on completion, 1 if more records to come, -1 on sys error,
 *  -2 on app error
 */
int NeoConnection::Fetch(BoltMessage& out)
{
    //ConnectionState state = connection.Get_State();
    //if (state == ConnectionState::Run || state == ConnectionState::Pull)
    //{
    //    // expecting success/fail for run message
    //    connection.has_more = true;
    //    if (int ret; (ret = connection.Poll_Readable()) < 0)
    //    {
    //        // check error satate
    //        state = connection.Get_State();
    //        if (state == ConnectionState::Error)
    //        {
    //            // its either a run or pull error, attempt to reset
    //            //  to clean this connection
    //            connection.has_more = true;     // poll for reset
    //            return Reset();     // return reset status, terminate fetch on success
    //        } // end if error state

    //        return ret;
    //    } // end if poll error

    //    state = connection.Get_State();
    //} // end if ready

    //if (state == ConnectionState::Streaming)
    //{
    //    // we're streaming bytes
    //    int bytes = connection.decoder.Decode(connection.view.cursor, out);

    //    // advance the cursor by the bytes
    //    connection.view.cursor += bytes;
    //    connection.view.offset += (size_t)bytes;

    //    // test if we completed?
    //    if (out.msg.struct_val.tag == BOLT_SUCCESS)
    //    {
    //        if (!connection.Is_Record_Done(out))
    //            return 1;   // more records to come

    //        connection.view.summary_meta = out.msg;
    //        connection.read_buf.Reset();    // reset the read buffer for next rounds
    //        return 0;
    //    } // end if

    //    // we're not done yet
    //    if (connection.view.offset >= connection.view.size)
    //        connection.Set_State(ConnectionState::Pull);
    //} // end else if streaming

    //return 1;
    return 0;
} // end Fetch


/**
 * @brief Begins a transaction with the database, this is a manual transaction
 *  that requires commit or rollback to finish.
 *
 * @param options optional parameters for the transaction
 *
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoConnection::Begin(const BoltValue& options)
{
    if (tran_count++ != 0)
        return 0;       // already has some

    // keep track of pool, from here on we allocate from it
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage begin(
        BoltValue(BOLT_BEGIN, {
            options
            })
    );

    if (!Encode_And_Flush(QueryState::Begin, begin))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Begin_Transaction


/**
 * @brief Commits the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 *
 * @param options optional parameters for the commit
 *
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoConnection::Commit(const BoltValue& options)
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

    if (!Encode_And_Flush(QueryState::Commit, commit))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Commit_Transaction


/**
 * @brief Rolls back the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 *
 * @param options optional parameters for the rollback
 *
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoConnection::Rollback(const BoltValue& options)
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

    if (!Encode_And_Flush(QueryState::Rollback, rollback))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
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

    if (!Encode_And_Flush(QueryState::Discard, discard))
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

    if (!Encode_And_Flush(QueryState::Telemetry, tel))
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
int NeoConnection::Reset()
{
    // memorize the last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage reset(BoltValue(BOLT_RESET, {}));

    if (!Encode_And_Flush(QueryState::Reset, reset))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Reset


/**
 * @brief sends a LOGOFF message to server to gracefully logoff from the
 *  current active connection. This is a v5.1+ feature.
 *
 * @return 0 on success always
 */
int NeoConnection::Logoff()
{
    // get the last offset in the pool to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    BoltMessage off(BoltValue(BOLT_LOGOFF, {}));

    if (!Encode_And_Flush(QueryState::Logoff, off))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush

    Release_Pool<BoltValue>(offset);
    return 0;
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

    if (!Encode_And_Flush(QueryState::Connection, gb))
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

    if (!Encode_And_Flush(QueryState::Ack_Failure, ack))
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

    if (!Encode_And_Flush(QueryState::Route, route))
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
 * @brief sets the client id from the parameter provided
 *
 * @param cli_id the new client id to set
 */
void NeoConnection::Set_ClientID(const int cli_id)
{
    client_id = cli_id;
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


/**
 * @brief sets the callbacks passed from Cell object.
 *
 * @param encb pointer to encoder function called after encode
 */
void NeoConnection::Set_Callbacks(std::function<void(BoltResult&)> rscb)
{
    auto qs = tasks.Front();
    if (qs.has_value())
    {
        qs->get().cb = rscb;
    } // end Set_Callbacks
} // end Set_Callbacks


/**
 * @brief returns a stringified version of the current driver state.
 */
std::string NeoConnection::State_ToString() const
{
    static std::string states[QUERY_STATES]{
        "Connection", "Logon","Logoff", "Run", "Pull", "Streaming",
        "Discard", "Begin", "Commit", "Rollback", "Route",
        "Reset", "Ack_Failure", "Error"
    };

    auto qs = const_cast<LockFreeQueue<DecoderTask>&>(tasks).Front();
    if (!qs.has_value())
        return "Unknown";

    u8 s = static_cast<u8>(qs->get().state);
    return states[s % QUERY_STATES];
} // end State_ToString



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
 * @brief waits on recieve /recv system call on non-blocking mode. It stops recieving
 *  when a compelete bolt packet is recieved or the consuming routine has deemed
 *  it necessary to stop fetching by setting has_more to false. However, once done
 *  it must be reset back for the loop to continue. Should the buffer lack space
 *  to recv chunks it grows to accomoditate more. Once it completes a full message
 *  or chunks it calls the callback to notify the caller.
 *
 * Optionally it can be controlled to shrink back inside of a pool based on some
 *  statistically collected traffic data.
 *
 * @param cell pointer to neocell class used to invoke the method
 * @param fn_decoder pointer to a decoder function/callback
 * @param has_more a reference to sentinel that controls the loop
 *
 * @return a true on success
 */
LBStatus NeoConnection::Poll_Readable()
{
    LBStatus rc;        // store's return value

    // do we need to grow for space
    if (read_buf.Writable_Size() < 256)
    {
        if ((read_buf.Grow(read_buf.Capacity() << 1)) < 0)
        {
            //err_string = "failed to grow read buffer";
            return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_MEMORY);
        } // end if
    } // end if

    rc = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
    if (!LB_OK(rc))
        return rc;

    // if data is completely received push to response handler
    int bytes = LB_Aux(rc);
    bytes_recvd += bytes;

    if (Recv_Completed())
    {
        rc = Decode_Response(read_buf.Read_Ptr(), bytes_recvd);
        if (!LB_OK(rc))
        {
            bytes_recvd = 0;
            return rc;
        } // end if

        read_buf.Consume(bytes_recvd);
        bytes_recvd = 0;
    } // end if complete

    read_buf.Advance(bytes);
    return LB_Make();
} // end Poll_Readable


/**
 * @brief persuming that data has been fully received from Poll_Readable(), the
 *  function decodes the bolt encoded responses (message).
 */
LBStatus NeoConnection::Decode_Response(u8* view, const size_t bytes)
{
    /*Utils::Dump_Hex((const char*)view, bytes);*/
    size_t decoded = 0; // tacks decoded bytes thus far
    LBStatus rc = 0;    // holds return values

    while (decoded < bytes)
    {
        if ((0xB0 & view[2]) != 0xB0)
        {
            //err_string = "Invalid Bolt Message Format.";
            return -2;      // protocol error, a hard one
        } // end if not valid

        auto task = tasks.Front();
        if (!task.has_value())
        {
            //err_string = "No query in the queue to handle response.";
            return -3;  // soft error
        } // end if no query

        task->get().view.cursor = view;
        task->get().view.size = bytes - decoded;

        u8 s = static_cast<u8>(task->get().state);
        u8 tag = view[3];

        switch (tag)
        {
        case BOLT_SUCCESS:
            rc = (this->*success_handler[s])(task->get());
            break;

        case BOLT_FAILURE:
            rc = Handle_Failure(task->get());
            break;

        case BOLT_RECORD:
            rc = Handle_Record(task->get());
            break;

        case BOLT_IGNORED:
            rc = Handle_Ignored(task->get());
            break;

        default:
            //err_string = "protocol error, unknown tag: %d" + std::to_string(tag);
            return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT);    // hard error
        } // end switch

        int skip = LB_Aux(rc);
        view += skip;
        decoded += skip;
    } // end while

    return rc;
} // end Decode_Response


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
 * @brief determines if a compelete bolt packet has been receved. The requirement
 *  of more data is left to the calling routine. The method only checks for
 *  a complete bolt packet;
 *  i.e. header_size + 2 + 2 (should trailing 0 exist) == bytes_recvd
 *
 * @return a true if a complete bolt packet is recieved
 */
bool NeoConnection::Recv_Completed()
{
    u8* ptr = read_buf.Read_Ptr();
    size_t bytes_seen = 0;
    int msgs = 0;
    if (bytes_recvd <= 4)
        return false;

    while (bytes_seen < bytes_recvd)
    {
        u16 temp;
        memcpy(&temp, ptr, sizeof(u16));
        u16 msg_len = ntohs(temp) + 2;

        ptr += msg_len;
        bytes_seen += msg_len;
        ++msgs;
        if (reinterpret_cast<u16*>(ptr)[0] == 0)
        {
            bytes_seen += 2;
            ptr += 2;
        } // end if normal ending record
    } // end while

    auto qs = tasks.Front();
    if (!qs.has_value())
    {
        //err_string = "No query in the queue to handle response.";
        return -3;  // soft error
    } // end if no query

    // test if we equal bytes recvd
    qs->get().result.messages += msgs;
    if (bytes_seen == bytes_recvd) return true; // complete packet received
    else return false;                          // more data needed
} // end Recv_Completed


/**
 * @brief determines if the record streaming is done based on the presence of
 *  "has_more" key in the summary message map.
 *
 * @param cursor pointer to the current position in the buffer
 * @param v reference to boltvalue to decode into
 *
 * @return true if record streaming is done else false
 */
bool NeoConnection::Is_Record_Done(DecoderTask& t)
{
    if (t.result.summary.msg(0).type == BoltType::Map &&
        t.result.summary.msg(0)["has_more"].type != BoltType::Unk &&
        t.result.summary.msg(0)["has_more"].bool_val == true)
    {
        t.state = QueryState::Streaming;    // still streaming
    } // end if
    else return true;

    return false;   // not done
} // end Is_Record_Done


/**
 * @brief encodes the boltvalue reference and flushes it to peer after it saved
 *  its state into the query_states queue for later use during response decoding.
 *
 * @param state the query state to save
 * @param msg the BoltValue to encode and send
 *
 * @return true on success alas false on sys error
 */
bool NeoConnection::Encode_And_Flush(QueryState state, BoltMessage& msg)
{
    tasks.Enqueue({ state });
    encoder.Encode(msg);
    if (!Flush())
    {
        tasks.Dequeue();   // remove the new state
        return false;
    } // end if no flush

    return true;
} // end Encode_And_Flush


/**
 * @brief performs version negotiation as specified by the bolt protocol. It uses
 *  v5.7+ manifest negotiation style to allow the server respond with version
 *  numbers supported. If server does not support manifest, it simply starts
 *  with version 4 or less.
 *
 * @return 0K on success alas status errors
 */
LBStatus NeoConnection::Negotiate_Version()
{
    LBStatus rc;      // return status codes

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
    if (!LB_OK(rc))
        return rc;
    
    rc = Recv((char*)versions, buf_len);
    if (!LB_OK(rc))
        return rc;

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
 * @brief connects to neo4j server using its latesest (as of this writing) v5.x HELLO
 *  handshake message. The message/payload consists mainly creds and other extra
 *  stuff user could add that is based on the bolt protocol v5.x spec. The v5.x
 *  spec consists of two steps, a basic hello and a logon message after version
 *  negotiation, thus LightningBolt implements both steps here as states of
 *  the driver.
 *
 * @return LB_Status codes
 */
LBStatus NeoConnection::Send_Hellov5()
{
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
        {"user_agent", (pextras && pextras[0]["user_agent"].type != BoltType::Unk) ?
            pextras[0]["user_agent"] :
            BoltValue(("LB/v" + std::to_string(client_id + 1) + ".0").c_str()), 1.0, 100},

        {"patch_bolt", (pextras ? pextras[0]["patch_bolt"] : BoltValue::Make_Unknown()), 4.3, 4.4},
        {"routing", (pextras ? pextras[0]["routing"] : BoltValue::Make_Unknown()), 4.1, 100},
        {"notifications_minimum_severity", (pextras ? pextras[0]["notifications_minimum_severity"]
            : BoltValue::Make_Unknown()), 5.2, 100},
        {"notifications_disabled_categories",
            (pextras ? pextras[0]["notifications_disabled_categories"] : BoltValue::Make_Unknown()), 5.2, 5.4}
    };

    LBStatus rc = 0;
    auto task = tasks.Front();
    if (task.has_value())
    {
        if (task->get().state == QueryState::Connection)
        {
            // persume version 5.3+ is supported; i.e. graft versions 5.x as 
            //  same versions with minor changes

            // update state to logon
            task->get().state = QueryState::Logon;

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
                bmp.Insert_Map("bolt_agent", BoltValue({
                        mp("product", "LightningBolt/v1.0.0"),
                        mp("platform", "Linux 6.6.87.2/microsoft-standard-WSL2; x64"),
                        mp("language", "C++/17"),
                    }));
            } // end sversion

            hello.msg.Insert_Struct(bmp);
        } // end if connecting
        else if (task->get().state == QueryState::Logon)
        {
            task->get().state = QueryState::Connection;
            hello = (BoltValue(BOLT_LOGON, { pauth[0] }));
        } // end else

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
    } // end if has value
    
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
 * @return LB_Status codes, OK on success else error code
 */
LBStatus NeoConnection::Send_Hellov4()
{
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();
    std::string uagent{ ("LB/" + std::to_string(client_id + 1) + ".0").c_str() };

    BoltValue map = pauth[0];
    map.Insert_Map("user_agent", BoltValue(uagent.c_str()));
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
 * @brief First message received after authentication with Neo4j graphdb server.
 *  We expect two major modes of connections v4.x and v5.x (latest). In v5.x we deal
 *  with this function twice HELLO + LOGON. Once successfuly authenticated it
 *  sets the driver state to ready.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Success_Hello(DecoderTask& task)
{
    u32 size = static_cast<u32>(task.view.size);
    if (task.state == QueryState::Logon)
    {
        Send_Hellov5();     // log on message
        return LB_Make(LBAction::LB_HASMORE, 
            LBDomain::LB_DOM_BOLT, LBCode::LB_CODE_NONE, size);     // poll once more
    } // end if

    Wake();
    read_buf.Reset();       // safe to reset we don't need the info
    return LB_OK_INFO(size);
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
    task.state = QueryState::Pull;

    // parse and save the field names; until its needed and guranteed to exist
    //  as long as we are streaming the result. 
    // on success returns LB_OK_INFO with aux # of bytes to skip buffer
    return decoder.Decode(task.view.cursor, task.result.fields);
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
    LBStatus rc = decoder.Decode(task.view.cursor, task.result.summary);
    if (!LB_OK(rc))
        return rc;

    if (!Is_Record_Done(task))
        return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
            LBCode::LB_CODE_NONE, LB_Aux(rc));

    if (task.cb) task.cb(task.result);

    return rc; // should be LB_OK_INFO
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
    Wake();     // anything pending

    tasks.Dequeue();
    read_buf.Reset();

    return LB_OK_INFO(task.view.size);
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
    task.state = QueryState::Streaming;

    //Dump_Hex((const char*)task.view.cursor, task.view.size);

    LBStatus rc = decoder.Decode(task.view.cursor, task.result.record);
    if (!LB_OK(rc))
        return rc;

    if (task.cb)
    {
        task.cb(task.result);
    }
    //task.result.records.push_back(record.msg(0));
    //task.result.client_id = client_id;

    return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
        LBCode::LB_CODE_NONE, LB_Aux(rc));
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
    LBCode code;

    LBStatus rc = decoder.Decode(task.view.cursor, task.result.error);
    if (!LB_OK(rc))
    {
        Wake(); // anyone on wait
        return rc;
    } // end if failed decoding

    //err_string = err.ToString();
    //std::cout << err.ToString() << std::endl;

    QueryState qs = task.state;
    switch (qs)
    {
    case QueryState::Connection:
    case QueryState::Logon:
    case QueryState::Logoff:
        action = LBAction::LB_FAIL;
        code = LBCode::LB_CODE_NEO4J_CONNECT;
        break;

    case QueryState::Run:
    case QueryState::Pull:
    case QueryState::Streaming:
        action = LBAction::LB_RESET;
        code = LBCode::LB_CODE_NEO4J_QUERY;
        break;

    }; // end switch

    Wake();  // those who wait
    return rc;
} // end Handle_Failure


/**
 * @brief handles the IGNORED message from server; it simply terminates the
 *  connection as it indicates a serious state out of sync.
 *
 * @param task the next task on the queue to process
 *
 * @return LBStatus codes with LB_OK_INFO containing number of bytes to skip
 */
inline LBStatus NeoConnection::Handle_Ignored(DecoderTask& task)
{
    return LB_OK_INFO(task.view.size);
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
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_STATE,
            LBCode::LB_CODE_ENCODER);
    } // end if still no encode

    return LB_Make();
} // end Retry_Encode