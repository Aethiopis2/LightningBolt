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
NeoConnection::NeoConnection(const std::string& con_string, const u64 cli_id,
    const BoltValue& extras)
    :encoder(write_buf), decoder(read_buf), client_id(cli_id),
    extra_connection_params(extras), state(BoltState::Disconnected)
{
    is_version5 = false;
    current_qid = hello_count = supported_version = 0;

    if (!Parse_Conn_String(con_string))
        Fatal("Invalid connection string: %s", con_string.c_str());
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
    return Get_State() == BoltState::Disconnected;
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
    Set_State(BoltState::Connecting);

    if ( (ret = Connect()) < 0)
        return -1;

    if ( (ret = Negotiate_Version()) < 0)
        return -1;

    if (supported_version >= 261)   // use version 5 hello
        hello = &NeoConnection::Send_Hellov5;
    else                            // version 4 and below
        hello = &NeoConnection::Send_Hellov4;
        
    
    ret = (this->*hello)(false);
    if (ret < 0)
        return -1;

    Poll_Readable();
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
        if (is_version5)
        {
            is_version5 = false;
            Set_State(BoltState::Connecting);
            BoltMessage off(BoltValue(BOLT_LOGOFF, {}));
            encoder.Encode(off);
            Flush();

            Poll_Readable();
        } // end if

        BoltMessage gb(BoltValue(BOLT_GOODBYE, {}));
        encoder.Encode(gb);
        Flush();
    } // end if not connecting

    Close_Driver();
} // end Stop


//===============================================================================|
/**
 * @brief a dispatcher based run query
 */
int NeoConnection::Run_Query(std::shared_ptr<BoltRequest> req)
{
    BoltMessage run(
        BoltValue(BOLT_RUN, {
            req->cypher, 
            req->parameters, 
            req->extras
        })
    );  

    encoder.Encode(run);
    Encode_Pull();
    // query_info.Enqueue({static_cast<s64>(current_qid), 2, 
    //     req->On_Complete ? req->On_Complete : nullptr});
    
    Flush();
    return 0;
} // end Run_Query



//===============================================================================|
/**
 * @brief a dispatcher based run query
 */
int NeoConnection::Run_Query(const char* cypher)
{
    BoltState s = Get_State();
    if (s != BoltState::Ready && s != BoltState::Run)
    {
        err_string = "invalid state: " + State_ToString();
        return -2;  // app error
    } // end if

    BoltMessage run(
        BoltValue(BOLT_RUN, {
            cypher, 
            BoltValue::Make_Map(),
            BoltValue::Make_Map()
        })
    );  

    encoder.Encode(run);
    Encode_Pull();
    Flush();

    Set_State(BoltState::Run);
    num_queries++;
    has_more = true;
    return 0;
} // end Run_Query

//===============================================================================|
void NeoConnection::Encode_Pull()
{
    BoltMessage pull(BoltValue(
        BOLT_PULL, {
            {mp("n", -1), mp("qid",-1)}
    }));
    encoder.Encode(pull);
} // end Send_Pull


//===============================================================================|
int NeoConnection::Fetch(BoltMessage& out)
{
    if (Get_State() != BoltState::Streaming)
        return 0;
    
    BoltQueryStateInfo temp;
    
    // int skip = decoder.Decode(temp.cursor, out);
    // temp.cursor += skip;

    // if (out.msg.struct_val.tag == BOLT_SUCCESS)
    // {
    //     Set_State(BoltState::Ready);
    //     query_info.Dequeue();
    //     if (query_info.Is_Empty())
    //         read_buf.Reset();
    //     return 0;
    // } // end if

    return 0;
} // end Fetch


//===============================================================================|
int NeoConnection::Fetch_Sync(BoltMessage& out)
{
    BoltState s = Get_State();
    if (s != BoltState::Streaming && s != BoltState::Run)
        return 0;
    
    if (is_chunked || has_more || num_queries > 0)
    {
        Poll_Readable();
        is_chunked = false;
    }

    // Dump_Hex((const char*)view.cursor, view.size);

    u8* temp = view.cursor;  // save it 
    int skip = decoder.Decode(view.cursor, out);
    view.cursor += skip;
    view.offset += skip;

    if (out.msg.struct_val.tag == BOLT_SUCCESS)
    {
        int ret = Success_Record(temp, view.size - view.offset);
        num_queries--;
        return 0;
    } // end if
    
    if (view.offset >= view.size)
        has_more = true;


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
/**
 * @brief
 */
int NeoConnection::Reconnect()
{
    int ret = (this->*hello)(false);
    if (ret < 0)
    {
        if (ret == -2)
        {
            err_string = "Invalid driver state: " + State_ToString();
            return -2;
        } // end if unsupported
        else 
        {
            return -1;
        } // end else net
    } // end if error

    return 0;
} // end Reconnect


//===============================================================================|
void NeoConnection::Run_Write(std::shared_ptr<BoltRequest> req)
{

} // end Run_Write


//===============================================================================|
/**
 * @brief
 */
void NeoConnection::Poll_Readable()
{
    while (read_buf.Writable_Size() > 0 && has_more)
    {
        ssize_t n = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
        if (n <= 0)
        {
            if (n == 0 || errno == EINTR)
                break;
            else 
            {
                Close_Driver();
                return;
            } // end else
        } // end if


        // if data is completely received push to response handler
        // Dump_Hex((const char*)read_buf.Read_Ptr(), n);
        Decode_Response(read_buf.Read_Ptr(), n);
        read_buf.Consume(n);
        read_buf.Advance(n);
    } // end while
} // end Poll_Readable


//===============================================================================|
/**
 * @brief persuming that data has been fully received from Poll_Readable(), the 
 *  function decodes the bolt encoded responses (message).
 */
void NeoConnection::Decode_Response(u8* view, const size_t bytes)
{
    if (bytes == 0)
        return;
    
    size_t skip = 0;
    while (skip < bytes)
    {
        // Dump_Hex((const char*)view, bytes);
        if (!(0xB0 & *(view + 2)))
            return;

        u8 s = static_cast<u8>(Get_State());
        u8 tag = *(view + 3); 
        // printf("bytes = %d -- skip = %d -- tag = %02x\n", bytes, skip, tag);
        switch (tag)
        {
        case BOLT_SUCCESS:
            skip = (this->*success_handler[s])(view, bytes);
            break;

        case BOLT_FAILURE: 
            Print("bolt failed");
            skip = bytes;
            read_buf.Reset();
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


//===============================================================================|
/**
 * @brief negotiates the supported neo4j version at the start of connection. The
 *  four bytes are neo4j magic numbers 0x6060B017 (in big endian) that is used as
 *  a signature followed by 4 bytes of version client requested version number. 
 *  LightningBolt supports all versions upto 5.x (latest as of this writing). 
 * 
 * @return 0 on success -1 on fail
 */
int NeoConnection::Negotiate_Version()
{
    const u8 version[]{
        0x60, 0x60, 0xB0, 0x17,         // neo4j magic number
        0x00, 0x00, 0x01, 0x05,         // request version 5 at first
        0x00, 0x00, 0x00, 0x04,         // if not try version 4
        0x00, 0x00, 0x00, 0x03,         // version 3 and ...
        0x00, 0x00, 0x00, 0x02          // version 2
    };

    if (Send(version, sizeof(version)) < 0)
        return -1;
    
    if (Recv((char*)&supported_version, sizeof(u32)) < 0)
        return -1;
    
    supported_version = htonl(supported_version);
    return 0;
} // end Negotiate_Version



//===============================================================================|
int NeoConnection::Send_Hellov5(bool logon)
{
    BoltMessage hello;
    if (!logon)
    {      
        hello = (BoltValue(
            BOLT_HELLO, {{
                mp("user_agent", ("LightningBolt/v" + std::to_string(client_id+1) + ".0").c_str())
            }}
        ));

        hello_count = 1;
        is_version5 = true;  
    } // end if first log
    else 
    {
        std::string uagent{("LightningBolt/" + std::to_string(client_id+1) +".0").c_str()};
        const char* username{user_auth[0].c_str()};
        const char* pwd{user_auth[1].c_str()};

        hello = (BoltValue(BOLT_LOGON, {{
            mp("user_agent", uagent.c_str()),
            mp("scheme", "basic"),
            mp("principal", username),
            mp("credentials", pwd)
        }}));

        hello_count = 2;
    } // end else

    encoder.Encode(hello);
    Flush();
    return 0;
} // end Send_Hello


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
int NeoConnection::Send_Hellov4(bool logon)
{
    std::string uagent{("LightningBolt/" + std::to_string(client_id+1) +".0").c_str()};
    const char* username{user_auth[0].c_str()};
    const char* pwd{user_auth[1].c_str()};
    
    BoltMessage hello(BoltValue(BOLT_HELLO, {
        BoltValue({
            mp("user_agent", uagent.c_str()),
            mp("scheme", "basic"),
            mp("principal", username),
            mp("credentials", pwd)
        })
    }));
    encoder.Encode(hello);

    hello_count = 0;
    is_version5 = false;

    Flush();
    return 0;
} // end Send_Hello


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
 * @brief initializes the members username and password which are creds used for 
 *  authentication by parsing the paramter provided along with the server info
 *  which includes the neo4j server address and listening port.
 * 
 * @param con_string a http formated string containing useful info about server
 *  and authorization; i.e. http://localhost:7786@username:password
 * 
 * @return true on success, alas false.
 */
bool NeoConnection::Parse_Conn_String(const std::string &con_string)
{
    // locate the first '@' string, this is the delimeter between the
    //  user info and server info.
    size_t pos = con_string.find_first_of('@');
    if (pos == std::string::npos)
        return false;

    // now split string into 2; server info and user auth info
    std::string server_info = con_string.substr(0, pos);
    std::string auth_info = con_string.substr(pos + 1, con_string.length() - (server_info.length() + 1));

    // is there an http character?
    if (server_info.find("http") != std::string::npos)
    {
        server_info = server_info.substr(6, server_info.length());
    } // end if don't begin as such

    // host port info parse
    std::vector<std::string> temp{Split_String(server_info, ':')};
    if (temp.size() != 2)
        return false;
    
    hostname = temp[0];
    port = temp[1];

    // split user auth info as vector; 0 - username, 1 - password
    user_auth = Split_String(auth_info, ':');
    if (user_auth.size() != 2)
        return false;
    
    return true;
} // end Parse_Conn_String


//===============================================================================|
// void NeoConnection::Next_State()
// {
//     BoltState s = Get_State();
//     if (s == BoltState::Disconnected) Set_State(BoltState::Connecting);
//     else if (s == BoltState::Connecting) Set_State(BoltState::Ready);
//     else if (s == BoltState::Ready) Set_State(BoltState::Run);
//     else if (s == BoltState::Run) Set_State(BoltState::Pull);
//     else if (s == BoltState::Pull) Set_State(BoltState::Streaming);
//     else if (s == BoltState::Streaming) Set_State(BoltState::Ready);
//     // else Set_State(BoltState::Error);
// } // end Next_State


//===============================================================================|
std::string NeoConnection::Dump_Error() const
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
int NeoConnection::Dummy(u8* view, const size_t bytes)
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
 *  with this function twice HELLO + LOGON. Since driver doesn't distinguish 
 *  connection states, we simply track where by counting. Once this completes driver
 *  is set to Ready state. 
 *  It saves the connection_id's and timeout values sent from server.
 * 
 * @param view start of address to decode from
 * @param bytes length of the recvd data
 */
int NeoConnection::Success_Hello(u8* view, const size_t bytes)
{
    Set_State(BoltState::Ready);
    if (is_version5 && hello_count <= 1)
    {
        Set_State(BoltState::Connecting);
        Send_Hellov5(true);     // log on the server
        return bytes;
    } // end if
    
    has_more = false;
    read_buf.Reset();
    return bytes;
} // end Success_Hello


//===============================================================================|
/**
 * @brief
 */
int NeoConnection::Success_Run(u8* cursor, const size_t bytes)
{
    Set_State(BoltState::Streaming);
    int skip = decoder.Decode(cursor, view.field_names);
    if (skip < bytes)
    {
        // skip = bytes - skip;
        is_chunked = true;
    } // end if

    has_more = true;
    return skip;
} // end Success_Run


//===============================================================================|
int NeoConnection::Success_Pull(u8* cursor, const size_t bytes)
{
    Set_State(BoltState::Streaming);
    // Dump_Hex((const char*)cursor, bytes);

    view.cursor = cursor;
    view.size = bytes;
    view.offset = 0;

    has_more = false;   // persume we're done.
    return bytes;
} // end Success_Record


//===============================================================================|
int NeoConnection::Success_Record(u8* cursor, const size_t bytes)
{
    Set_State(BoltState::Ready);
    int skip = decoder.Decode(cursor, view.summary_meta);

    //Print("%s", view.summary_meta.ToString().c_str());

    has_more = false;
    read_buf.Reset();
    return skip;
} // end Success_Record