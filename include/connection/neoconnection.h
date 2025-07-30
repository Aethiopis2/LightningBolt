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
    Disconnected,   // dirver disconnected
    Connecting,     // driver is connecting TCP + version neg + hello
    Ready,          // driver is ready for call and is idle
    Run,            // driver is running query/cypher
    Streaming,      // driver is in a streaming state; i.e. actually fetching
    Trx,            // manual transaction state
    BufferFull,     // the receiving buffer is full and needs to wait
    Error,          // driver has encountered errors?  
};
constexpr int DRIVER_STATES = 8;



/**
 * @brief this structure is used to track the state of query opereration esp during
 *  pipelined calls. It also stores a pointer to a callback function that is requesting
 *  the results of the query in async time. 
 *      1. Ready or Idle    --> appears during initalization only
 *      2. Trx              --> running explicit transaction mode expecting commit or rollback
 *      3. Run              --> just sent a run query command
 *      4. Streaming        --> downloading the actual records or after pull sent
 */
struct BoltQueryStateInfo
{
    s64 qid;            // query id
    u8 index;           // index into the next handler

    // a callback for true async query calls.
    std::function<void(NeoConnection*)> Callback;

    u8* cursor{nullptr};
    size_t length{0};
    BoltMessage fields;
};


/**
 * @brief a view points at the next row/value to decode in a single
 *  bolt request/query. Since Bolt returns pipelined requests in the order
 *  sent, we can queue each response for processing in a ring buffer.
 */
struct BoltView
{
    u8* cursor;                 // the current address
    u64 offset;                 // the cursor offset
    size_t size;                // the size of view into buffer
    BoltMessage field_names;    // meta info on query usually the field names or labels
    BoltMessage summary_meta;   // summary meta info trailing at the end of records
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoConnection : public TcpClient
{
public: 

    NeoConnection(const u64 cli_id, const std::string& connection_string);
    ~NeoConnection();
    
    bool Is_Closed() const override;
    int Start();
    void Stop();

    int Run_Query(std::shared_ptr<BoltRequest> req);
    int Run_Query(const char* cypher);
    void Decode_Response(u8* view, const size_t bytes);
    int Fetch(BoltMessage& out);
    int Fetch_Sync(BoltMessage& out);

    int Begin_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Commit_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Rollback_Transaction(const BoltValue& options = BoltValue::Make_Map());

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
    
    BoltState state;        // the state of connection
    u64 current_qid;        // incremental identifier for queries
    u64 client_id;          // connection identifier
    u32 supported_version;  // the current version supported by neo4j server
    u32 num_queries{0};
    bool has_more{true};
    bool is_chunked{false};

    

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;
    BoltView view;
    
    // helper's during initalizaiton
    bool is_version5;
    u8 hello_count;

    std::vector<std::string> user_auth;             // db creds, i.e. username and password resp
    std::string message_string;             
    std::string err_string;


    // utilities
    int Reconnect();
    void Close_Driver();
    
    int Negotiate_Version();
    int Send_Hellov4(bool logon=false);
    int Send_Hellov5(bool logon=false);

    void Run_Read(std::shared_ptr<BoltRequest> req);
    void Run_Write(std::shared_ptr<BoltRequest> req);
    size_t Extract_Bolt_Message_Length(const u8* view, size_t available);

    void Encode_Pull();

    int Dummy(u8* view, const size_t bytes);
    int Success_Hello(u8* view, const size_t bytes);
    int Success_Run(u8* view, const size_t bytes);
    int Success_Pull(u8* view, const size_t bytes);
    int Success_Record(u8* view, const size_t bytes);
    void Fail_Hello();
    void Fail_Run();
    void Fail_Pull();
    void Fail_Record();

    
    bool Parse_Conn_String(const std::string &conn_string);
    //void Next_State();
    

    // jump tables and such
    using Hello_Fn = int (NeoConnection::*)(bool);
    Hello_Fn hello;

    using Success_Fn = int (NeoConnection::*)(u8*, const size_t);
    using Fail_Fn = int (NeoConnection::*)(u8*, const size_t, 
        std::function<void(NeoConnection*)>);

    Success_Fn success_handler[5]{ 
        &NeoConnection::Dummy,
        &NeoConnection::Success_Hello,
        &NeoConnection::Dummy,
        &NeoConnection::Success_Run, 
        &NeoConnection::Success_Pull,
    };
};