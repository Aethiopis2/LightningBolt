/**
 * @file addicion.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief implementation detials for NeoConnection neo4j bolt based driver
 * 
 * @version 1.0
 * @date 14th of April 2025, Monday.
 * 
 * @copyright Copyright (c) 2025
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <emmintrin.h>
#include "connection/neoconnection.h"
#include "connection/central_dispatcher.h"
#include "bolt/bolt_response.h"
#include "utils/utils.h"
#include "utils/errors.h"



//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief constructor for NeoConnection
 * 
 * @param con_string the connection string to connect to neo4j server
 * @param cli_id the client id of this connection
 * @param extras extra parameters for connection
 */
NeoConnection::NeoConnection(BoltValue connection_params, const u64 cli_id)
    :encoder(write_buf), decoder(read_buf), conn_params(connection_params),
     client_id(cli_id), state(BoltState::Disconnected)
{
	// split host and port from the host param
	std::vector<std::string> temp{ Split_String(
        connection_params["host"].ToString(), ':') };

	hostname = temp[0];
	port = (temp.size() > 1) ? temp[1] : "7687";
 
    current_qid = 0;
	num_queries = 0;
    transaction_count = 0;
    bytes_to_decode = 0;
    has_more = true;
    err_string = "";
} // end NeoConnection

//===============================================================================|
/**
 * @brief house cleanup via Stop()
 */
NeoConnection::~NeoConnection()
{
    Stop();
} // end NeoConnection

//===============================================================================|
/**
 * @brief checks if the connection is closed
 * 
 * @return true if closed, false otherwise
 */
bool NeoConnection::Is_Closed() const
{
    return state == BoltState::Disconnected;
} // end Is_Closed

//===============================================================================|
/**
 * @brief start's a connection to a neo4j database server using bolt protocol. 
 *  Function begins by TCP connecting with server and then negotiates a supported
 *  version followed by hello request that depends on the versions supported. 
 * 
 *  LightingingBolt driver supports versions 6.0, 5.x, 4.x, 3.x (for the most part)
 *  of the bolt protocol.
 * 
 * @return 0 on success. -1 on sys error while -2 for app specific errors
 */
int NeoConnection::Start()
{
    if (Connect() < 0)
        return -1;

    Set_State(BoltState::Connecting);
    if (int ret; (ret = Negotiate_Version()) >= 0)
    {
        if (ret == 6 || ret == 5)   // use version 6/5 hello
            ret = Send_Hellov5();
        else if (ret >= 1)          // version 4 and below 
            ret = Send_Hellov4();
        else
        {
            err_string = "unsupported negotiated version";
            Close_Driver();
            return -2;
        } // end else
    } // end if Negoitate

    // wait on response 
    return Poll_Readable();
} // end Start

//===============================================================================|
/**
 * @brief terminates the active connection does house cleaning.
 */
void NeoConnection::Stop()
{
    if (Get_State() != BoltState::Disconnected)
    {
        // would be ignored if not supported anyways
        Logoff();
        Goodbye();
    } // end if not connecting

    Close_Driver();
} // end Stop

//===============================================================================|
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
int NeoConnection::Run_Query(const char* cypher, BoltValue params, BoltValue extras,
    const int n)
{
    // its ok to pipline calls too
    if (state != BoltState::Ready && state != BoltState::Run)
    {
        err_string = "invalid state: " + State_ToString();
        return -2;  // app error
    } // end if

    // update the state and number of queries piped, also set the poll flag
    //  to signal recv to wait on block for incoming data
    Set_State(BoltState::Run);
    num_queries++;
    has_more = true;

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
    Flush();

    return 0;
} // end Run_Query

//===============================================================================|
int NeoConnection::Fetch(BoltMessage& out)
{
    if (state == BoltState::Run || state == BoltState::Pull)
    {
        // expecting success/fail for run message
        has_more = true;
        if (int ret; (ret = Poll_Readable()) < 0)
            return ret;
    } // end if ready

    if (state == BoltState::Streaming)
    {
        // we're streaming bytes
        int bytes = decoder.Decode(view.cursor, out);

        // advance the cursor by the bytes
        view.cursor += bytes;
        view.offset += (size_t)bytes;

        // test if we completed?
        if (out.msg.struct_val.tag == BOLT_SUCCESS)
        {
            view.summary_meta = out.msg;
            Set_State(BoltState::Ready);
            read_buf.Reset();
            return 0;
        } // end if

        if (view.offset >= view.size)
            Set_State(BoltState::Pull);
    } // end else if streaming

    return 1;
} // end Fetch

//===============================================================================|
/**
 * @brief Begins a transaction with the database, this is a manual transaction
 *  that requires commit or rollback to finish.
 * 
 * @param options optional parameters for the transaction
 * 
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoConnection::Begin_Transaction(const BoltValue& options)
{
    if (transaction_count++ > 0)
        return 0;       // already has some

    BoltMessage begin(
        BoltValue(BOLT_BEGIN, {
            options
        })
    );
    encoder.Encode(begin);
    Flush();

    // wait for success
    Poll_Readable();
    return 0;
} // end Begin_Transaction


//===============================================================================|
/**
 * @brief Commits the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 * 
 * @param options optional parameters for the commit
 * 
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoConnection::Commit_Transaction(const BoltValue& options)
{
    if (transaction_count-- > 0)
        return 0;       // got a few more

    BoltMessage commit(
        BoltValue(BOLT_COMMIT, {
            options
        })
    );
    encoder.Encode(commit);
    Flush();   

    // wait for success
    Poll_Readable();
    return 0;
} // end Commit_Transaction


//===============================================================================|
/**
 * @brief Rolls back the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 * 
 * @param options optional parameters for the rollback
 * 
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoConnection::Rollback_Transaction(const BoltValue& options)
{
    if (transaction_count-- > 0)
        return 0;       // not yet

    BoltMessage rollback(
        BoltValue(BOLT_ROLLBACK, {
            options
        })
    );
    encoder.Encode(rollback);
    Flush();

    // wait for success
    Poll_Readable();
    return 0;
} // end Rollback_Transaction

//===============================================================================|
/*@brief encodes a PULL message and sends it. Useful during reactive style fetch
*
* @param n the number of chunks to to fetch at once, defaulted to - 1 to fetch
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

//===============================================================================|
int NeoConnection::Reset()
{
    if (state != BoltState::Error || state != BoltState::Ready)
        return 0;

    BoltValue bv(BOLT_RESET, {});
    if (!Flush_And_Poll(bv))
        return -1;

    return 0;
} // end Reset

//===============================================================================|
int NeoConnection::Discard(const int n)
{
    if (state != BoltState::Streaming)
        return 0;       // ignore

    BoltValue bv(BOLT_DISCARD, {
        BoltValue({
            mp("n", n),
            mp("qid", (int)client_id)
            })
        });

    if (!Flush_And_Poll(bv))
        return -1;

    return 0;
} // end Discard

//===============================================================================|
int NeoConnection::Telemetry(const int api)
{
    if (state != BoltState::Ready)
        return 0;       // ignore

    BoltValue bv(BOLT_TELEMETRY, { api });
    if (!Flush_And_Poll(bv))
        return -1;

    return 0;
} // end Telemetry

//===============================================================================|
int NeoConnection::Logoff()
{
    if (state == BoltState::Disconnected)
        return 0;       // ignore

    Set_State(BoltState::Connecting);
    BoltMessage off(BoltValue(BOLT_LOGOFF, {}));
    encoder.Encode(off);

    if (!Flush())
        return -1;

    Close_Driver();
    return 0;
} // end Logoff

//===============================================================================|
int NeoConnection::Goodbye()
{
    if (state == BoltState::Disconnected)
        return 0;       // already done

    BoltValue gb(BoltValue(BOLT_GOODBYE, {}));
    encoder.Encode(gb);

    if (!Flush())
        return -1;

    Close_Driver();
    Set_State(BoltState::Disconnected);
    return 0;
} // end Goodbye

//===============================================================================|
int NeoConnection::Ack_Failure()
{
    if (state != BoltState::Error)
        return 0;       // already done

    BoltValue ack(BoltValue(BOLT_ACK_FAILURE, {}));
    if (!Flush_And_Poll(ack))
        return -1;

    return 0;
} // end Failure

//===============================================================================|
/**
 * @brief set's the state of driver externally.
 * 
 * @param s the new state to set
 */
void NeoConnection::Set_State(BoltState s)
{
    state = s;
} // end Set_State

//===============================================================================|
/**
 * @brief return's the current state
 */
BoltState NeoConnection::Get_State() const
{
    return state;
} // end Get_State

//===============================================================================|
/**
 * @brief return's the client id for this driver
 */
u64 NeoConnection::Get_Client_ID() const
{
    return client_id;
} // end client_id

//===============================================================================|
/**
 * @brief return's the last error as string encounterd in this driver
 */
std::string NeoConnection::Get_Last_Error() const
{
    return err_string;
} // end Dump_Error

//===============================================================================|
/**
 * @brief returns a stringified version of the current driver state.
 */
std::string NeoConnection::State_ToString() const
{
    static std::string states[DRIVER_STATES]{
        "Disconnected", "Connecting","LOGON", "Ready", 
        "Run", "Pull", "Streaming", "Error"
    };

    u8 s = static_cast<u8>(Get_State());
    return states[s];
} // end State_ToString



//===============================================================================|
/**
 * @brief kills the active connection if not already closed; reset's buffers and
 *  turns driver state to disconnected.
 */
void NeoConnection::Close_Driver()
{
    Set_State(BoltState::Disconnected);
    if (!Is_Closed())
        CLOSE(fd);

    write_buf.Reset();
    read_buf.Reset();
} // end Close_Driver

//===============================================================================|
/**
 * @brief sends the contents of write buffer to the connected peer.
 *
 * @return a true on success alas false
 */
bool NeoConnection::Poll_Writable()
{
    while (!write_buf.Empty())
    {
        ssize_t n = Send(write_buf.Read_Ptr(), write_buf.Size());
        if (n <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return false;

            Stop();
            return false;
        } // end if

        write_buf.Consume(n);
    } // end while

    return true;
} // end Poll_Writable

//===============================================================================|
/**
 * @brief waits on recieve /recv system call on blocking mode. It stops recieving
 *  when a compelete bolt packet is recieved or the consuming routine has deemed
 *  it necessary to stop fetching by setting has_more to false. However, once done
 *  it must be reset back for the loop to continue. Should the buffer lack space
 *  to recv chunks it grows to accomodiate more.
 * Optionally it can be controlled to shrink back inside of a pool based on some
 *  statistically collected traffic data.
 */
int NeoConnection::Poll_Readable()
{
    // do we need to grow for space
    if (read_buf.Writable_Size() < 256)
        read_buf.Grow(read_buf.Capacity() * 2);

    while (has_more)
    {
        ssize_t n = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
        if (n <= 0)
        {
            if (n == 0 || errno == EINTR)
                continue;
            else
            {
                Close_Driver();
                return -1;
            } // end else
        } // end if

        // if data is completely received push to response handler
        bytes_to_decode += n;
        if (Recv_Completed(bytes_to_decode))
        {
            if (!Decode_Response(read_buf.Read_Ptr(), bytes_to_decode))
                return -2;

            read_buf.Consume(bytes_to_decode);
            bytes_to_decode = 0;
            //has_more = false;   // persume done
        } // end if complete

        read_buf.Advance(n);
    } // end while

    return 0;
} // end Poll_Readable

//===============================================================================|
/**
 * @brief makes sure all the contents of the write buffer has been written to the
 *  sending buffer and kernel is probably sending it.
 *
 * @return a true on success
 */
bool NeoConnection::Flush()
{
    while (!write_buf.Empty())
    {
        if (Poll_Writable())
            return false;   // has to be a syscall error always

        // Optional: safety guard
        if (write_buf.Size() > 0)
        {
            std::this_thread::yield();  // Allow other I/O threads time
        } // end if
    } // end while

    write_buf.Reset();
    return true;
} // end Flush

//===============================================================================|
/**
 * @brief persuming that data has been fully received from Poll_Readable(), the
 *  function decodes the bolt encoded responses (message).
 */
bool NeoConnection::Decode_Response(u8* view, const size_t bytes)
{
    if (bytes == 0)
        return 0;

    //Dump_Hex((const char*)view, bytes);
    size_t skip = 0;
    while (skip < bytes)
    {
        if (!(0xB0 & *(view + 2)))
        {
            err_string = "Invalid Bolt Message Format.";
            return false;
        } // end if not valid

        u8 s = static_cast<u8>(state);
        u8 tag = *(view + 3);

        switch (tag)
        {
        case BOLT_SUCCESS:
            (this->*success_handler[s])(view, bytes);
            skip = bytes;
            break;

        case BOLT_FAILURE:
            (this->*fail_handler[s])(view, bytes);
            return false;
            break;

        case BOLT_RECORD:
            Success_Pull(view, bytes);
            skip = bytes;
            break;

        default:
            Print("What is this?");
            skip = bytes;
            read_buf.Reset();
        } // end switch

        view += skip;
    } // end while

    return true;
} // end Decode_Response

//===============================================================================|
/**
 * @brief determines if a compelete bolt packet has been receved. The requirement
 *  of more data is left to the calling routine. The method only checks for
 *  a complete bolt packet;
 *  i.e. header_size + 2 + 2 (should trailing 0 exist) == bytes
 *
 * @param bytes total bytes recieved during this chunk
 *
 * @return a true if a complete bolt packet is recieved
 */
bool NeoConnection::Recv_Completed(const size_t recvd_bytes)
{
    u8* ptr = read_buf.Read_Ptr();
    size_t bytes_seen = 0;

    while (bytes_seen < recvd_bytes)
    {
        u16 msg_len = ntohs(reinterpret_cast<u16*>(ptr)[0]) + 2;    // including header word
        ptr += msg_len;
        bytes_seen += msg_len;
        if (reinterpret_cast<u16*>(ptr)[0] == 0)
        {
            bytes_seen += 2;
            ptr += 2;
        } // end if normal ending record
    } // end while

    // test if we equal bytes recvd
    if (bytes_seen == recvd_bytes) return true; // complete packet received
    else return false;                          // more data needed
} // end Recv_Completed

//===============================================================================|
/**
 * @brief performs version negotiation as specified by the bolt protocol. It uses
 *  v5.7+ manifest negotiation style to allow the server respond with version 
 *  numbers supported. If server does not support manifest, it simply starts 
 *  with version 4 or less.
 *
 * @return supported version on success or 0 on fail
 */
int NeoConnection::Negotiate_Version()
{
    u8 version[64]{
        0x60, 0x60, 0xB0, 0x17,         // neo4j magic number
        0x00, 0x00, 0x01, 0xFF,         // manifest v1
        0x00, 0x00, 0x04, 0x04,         // if not try version 4
        0x00, 0x00, 0x00, 0x03,         // version 3 and ...
        0x00, 0x00, 0x00, 0x02          // version 2 (last two are not supported)
    };

    if (Send(version, 20) < 0)
    {
        Close_Driver();
        return -1;
    } // end if bad sending

    if ((Recv((char*)version, sizeof(version))) < 0)
    {
        Close_Driver();
        return -1;
    } // end if bad reception


    u32 supported = 0;      // get's the maximum supported
    const u8 nums = *reinterpret_cast<u8*>(version + 4);  
        // the byte next to manifest is number of supported versions by server.
         
    // pick the higest version
    u32* iter = reinterpret_cast<u32*>(version + 5);
    u32* alias = iter;
    for (int i = 0; i < nums; i++)
    {
        u32 v = htonl(*iter);
        if (supported < v)
        {
            supported = v & 0x0F; // only interseted in the last nybble
            alias = iter;
        } // end if supported
        ++iter;
    } // end for
    
    // send to server; and echo back whatever the server caps are
    if (Send(alias, 5) < 0)
        return -1;

    return supported;
} // end Negotiate_Version

//===============================================================================|
/**
 * @brief connects to neo4j server using its latesest (as of this writing) v5.x HELLO
 *  handshake message. The message/payload consists mainly creds and other extra
 *  stuff user could add that is based on the bolt protocol v5.x spec. The v5.x
 *  spec consists of two steps, a basic hello and a logon message after version
 *  negotiation, thus LightningBolt implements both steps here as states of
 *  the driver.
 *  
 * @return 0 on success
 */
int NeoConnection::Send_Hellov5()
{
    BoltMessage hello;

    // extract out any extra parameters from conn_params, supplied from the
    //  user during connection string
    struct Param_Helper
    {
        std::string key;
        BoltValue val;
    };
    std::vector<Param_Helper> params{
        {"user_agent", conn_params["user_agent"].type != BoltType::Unk ?
            conn_params["user_agent"] :
            BoltValue(("LB/v" + std::to_string(client_id + 1) + ".0").c_str())},

        {"patch_bolt", conn_params["patch_bolt"]},
        {"routing", conn_params["routing"]},
        {"notifications_minimum_severity", conn_params["notifications_minimum_severity"]},
        {"notifications_disabled_categories", conn_params["notifications_disabled_categories"]},
        {"notifications_disabled_classes", conn_params["notifications_disabled_classes"]}
    };

    if (state == BoltState::Connecting)
    {
        // persume version 5.3+ is supported; i.e. graft versions 5.x as 
        //  same versions with minor changes

        // update state to logon
        Set_State(BoltState::Logon);

        hello.msg = BoltValue::Make_Struct(BOLT_HELLO);
        BoltValue bmp = BoltValue::Make_Map();

        for (auto& param : params)
        {
            if (param.val.type != BoltType::Unk)
            {
                bmp.Insert_Map(param.key.c_str(), param.val);
            } // end if not unknown
        } // end for params

        // add bolt agent info
        bmp.Insert_Map("bolt_agent", BoltValue({
                mp("product", "LightningBolt/v1.0.0"),
                mp("platform", "Linux 6.6.87.2/microsoft-standard-WSL2; x64"),
                mp("language", "C++/17"),
            }, false));

        hello.msg.Insert_Struct(bmp);
    } // end if connecting
    else if (state == BoltState::Logon)
    {
        Set_State(BoltState::Connecting);

        hello = (BoltValue(BOLT_LOGON, { {
            mp("scheme", "basic"),
            mp("principal", conn_params["username"]),
            mp("credentials", conn_params["password"])
        } }));
    } // end else

    encoder.Encode(hello);
    if (!Flush())
        return -1;

    return 0;
} // end Send_Hellov5

//===============================================================================|
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
 * @return 0 on success or -2 on fail to define app specific error
 */
int NeoConnection::Send_Hellov4()
{
    std::string uagent{ ("LB/" + std::to_string(client_id + 1) + ".0").c_str() };

    BoltMessage hello(BoltValue(BOLT_HELLO, {
        BoltValue({
            mp("user_agent", uagent.c_str()),
            mp("scheme", "basic"),
            mp("principal", conn_params["username"]),
            mp("credentials", conn_params["password"])
        })
        }));

    encoder.Encode(hello);
    Flush();
    return 0;
} // end Send_Hello

//===============================================================================|
/**
 * @brief encodes a PULL message after a RUN command to fetch all results.
 *
 * @param n the number of chunks to to fetch at once, defaulted to -1 to fetch
 *  everything.
 */
void NeoConnection::Encode_Pull(const int n)
{
    BoltMessage pull(BoltValue(
        BOLT_PULL, {
            {mp("n", n), mp("qid",-1)}
        }));

    encoder.Encode(pull);
} // end Send_Pull

//===============================================================================|
/**
 * @brief serves as a safety net when things fire outof sync so as not to cause
 *  segfaults and allow the application to continue safely after having sent a  
 *  REST signal possibly, in the worst case.
 * 
 * @param cursor the current position in the buffer to decode from
 * @param bytes the number of bytes recieved.
 */
inline void NeoConnection::Dummy(u8* view, const size_t bytes)
{
    Set_State(BoltState::Error);

    // save the current error
    err_string = "State out of sync: " + State_ToString();
} // end Success_Run


//===============================================================================|
/**
 * @brief First message received after authentication with Neo4j graphdb server.
 *  We expect to major modes of connections v4.x and v5.x (latest). In v5.x we deal
 *  with this function twice HELLO + LOGON. Once successfuly authenticated it 
 *  sets the driver state to ready.
 * 
 * @param view start of address to decode from
 * @param bytes length of the recvd data
 */
inline void NeoConnection::Success_Hello(u8* view, const size_t bytes)
{
    if (state == BoltState::Logon)
    {
        Send_Hellov5();     // log on the server
        return;
    } // end if
    
    has_more = false;
    read_buf.Reset();
    Set_State(BoltState::Ready);
} // end Success_Hello


//===============================================================================|
/**
 * @brief this handles a successful run query message; we temporariliy save the
 *  metadata returned for the next records inside the view field_names memeber.
 *  Sets the state to pull to inidcate we expect records next.
 * 
 * @param cursor the starting address for buffer
 * @param bytes the bytes recvd in the buffer
 */
inline void NeoConnection::Success_Run(u8* cursor, const size_t bytes)
{
    has_more = true; // indicates to decode loop that we have incoming records
    Set_State(BoltState::Pull);

    // parse and save the field names; until its needed and guranteed to exist
    //  as long as we are streaming the result
    decoder.Decode(cursor, view.field_names);
} // end Success_Run


//===============================================================================|
/**
 * @brief this is called immidiately after Successful Run, and it simply sets the
 *  member view to point at the next bytes and sets has_more to false to make 
 *  sure the recv loop won't run again unless required through Fecth(). It updates
 *  the state to streaming to indicate driver is now consuming buffer.
 * 
 * @param cursor the starting address for buffer
 * @param bytes the bytes recvd in the buffer
 */
inline void NeoConnection::Success_Pull(u8* cursor, const size_t bytes)
{
    Set_State(BoltState::Streaming);

    // update the cursor view
    view.cursor = cursor;
    view.size = bytes;
    view.offset = 0;
    has_more = false;       // persume done
} // end Success_Record

//===============================================================================|
/**
 * @brief handles the success summary message sent after the completion of each
 *  record streaming. If the summary message contains "has_more" key and is set 
 *  to true then it persumes not done and sets the state back to PULL for Fetch()
 *  once done it sets the state to ready.
 * 
 * @param cursor the current position in the buffer to decode from
 * @param bytes the number of bytes recieved.
 */
inline void NeoConnection::Success_Record(u8* cursor, const size_t bytes)
{
    Dump_Hex((const char*)cursor, bytes);
    Set_State(BoltState::Ready);
    int skip = decoder.Decode(cursor, view.summary_meta);

    if (view.summary_meta.msg[0].type == BoltType::Map && 
        view.summary_meta.msg[0]["has_more"].type != BoltType::Unk &&
        view.summary_meta.msg[0]["has_more"].bool_val == true)
    {
        // all done
        has_more = true;
		Set_State(BoltState::Pull);
	} // end if
    else
    {
        has_more = false;
        Set_State(BoltState::Ready);
        read_buf.Reset();
    } // end else
} // end Success_Record

//===============================================================================|
/**
 * @brief placeholder for unhandled failure messages.
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
void NeoConnection::DummyF(u8* view, const size_t bytes)
{
	// do nothing
} // end DummyF

//===============================================================================|
/**
 * @brief triggered when the first BOLT HELLO message/negotiation fails. After
 *  which connection is presumed closed and must be restarted again.
 * 
 * @param view the start of buffer message to decode (contains error string)
 * @param bytes length of the buffer above.
 */
void NeoConnection::Fail_Hello(u8* view, const size_t bytes)
{
    BoltMessage fail;

	Set_State(BoltState::Disconnected);
	decoder.Decode(view, fail);
    err_string = fail.ToString();

	has_more = false;
    Stop();
} // end Fail_Hello

//===============================================================================|
/**
 * @brief 
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
void NeoConnection::Fail_Run(u8* view, const size_t bytes)
{
	Fail_Hello(view, bytes);
} // end Fail_Run

//===============================================================================|
/**
 * @brief 
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
void NeoConnection::Fail_Pull(u8* view, const size_t bytes)
{
   
} // end Fail_Pull

//===============================================================================|
/**
 * @brief
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
void NeoConnection::Fail_Record(u8* view, const size_t bytes)
{
    BoltMessage fail;

    Set_State(BoltState::Run);
    decoder.Decode(view, fail);
    err_string = fail.ToString();

    has_more = false;
    read_buf.Reset();
	write_buf.Reset();
} // end Fail_Record

//===============================================================================|
/**
 * @brief encodes the boltvalue reference and flushes it to peer. It then blocks
 *  and waits for a response from server.
 * 
 * @param v the BoltValue to encode and send
 * 
 * @return true on success alas false on sys error
 */
inline bool NeoConnection::Flush_And_Poll(BoltValue& v)
{
    BoltMessage reset(v);
    encoder.Encode(reset);

    if (!Flush())
        return false;

    if (Poll_Readable() < 0)
        return false;

    return true;
} // end Flush_And_Poll