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
    state(BoltState::Disconnected)
{
	conn_params.disposable = true;      // mark disposable for cleanup

	// split host and port from the host param
	std::vector<std::string> temp{ Split_String(
        connection_params["host"].ToString(), ':') };

	hostname = temp[0];
	port = (temp.size() > 1) ? temp[1] : "7687";
 
	client_id = cli_id;
    current_qid = 0;
	num_queries = 0;
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
 *  Function begins by TCP connecting with server and continues to negotiate supported
 *  version followed by hello request that varies depending on the versions supported. 
 *  LightingingBolt driver supports versions 5.x, 4.x, 3.x, 2.x, 1.x (for the most part)
 *  of the bolt protocol.
 * 
 * @return 0 on success. -1 on system error while -2 for application specific errors
 */
int NeoConnection::Start()
{
    int ret;
    if ( (ret = Connect()) < 0)
        return -1;

    if ( (ret = Negotiate_Version()) < 0)
        return -1;

    Set_State(BoltState::Connecting);
    if (ret == 5)                   // use version 5 hello
        ret = Send_Hellov5();
    else if (ret >= 3)              // version 4 and below
        ret = Send_Hellov4();
    else if (ret >= 1)
		ret = Send_Hello();      // legacy v2/v1 hello
    else
    {
		err_string = "unsupported negotiated version";
		return -2;
	} // end else
    
    if ( (ret = Poll_Readable()) < 0)
        return ret;

    return 0;
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
        Set_State(BoltState::Connecting);
        BoltMessage off(BoltValue(BOLT_LOGOFF, {}));
        encoder.Encode(off);

        Flush();
        Poll_Readable();

        BoltMessage gb(BoltValue(BOLT_GOODBYE, {}));
        encoder.Encode(gb);
        Flush();
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
 * 
 * @return 0 on success and -2 on application error
 */
int NeoConnection::Run_Query(const char* cypher, BoltValue params, BoltValue extras)
{
    // its ok to pipline calls too
    if (state != BoltState::Ready && state != BoltState::Run)
    {
        err_string = "invalid state: " + State_ToString();
        return -2;  // app error
    } // end if

    BoltMessage run(
        BoltValue(BOLT_RUN, {
            cypher, 
            params,
            extras
        })
    );  

    encoder.Encode(run);
    Encode_Pull();
    Flush();

    num_queries++;
    has_more = true;
    Set_State(BoltState::Run);
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
    BoltState s = Get_State();
    if (s != BoltState::Ready)
    {
        err_string = "invalid state: " + State_ToString();
        return -2;  // app error
    } // end if

    BoltMessage begin(
        BoltValue(BOLT_BEGIN, {
            options
        })
    );
    encoder.Encode(begin);
    Flush();

    Set_State(BoltState::Trx);

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
    BoltState s = Get_State();
    if (s != BoltState::Trx)
    {
        err_string = "invalid state: " + State_ToString();
        return -2;  // app error
    } // end if

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
    BoltState s = Get_State(); 
    if (s != BoltState::Trx)
    {
        err_string = "invalid state: " + State_ToString();
        return -2;  // app error
    } // end if

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
BoltState NeoConnection::Get_State() const
{
    return state;
} // end Get_State


//===============================================================================|
void NeoConnection::Set_State(BoltState s)
{
    state = s;
} // end Set_State

//===============================================================================|
void NeoConnection::Run_Write(std::shared_ptr<BoltRequest> req)
{

} // end Run_Write


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
    int ret = 0;
	size_t write_size = read_buf.Writable_Size();
    if (write_size < 256)
    {
        // do we need to grow for space
        read_buf.Grow(read_buf.Capacity() * 2);
    } // end else if

    while (read_buf.Writable_Size() > 0 && has_more)
    {
        ssize_t n = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
        if (n <= 0)
        {
            if (n == 0)
                break;
            else if (errno == EINTR)
                continue;
            else 
            {
                Close_Driver();
                return -1;
            } // end else
        } // end if

		bytes_to_decode += n;

        // if data is completely received push to response handler
        if (Recv_Completed(bytes_to_decode))
        {
            ret = Decode_Response(read_buf.Read_Ptr(), bytes_to_decode);
            read_buf.Consume(bytes_to_decode);
			bytes_to_decode = 0;
        } // end if complete

        read_buf.Advance(n);
    } // end while

    return ret;
} // end Poll_Readable

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
 * @brief persuming that data has been fully received from Poll_Readable(), the 
 *  function decodes the bolt encoded responses (message).
 */
int NeoConnection::Decode_Response(u8* view, const size_t bytes)
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
            return -2;
		} // end if not valid

        u8 s = static_cast<u8>(state);
        u8 tag = *(view + 3); 
  
        switch (tag)
        {
        case BOLT_SUCCESS:
            skip = (this->*success_handler[s])(view, bytes);
            break;

        case BOLT_FAILURE: 
			(this->*fail_handler[s])(view, bytes);
			return -2;
            break;

        case BOLT_RECORD:
            skip = Success_Pull(view, bytes);
            break;

        default:
            Print("What is this?");
            skip = bytes;
            read_buf.Reset();
        } // end switch

        view += skip;
    } // end while

    return 0;
} // end Decode_Response


//===============================================================================|
void NeoConnection::Poll_Writable()
{
    while (!write_buf.Empty())
    {
        ssize_t n = Send(write_buf.Read_Ptr(), write_buf.Size());
        if (n <= 0) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) 
                return;

            Stop();
            return;
        } // end if

        write_buf.Consume(n);
    } // end while

} // end Poll_Writable


//===============================================================================|
void NeoConnection::Flush()
{
    while (!write_buf.Empty())
    {
        Poll_Writable();

        // Optional: safety guard
        if (write_buf.Size() > 0) 
        {
            std::this_thread::yield();  // Allow other I/O threads time
        } // end if
    } // end while

    write_buf.Reset();
} // end Dispatch_Requests





// //===============================================================================|
// void NeoConnection::Send_Run()
// {
    
// } // end Send_Run



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
 * @brief negotiates the supported neo4j version at the start of connection. The
 *  four bytes are neo4j magic numbers 0x6060B017 (in big endian) that is used as
 *  a signature followed by 4 bytes of version client requested version number.
 *  LightningBolt supports all versions upto 5.x (latest as of this writing).
 *
 * @return the supported version as 32-bit digit on success alas -1 on fail
 */
int NeoConnection::Negotiate_Version()
{
    u32 supported_version;
    const u8 version[]{
        0x60, 0x60, 0xB0, 0x17,         // neo4j magic number
        0x00, 0x00, 0x03, 0x05,         // request version 5.3+ at first
        0x00, 0x00, 0x00, 0x04,         // if not try version 4
        0x00, 0x00, 0x00, 0x03,         // version 3 and ...
		0x00, 0x00, 0x00, 0x02          // version 2 (last two are not supported)
    };

    if (Send(version, sizeof(version)) < 0)
        return -1;

    if (Recv((char*)&supported_version, sizeof(u32)) < 0)
        return -1;

	return htonl(supported_version) & 0x0F; // only interseted in the last byte
} // end Negotiate_Version

//===============================================================================|
/**
 * @brief connects to neo4j server using its lates (as of this writing) v5.x HELLO
 *  handshake message. The message/payload consists mainly creds and other extra
 *  stuff user could add that is based on the bolt protocol v5.x spec. The v5.x 
 *  spec consists of two steps, a basic hello and a logon message after version
 *  negotiation, thus LightningBolt implements both steps here as states of 
 *  the driver.
 * 
 * @return always 0 on success
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
        // persume version 5.3+ is supported; i.e. graft versions 5.0 - 5.0 as 
        //  same versions with minor changes
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
        hello = (BoltValue(BOLT_LOGON, { {
            mp("scheme", "basic"),
            mp("principal", conn_params["username"]),
            mp("credentials", conn_params["password"])
        } }));
    } // end else

    encoder.Encode(hello);
    Flush();
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
 * @brief is a minified version of Version 4 hello, and here for my reasons it
 *  will simply call v4 hello for legacy support.
 *.
 * @return 0 on success
 */
int NeoConnection::Send_Hello()
{
	Send_Hellov4(); // same as v4 hello for legacy support
	return 0;
} // end Send_Hello

//===============================================================================|
std::string NeoConnection::Get_Last_Error() const
{ 
    return err_string;
} // end Dump_Error


//===============================================================================|
std::string NeoConnection::Dump_Msg() const
{ 
    return message_string;
} // end Dump_Msg


//===============================================================================|
/**
 * @returns a stringified version of the current driver state.
 */
std::string NeoConnection::State_ToString() const
{ 
    static std::string states[DRIVER_STATES]{
        "Disconnected", "Connecting", "Ready", "Run", 
        "Streaming", "Trx" "BufferFull", "Error"
    };

    u8 s = static_cast<u8>(Get_State());
    return states[s];
} // end State_ToString


//===============================================================================|
u64 NeoConnection::Client_ID() const
{
    return client_id;
} // end client_id


//===============================================================================|
int NeoConnection::DummyS(u8* view, const size_t bytes)
{
    // ignore
    // BoltMessage unk;

    // if (pipelines.Size() > qid)
    //     decoder.Decode(active_queries[qid].cursor, unk);

    // sPrint("Error: %s", unk.ToString().c_str());
    Print("Error: %s", State_ToString().c_str());

    return -1;
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
int NeoConnection::Success_Hello(u8* view, const size_t bytes)
{
    if (state == BoltState::Connecting)
    {
        Set_State(BoltState::Logon);
        Send_Hellov5();     // log on the server
        return bytes;
    } // end if
    
    has_more = false;
    read_buf.Reset();
    Set_State(BoltState::Ready);
    return bytes;
} // end Success_Hello


//===============================================================================|
/**
 * @brief this handles a successful run query message; we temporariliy save the
 *  metadata returned for the next records inside the view field_names memeber.
 *  Sets the state to pull to inidcate we expect records next.
 * 
 * @param cursor the starting address for buffer
 * @param bytes the bytes recvd in the buffer
 * 
 * @return number of bytes to skip buffer in case of chunked responses
 */
int NeoConnection::Success_Run(u8* cursor, const size_t bytes)
{
    has_more = true; // indicates to decode loop that we have incoming records
    Set_State(BoltState::Pull);

    return decoder.Decode(cursor, view.field_names);
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
 * 
 * @return the bytes recvd always to skip the decode loop for Fetch() to handle
 */
int NeoConnection::Success_Pull(u8* cursor, const size_t bytes)
{
    view.cursor = cursor;
    view.size = bytes;
    view.offset = 0;
    has_more = false;       // persume done
    Set_State(BoltState::Streaming);

    return bytes;
} // end Success_Record

//===============================================================================|
int NeoConnection::Success_Record(u8* cursor, const size_t bytes)
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

    return skip;
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
 * @brief encodes a PULL message after a RUN command to fetch all results.
 */
void NeoConnection::Encode_Pull()
{
    BoltMessage pull(BoltValue(
        BOLT_PULL, {
            {mp("n", -1), mp("qid",-1)}
        }));

    encoder.Encode(pull);
} // end Send_Pull