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
 * @brief Parses the connection string only, makes it easy for object construction.
 */
NeoConnection::NeoConnection(void* pdisp, u64 id, const std::string &con_string)
    :pdispatcher(pdisp), encoder(write_buf), decoder(read_buf), state(BoltState::Disconnected)
{
    client_id = id;
    supported_version = 0;
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
 * @brief
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
    Set_State(BoltState::Connecting);
    qinfo.Enqueue({-1, BoltState::Connecting, nullptr});

    if (Connect() < 0)
        return -1;

    int ret;
    if ( (ret = Negotiate_Version()) < 0)
        return -1;

    if (supported_version >= 261)         // use version 5 hello
        hello = &NeoConnection::Send_Hellov5;
    else                    // version 4 and below
        hello = &NeoConnection::Send_Hellov4;
     
    // non-blocking socket
    if (Toggle_NonBlock() < 0)
        return -1;
    
    ret = (this->*hello)(false);
    if (ret < 0)
        return -1;

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
        if (isVersion5)
        {
            BoltMessage off(BoltValue(BOLT_LOGOFF, {}));
            encoder.Encode(off);
            BoltValue::Free_Bolt_Value(off.msg);
            Flush();

            Recv((char*)read_buf.Write_Ptr(), read_buf.Size());
        } // end if

        BoltMessage gb(BoltValue(BOLT_GOODBYE, {}));
        encoder.Encode(gb);
        BoltValue::Free_Bolt_Value(gb.msg);
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
    Wait_Until(BoltState::Ready);
    //printf(">inside run query\n"); 
    BoltMessage run(
        BoltValue(BOLT_RUN, {
            req->cypher, 
            req->parameters, 
            req->extras
        })
    );  
    encoder.Encode(run);
    BoltValue::Free_Bolt_Value(run.msg);
    qinfo.Enqueue({-1, BoltState::Run, req->On_Complete ? req->On_Complete : nullptr});


    ((CentralDispatcher*)pdispatcher)->Add_Ref();
    BoltMessage pull(BoltValue(
        BOLT_PULL, {
            {mp("n", -1), mp("qid",-1)}
    }));
    encoder.Encode(pull);
    BoltValue::Free_Bolt_Value(pull.msg);
    Flush();

    return 0;
} // end Run_Query


//===============================================================================|
int NeoConnection::Fetch(BoltMessage& out)
{
    Wait_Until(BoltState::Streaming);
    if (Get_State() != BoltState::Streaming)
        return 0;
     
    decoder.Decode(out, read_buf.Read_Ptr());
    
    if (out.msg.struct_val.tag == BOLT_SUCCESS)
    {
        Set_State(BoltState::Ready);
        read_buf.Reset();
        return 0;
    } // end if

    return 1;
} // end Fetch


//===============================================================================|
void NeoConnection::Wait_Until(BoltState desired)
{
    BoltState prev = Get_State();
    while (prev != desired)
    {
        state.wait(prev);
        prev = Get_State();
    } // end while
    
    // const auto deadline = std::chrono::steady_clock::now() + 
    //     std::chrono::seconds(3);
    // while (true)
    // {
    //     BoltState s = Get_State();
    //     if (s == desired)
    //         break;

        
    //     if (s == BoltState::Disconnected || s == BoltState::Error)
    //         Fatal("connection failed before reaching desired state: %s", State_ToString().c_str());

    //     if (std::chrono::steady_clock::now() > deadline)
    //         Fatal("timeout waiting for desired state");

    //     std::this_thread::yield();
    // }
}


//===============================================================================|
BoltState NeoConnection::Get_State() const
{
    return state.load(std::memory_order_acquire);
} // end Get_State


//===============================================================================|
void NeoConnection::Set_State(BoltState s)
{
    state.store(s, std::memory_order_release);
    state.notify_all();
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
    while (read_buf.Writable_Size() > 0)
    {
        ssize_t n = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());

        if (n <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            Close_Driver();
            return;
        } // end if

        read_buf.Advance(n);

        // if data is completely received push to response handler
        DecoderTask task{this, read_buf.Read_Ptr(), static_cast<size_t>(n)};
        ((CentralDispatcher*)pdispatcher)->Submit_Response(
            std::make_shared<DecoderTask>(task)
        );
    } // end if
} // end Poll_Readable


//===============================================================================|
/**
 * @brief persuming that data has been fully received from Poll_Readable(), the 
 *  function decodes the bolt encoded responses (message).
 */
void NeoConnection::Decode_Response(u8* view, const size_t bytes)
{
    if ( bytes == 0 || !(0xB0 & *(view + 2)))
        return;
    
    size_t skip = 0;
    // Dump_Hex((const char*)view, bytes);

    u8 tag = *(view + 3);   
    BoltMessage msg;
    BoltQInfo info;

    auto next_info = qinfo.Dequeue();
    if (next_info.has_value())
        info = next_info.value();
    else 
        return;     // may be out of sync so just drop it

    switch (tag)
    {
    case BOLT_SUCCESS:
        skip += (this->*success_handler[static_cast<u8>(info.state)])(view, bytes, info.Callback);
        break;

    case BOLT_FAILURE: 
        decoder.Decode(msg);
        Print("bolt failed: %s", msg.ToString().c_str());
        read_buf.Reset();
        break;

    case BOLT_RECORD:
        skip += Success_Record(view, bytes, info.Callback);
        break;

    default:
        Print("What is this?");
        decoder.Decode(msg);
        Print("bolt unk: %s", msg.ToString().c_str());
        read_buf.Reset();
    } // end switch
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
    if (!logon)
    {
        hello_count = 1;
        isVersion5 = true;
        
        BoltValue hello(BOLT_HELLO, {{
            mp("user_agent", "LightningBolt/v1.0.0")
        }});
        BoltMessage msg(hello);
        BoltValue::Free_Bolt_Value(hello);
        encoder.Encode(msg);
    } // end if first log
    else 
    {
        hello_count = 2;
        ((CentralDispatcher*)pdispatcher)->Add_Ref();
        qinfo.Enqueue({-1, BoltState::Connecting, nullptr});

        std::string uagent{("LightningBolt/" + std::to_string(client_id+1) +".0").c_str()};
        const char* username{user_auth[0].c_str()};
        const char* pwd{user_auth[1].c_str()};

        BoltMessage log(BoltValue(BOLT_LOGON, {{
            mp("user_agent", uagent.c_str()),
            mp("scheme", "basic"),
            mp("principal", username),
            mp("credentials", pwd)
        }}));
        encoder.Encode(log);
        BoltValue::Free_Bolt_Value(log.msg);
    } // end else

    // Dump_Hex((const char*)write_buf.Data(), write_buf.Size());
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
    hello_count = 0;
    isVersion5 = false;
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
    BoltValue::Free_Bolt_Value(hello.msg);

    Flush();
    return 0;
} // end Send_Hello


// //===============================================================================|
// void NeoConnection::Send_Pull()
// {

// } // end Send_Pull



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
        "Connecting", "Ready", "Run", 
        "Streaming", "Error", "Disconnected"
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
int NeoConnection::Dummy(u8* view, const size_t bytes,
    std::function<void(NeoConnection*)> callback)
{
    // ignore
    BoltMessage unk;
    decoder.Decode(view, unk);

    Print("Error: %s", unk.ToString().c_str());
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
int NeoConnection::Success_Hello(u8* view, const size_t bytes, 
    std::function<void(NeoConnection*)> callback)
{
    BoltMessage hello_rsp;
    int skip = decoder.Decode(view, hello_rsp);

    // printf("%s\n", hello_rsp.ToString().c_str());
    if (hello_count <= 1)
    {
        connection_id = hello_rsp.msg(0)["connection_id"].ToString();
        neo_timeout = atoi(((hello_rsp.msg(0)["hints"])
            ["connection.recv_timeout_seconds"]).ToString().c_str());
    } // end if count logon or hello v4

    if (isVersion5 && hello_count <= 1)
    {
        Send_Hellov5(true);     // log on the server
        return skip;
    } // end if
    
    Set_State(BoltState::Ready);
    read_buf.Reset();
    return skip;
} // end Success_Hello


//===============================================================================|
int NeoConnection::Success_Run(u8* view, const size_t bytes, 
    std::function<void(NeoConnection*)> callback)
{
    BoltMessage msg;

    //Dump_Hex((const char*)view, bytes);
    int skip = decoder.Decode(msg, view);
    
    if (skip < bytes)
    {
        skip += Success_Record(view + skip, bytes - skip, callback);
        return skip;
    } // end 

    qinfo.Enqueue({-1, BoltState::Streaming, callback});
    return skip;
} // end Success_Run


//===============================================================================|
int NeoConnection::Success_Record(u8* view, const size_t bytes,
    std::function<void(NeoConnection*)> callback)
{
    Set_State(BoltState::Streaming);

    // Dump_Hex((const char*)view, bytes);
    if (callback)
    {
        callback(this);
    } // end if callback

    return 0;
} // end Success_Record


//===============================================================================|
// int NeoConnection::Success_Record(u8* view, const size_t bytes)
// {
//     //Next_State();
//     Print("Inside Record");
//     // if (val.struct_val.tag == BOLT_SUCCESS)
//     // {
//     //     Set_State(BoltState::Ready);
//     //     return;
//     // }
//     return 0;
// } // end Success_Record