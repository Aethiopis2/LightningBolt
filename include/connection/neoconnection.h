/**
 * @file neo4j_cpl.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief 
 * @version 1.0
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/tcp_client.h"
#include "utils/lock_free_queue.h"
#include "bolt/bolt_decoder.h"
#include "bolt/bolt_encoder.h"
#include "bolt/bolt_request.h"






//===============================================================================|
//          ENUM
//===============================================================================|
/**
 * LightningBolt states over bolt
 */
enum class BoltState : u8 {
    Connecting,         // driver is connecting TCP + version neg + hello
    Ready,              // driver is ready for call and is idle
    Run,                // driver is running query/cypher
    Streaming,          // driver is in a streaming state; i.e. actually fetching
    Error,              // driver has encountered errors?
    Disconnected        // dirver disconnected
};
constexpr int DRIVER_STATES = 6;



/**
 * @brief this structure is used to track the state and other relevant
 *  info relating to a query operation. useful in pipelined calls. 
 *  Basically a query exists in the following states as far as LB is concerned
 *      1. Ready or Idle    --> appears during initalization only
 *      2. Run              --> just sent a run query command
 *      3. Streaming        --> downloading the actual records or after pull sent
 */
struct BoltQInfo
{
    s64 qid;            // query id
    BoltState state;    // the current state of query

    // a callback for true async query calls.
    std::function<void(NeoConnection*)> Callback;
};


struct BoltCursor
{
    u8* cursor;             // the current address
    size_t view_len;        // the size of view into buffer
    bool is_done{false};    // can we reuse the space?
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoConnection : public TcpClient
{
public: 

    NeoConnection(void* dsp, const u64 id, const std::string &connection_string);
    ~NeoConnection();
    
    bool Is_Closed() const override;
    int Start();
    void Stop();

    int Run_Query(std::shared_ptr<BoltRequest> req);
    void Decode_Response(u8* view, const size_t bytes);
    int Fetch(BoltMessage& out);
    void Wait_Until(BoltState desired);

    void Poll_Readable();
    void Poll_Writable();
    void Flush();

    void Set_State(BoltState s);
    BoltState Get_State() const;
    u64 Client_ID() const;

    std::string Dump_Error() const;
    std::string Dump_Msg() const;
    std::string State_ToString() const;

private: 

    std::atomic<BoltState> state;        
    LockFreeQueue<BoltQInfo> qinfo;         // queue of states for processing

    std::vector<std::string> user_auth;     // db creds, i.e. username and password resp
    std::string connection_id;              // neo4j server provided connection id
    s64 neo_timeout;                        // time out value ret from server
    std::string message_string;             
    std::string err_string;
    
    
    u64 client_id;
    u32 supported_version;          // the current version supported by neo4j server
    bool isVersion5;
    int hello_count=0;

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;
    BoltEncoder encoder;
    BoltDecoder decoder;
    
    std::vector<BoltMessage> messages;
    void* pdispatcher;     // pointer to dispatcher as void to avoid circular references

    // utilities
    int Reconnect();
    void Close_Driver();
    
    int Negotiate_Version();
    int Send_Hellov4(bool logon=false);
    int Send_Hellov5(bool logon=false);

    void Run_Read(std::shared_ptr<BoltRequest> req);
    void Run_Write(std::shared_ptr<BoltRequest> req);
    size_t Extract_Bolt_Message_Length(const u8* view, size_t available);

    int Dummy(BoltCursor& view, std::function<void(NeoConnection*)> callback);
    int Success_Hello(BoltCursor& view, std::function<void(NeoConnection*)> callback);
    int Success_Run(BoltCursor& view, std::function<void(NeoConnection*)> callback);
    //int Pull_Record(u8* view, const size_t bytes);
    int Success_Record(BoltCursor& view, std::function<void(NeoConnection*)> callback);
    void Fail_Hello();
    void Fail_Run();
    void Fail_Pull();
    void Fail_Record();

    
    bool Parse_Conn_String(const std::string &conn_string);
    //void Next_State();
    

    // jump tables and such
    using Hello_Fn = int (NeoConnection::*)(bool);
    Hello_Fn hello;

    using Success_Fn = int (NeoConnection::*)(BoltCursor&,
        std::function<void(NeoConnection*)>);
    using Fail_Fn = int (NeoConnection::*)(u8*, const size_t, 
        std::function<void(NeoConnection*)>);

    Success_Fn success_handler[DRIVER_STATES]{
        &NeoConnection::Success_Hello, &NeoConnection::Dummy, 
        &NeoConnection::Success_Run, &NeoConnection::Success_Record,
        &NeoConnection::Dummy, &NeoConnection::Dummy
    };
};