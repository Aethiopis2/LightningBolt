/**
 * @brief implementation detials for NeoConnection neo4j bolt based driver
 * 
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 12th of Decemeber 2025, Friday
 * 
 * @copyright Copyright (c) 2025
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/neoconnection.h"




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief makes sure everything setup proper
 */
NeoConnection::NeoConnection()
    :encoder(write_buf), decoder(read_buf), client_id(-1), bytes_recvd(0),
     has_more(true), err_string(""), state(ConnectionState::Disconnected),
     conn_params(nullptr), sversion(0.0)
{
} // end NeoConnection


/**
 * @brief does nothing here
 */
NeoConnection::~NeoConnection() { } 


/**
 * @brief sets the members hostname and port from the parameter for later, i.e.
 *  during re-connection. It then start's a TCP Connection with the server over
 *  the provided address. To start the connection it uses the member Reconnect().
 *  It also optionally sets the client id.
 * 
 * @param params map connection parameters passed BoltValue pointer type
 * @param cli_id optional id to set for this connection
 *
 * @return 0 on success. -1 on sys error with code contained in errno
 */
int NeoConnection::Init(BoltValue* params, const int cli_id)
{
    conn_params = params;

    // host string itself may be split into multiple addresses using ';' char
    //  let's split it there and successfully connect to the first address found
    std::vector<std::string> addresses{ Split_String(conn_params[0]["host"].ToString(), ';') };
    bool is_open = false;
    for (auto& address : addresses)
    {
        // initalize and tcp connect
        std::vector<std::string> creds{ Split_String(address, ':') };
        hostname = creds[0];
        port = creds[1];
        if (Reconnect() == 0)
        {
            is_open = true;
            break;
        } // end if
    } // end for all addresses

    if (!is_open)
        return -1;

    // set client id
    Set_ClientID(cli_id);

    return 0;
} // end Init


/**
 * @brief starts a connection to a server at address provided by string members
 *  or attributes if you like, hostname & port. On successful connection it sets
 *  the boolean attribute is_open to true to flag open connection
 * 
 * @return 0 on success. -1 on sys error with code contained in errno
 */
int NeoConnection::Reconnect()
{
    if (Connect() < 0)
    {
        return -1;
    } // end if no good

    return 0;
} // end Reconnect


/**
 * @brief waits on recieve /recv system call on blocking mode. It stops recieving
 *  when a compelete bolt packet is recieved or the consuming routine has deemed
 *  it necessary to stop fetching by setting has_more to false. However, once done
 *  it must be reset back for the loop to continue. Should the buffer lack space
 *  to recv chunks it grows to accomodiate more. Once it completes a full message
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
int NeoConnection::Poll_Readable()
{
    // do we need to grow for space
    if (read_buf.Writable_Size() < 256)
        read_buf.Grow(read_buf.Capacity() << 1);

    while (has_more)
    {
        ssize_t n = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
        if (n == 0)
        {
            err_string = "peer has closed the connection";
            Terminate();
            return -3;
        } // end if peer closed connection
        else if (n < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                continue;
            else
            {
                Terminate();
                return -1;
            } // end else
        } // end if

        // if data is completely received push to response handler
        bytes_recvd += n;
        if (Recv_Completed())
        {
            if (!Decode_Response(read_buf.Read_Ptr(), bytes_recvd))
                return -2;

            read_buf.Consume(bytes_recvd);
            bytes_recvd = 0;
        } // end if complete

        read_buf.Advance(n);
        if (read_buf.Writable_Size() < 256)
            read_buf.Grow(read_buf.Capacity() << 1);
    } // end while

    return 0;
} // end Poll_Readable


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
 * @return a true on success
 */
bool NeoConnection::Flush()
{
    while (!write_buf.Empty())
    {
        if (!Poll_Writable())
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


/**
 * @brief encodes the boltvalue reference and flushes it to peer. It then blocks
 *  and waits for a response from server.
 *
 * @param cell pointer to NeoCell instance used to invoke function
 * @param fn_decoder a callback value for poll function
 * @param has_more a boolean used for loop control
 * @param v the BoltValue to encode and send
 *
 * @return true on success alas false on sys error
 */
inline bool NeoConnection::Flush_And_Poll(BoltValue& v)
{
    encoder.Encode(v);
    if (!Flush())
        return false;

    if (!Decode_Response(read_buf.Read_Ptr(), bytes_recvd))
        return false;

    return true;
} // end Flush_And_Poll


/**
 * @brief returns false if a connection is closed
 */
bool NeoConnection::Is_Closed() const
{
    return Get_State() == ConnectionState::Disconnected;
} // end Is_Closed


/**
 * @brief termiantes the active connection via Disconnect() member and sets the
 *  boolean member is_open to false.
 */
void NeoConnection::Terminate()
{
    Disconnect();
    Set_State(ConnectionState::Disconnected);
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
 * @brief set's the state of connection in memory_order_release
 * 
 * @param s the new state to set
 */
void NeoConnection::Set_State(const ConnectionState s)
{
    state.store(s, std::memory_order_release);
    //state.notify_one();
} // end Set_State


/**
 * @brief return's the last error as string encounterd in this connection
 */
std::string NeoConnection::Get_Last_Error() const
{
    return err_string;
} // end Dump_Error


/**
 * @brief returns a stringified version of the current driver state.
 */
std::string NeoConnection::State_ToString() const
{
    static std::string states[CONNECTION_STATES]{
        "Disconnected", "Connecting","LOGON", "Ready",
        "Run", "Pull", "Streaming", "Error"
    };

    u8 s = static_cast<u8>(Get_State());
    return states[s % CONNECTION_STATES];
} // end State_ToString


/**
 * @brief return's the current atomic state of the connection
 */
ConnectionState NeoConnection::Get_State() const
{
    return state.load(std::memory_order_acquire);
} // end Set_State


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
                continue;
            else if (n == 0)
                err_string = "peer has closed the connection";

            Terminate();
            return false;
        } // end if

        write_buf.Consume(n);
    } // end while

    return true;
} // end Poll_Writable


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

    while (bytes_seen < bytes_recvd)
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
    if (bytes_seen == bytes_recvd) return true; // complete packet received
    else return false;                          // more data needed
} // end Recv_Completed


/**
 * @brief persuming that data has been fully received from Poll_Readable(), the
 *  function decodes the bolt encoded responses (message).
 */
bool NeoConnection::Decode_Response(u8* view, const size_t bytes)
{
    if (bytes == 0)
        return 0;

    Dump_Hex((const char*)view, bytes);
    size_t decoded = 0;
    while (decoded < bytes)
    {
        if (!(0xB0 & *(view + 2)))
        {
            err_string = "Invalid Bolt Message Format.";
            return false;
        } // end if not valid

        u8 s = static_cast<u8>(Get_State());
        u8 tag = *(view + 3);
        int skip = 0;

        switch (tag)
        {
        case BOLT_SUCCESS:
            skip = (this->*success_handler[s])(view, bytes);
            break;

        case BOLT_FAILURE:
            skip = (this->*fail_handler[s])(view, bytes);
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
        decoded += skip;
    } // end while

    return true;
} // end Decode_Response


/**
 * @brief performs version negotiation as specified by the bolt protocol. It uses
 *  v5.7+ manifest negotiation style to allow the server respond with version
 *  numbers supported. If server does not support manifest, it simply starts
 *  with version 4 or less.
 *
 * @return supported version on success or < 0 on fail
 */
float NeoConnection::Negotiate_Version()
{
    u8 versions[64]{
        0x60, 0x60, 0xB0, 0x17,         // neo4j magic number
        0x00, 0x00, 0x01, 0xFF,         // manifest v1
        0x00, 0x00, 0x04, 0x04,         // if not try version 4
        0x00, 0x00, 0x00, 0x03,         // version 3 and ...
        0x00, 0x00, 0x00, 0x02          // version 2 (last two are not supported)
    };

    if (Send(versions, 20) < 0)
    {
        Disconnect();
        return -1;
    } // end if bad sending

    if (Recv((char*)versions, sizeof(versions)) < 0)
    {
        Disconnect();
        return -1;
    } // end if bad reception

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
    else nums = 1;

    // pick the higest version
    float max_version = 0.0f;        // reset
    u32* alias = reinterpret_cast<u32*>(ptr);
    for (int i = 0; i < nums; i++)
    {
        u32 v = htonl(*reinterpret_cast<u32*>(ptr));
        float supported = (v & 0x000F) + (static_cast<float>(((v & 0x0F00) >> 8)) / 10.0f);
        if (max_version < supported)
        {
            max_version = supported;
            alias = reinterpret_cast<u32*>(ptr);
        } // end if supported

        ptr += sizeof(u32);
    } // end for

    // send to server; and echo back whatever the server caps are
    if (Send(alias, 5) < 0)
        return -1;

    Toggle_NonBlock();
    sversion = max_version;
    return max_version;
} // end Negotiate_Version


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
        float active_version;
        float removed_version;
    };
    std::vector<Param_Helper> param_list{
        {"schema", conn_params[0]["schema"], 5.0, 5.0},
        {"user_agent", conn_params[0]["user_agent"].type != BoltType::Unk ?
            conn_params[0]["user_agent"] :
            BoltValue(("LB/v" + std::to_string(client_id + 1) + ".0").c_str()), 1.0, 100},

        {"patch_bolt", conn_params[0]["patch_bolt"], 4.3, 4.4},
        {"routing", conn_params[0]["routing"], 4.1, 100},
        {"notifications_minimum_severity", conn_params[0]["notifications_minimum_severity"], 5.2, 100},
        {"notifications_disabled_categories", conn_params[0]["notifications_disabled_categories"], 5.2, 5.4}
    };

    ConnectionState s = Get_State();
    if (s == ConnectionState::Connecting)
    {
        // persume version 5.3+ is supported; i.e. graft versions 5.x as 
        //  same versions with minor changes

        // update state to logon
        Set_State(ConnectionState::Logon);

        hello.msg = BoltValue::Make_Struct(BOLT_HELLO);
        BoltValue bmp = BoltValue::Make_Map();

        for (auto& param : param_list)
        {
            if (param.val.type != BoltType::Unk && param.active_version <= sversion && 
                param.removed_version > sversion)
            {
                bmp.Insert_Map(param.key.c_str(), param.val);
            } // end if not unknown
        } // end for params

        // add bolt agent info
        if (sversion >= 5.3)
        {
            bmp.Insert_Map("bolt_agent", BoltValue({
                    mp("product", "LightningBolt/v1.0.0"),
                    mp("platform", "Linux 6.6.87.2/microsoft-standard-WSL2; x64"),
                    mp("language", "C++/17"),
                }, false));
        } // end sversion

        hello.msg.Insert_Struct(bmp);
    } // end if connecting
    else if (s == ConnectionState::Logon)
    {
        Set_State(ConnectionState::Connecting);

        hello = (BoltValue(BOLT_LOGON, { {
            mp("scheme", "basic"),
            mp("principal", conn_params[0]["username"]),
            mp("credentials", conn_params[0]["password"])
        } }));
    } // end else

    encoder.Encode(hello);
    if (!Flush())
        return -1;

    return 0;
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
 * @return 0 on success or -2 on fail to define app specific error
 */
int NeoConnection::Send_Hellov4()
{
    std::string uagent{ ("LB/" + std::to_string(client_id + 1) + ".0").c_str() };

    BoltMessage hello(BoltValue(BOLT_HELLO, {
        BoltValue({
            mp("user_agent", uagent.c_str()),
            mp("scheme", "basic"),
            mp("principal", conn_params[0]["username"]),
            mp("credentials", conn_params[0]["password"])
        })
        }));

    encoder.Encode(hello);
    if (!Flush())
        return -1;

    return 0;
} // end Send_Hello


/**
 * @brief serves as a safety net when things fire outof sync so as not to cause
 *  segfaults and allow the application to continue safely after having sent a
 *  REST signal possibly, in the worst case.
 *
 * @param cursor the current position in the buffer to decode from
 * @param bytes the number of bytes recieved.
 *
 * @return -2 to indicate error and requires RESET or Reconnect
 */
inline int NeoConnection::Dummy(u8* view, const size_t bytes)
{
    Set_State(ConnectionState::Error);
    err_string = "State out of sync: " + State_ToString();

    return -2;      // should send RESET
} // end dummy


/**
 * @brief First message received after authentication with Neo4j graphdb server.
 *  We expect to major modes of connections v4.x and v5.x (latest). In v5.x we deal
 *  with this function twice HELLO + LOGON. Once successfuly authenticated it
 *  sets the driver state to ready.
 *
 * @param view start of address to decode from
 * @param bytes length of the recvd data
 *
 * @return bytes recevd
 */
inline int NeoConnection::Success_Hello(u8* view, const size_t bytes)
{
    ConnectionState s = Get_State();
    if (s == ConnectionState::Logon)
    {
        Send_Hellov5();     // log on the server
        return bytes;
    } // end if

    has_more = false;
    read_buf.Reset();
    Set_State(ConnectionState::Ready);

    return bytes;
} // end Success_Hello


/**
 * @brief this handles a successful run query message; we temporariliy save the
 *  metadata returned for the next records inside the view field_names memeber.
 *  Sets the state to pull to inidcate we expect records next.
 *
 * @param cursor the starting address for buffer
 * @param bytes the bytes recvd in the buffer
 */
inline int NeoConnection::Success_Run(u8* cursor, const size_t bytes)
{
    has_more = true; // indicates to decode loop that we have incoming records
    Set_State(ConnectionState::Pull);

    // parse and save the field names; until its needed and guranteed to exist
    //  as long as we are streaming the result
    int skip = decoder.Decode(cursor, view.field_names);
    return skip;
} // end Success_Run


/**
 * @brief this is called immidiately after Successful Run, and it simply sets the
 *  member view to point at the next bytes and sets has_more to false to make
 *  sure the recv loop won't run again unless required through Fecth(). It updates
 *  the state to streaming to indicate driver is now consuming buffer.
 *
 * @param cursor the starting address for buffer
 * @param bytes the bytes recvd in the buffer
 */
inline int NeoConnection::Success_Pull(u8* cursor, const size_t bytes)
{
    Set_State(ConnectionState::Streaming);

    // update the cursor view
    view.cursor = cursor;
    view.size = bytes;
    view.offset = 0;
    has_more = false;       // persume done

    return bytes;
} // end Success_Record


/**
 * @brief handles the success summary message sent after the completion of each
 *  record streaming. If the summary message contains "has_more" key and is set
 *  to true then it persumes not done and sets the state back to PULL for Fetch()
 *  once done it sets the state to ready.
 *
 * @param cursor the current position in the buffer to decode from
 * @param bytes the number of bytes recieved.
 */
inline int NeoConnection::Success_Record(u8* cursor, const size_t bytes)
{
    Dump_Hex((const char*)cursor, bytes);
    Set_State(ConnectionState::Ready);
    int skip = decoder.Decode(cursor, view.summary_meta);

    if (view.summary_meta.msg[0].type == BoltType::Map &&
        view.summary_meta.msg[0]["has_more"].type != BoltType::Unk &&
        view.summary_meta.msg[0]["has_more"].bool_val == true)
    {
        // all done
        has_more = true;
        Set_State(ConnectionState::Pull);
    } // end if
    else
    {
        has_more = false;
        Set_State(ConnectionState::Ready);
        read_buf.Reset();
    } // end else

    return bytes;
} // end Success_Record


/**
 * @brief triggered when the first BOLT HELLO message/negotiation fails. After
 *  which connection is presumed closed and must be restarted again.
 *
 * @param view the start of buffer message to decode (contains error string)
 * @param bytes length of the buffer above.
 */
inline int NeoConnection::Fail_Hello(u8* view, const size_t bytes)
{
    BoltMessage fail;
    int skip = decoder.Decode(view, fail);
    err_string = fail.ToString();

    Set_State(ConnectionState::Disconnected);
    has_more = false;

    return skip;
} // end Fail_Hello


/**
 * @brief
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
inline int NeoConnection::Fail_Run(u8* view, const size_t bytes)
{
    Fail_Hello(view, bytes);
} // end Fail_Run


/**
 * @brief
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
inline int NeoConnection::Fail_Pull(u8* view, const size_t bytes)
{

} // end Fail_Pull


/**
 * @brief
 *
 * @param view the start of buffer message to decode
 * @param bytes length of the buffer above.
 */
int inline NeoConnection::Fail_Record(u8* view, const size_t bytes)
{
    BoltMessage fail;

    Set_State(ConnectionState::Run);
    decoder.Decode(view, fail);
    err_string = fail.ToString();

    has_more = false;
    read_buf.Reset();
    write_buf.Reset();
} // end Fail_Record