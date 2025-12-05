/**
 * @brief definition of NeoConnection class.
 * 
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
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
//          ENUM & TYPES
//===============================================================================|
/**
 * LightningBolt states over bolt
 */
enum class BoltState : u8 {
    Disconnected,   // dirver disconnected
    Connecting,     // driver is connecting TCP + version neg 
	Logon,          // driver is sending logon info (v5.1+)     
    Ready,          // driver is ready for call and is idle
    Run,            // driver is expecting run success/fail message
    Pull,           // driver is expecting pull success/fail message
    Streaming,      // driver is in a streaming state; i.e. reading buffer
    Trx,            // manual transaction state
    Error,          // driver has encountered errors?  
};
constexpr int DRIVER_STATES = 9;

//===============================================================================|
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

//===============================================================================|
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
/**
 * @brief the result of a bolt query. It consists of three fields, the fields
 *  names for the query result, a list of records, and a summary detail, as per
 *  driver specs for neo4j
 */
struct BoltResult
{
	BoltValue fields;           // the field names for the record
    BoltValue records;          // the actual list of records returned
    BoltValue summary;          // the summary message at end of records
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoConnection : public TcpClient
{
public: 

    NeoConnection(BoltValue connection_params, const u64 cli_id = 0);
    ~NeoConnection();
    
    bool Is_Closed() const override;
    int Start();
    void Stop();

    int Run_Query(const char* cypher, BoltValue params=BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1);
    int Fetch(BoltMessage& out);
    int Fetch(BoltResult& res);

    int Begin_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Commit_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Rollback_Transaction(const BoltValue& options = BoltValue::Make_Map());

    void Set_State(BoltState s);
    BoltState Get_State() const;
    u64 Get_Client_ID() const;
    std::string Get_Last_Error() const;
    std::string State_ToString() const;

private: 
    
    u64 client_id;          // connection identifier
    u64 current_qid;        // incremental identifier for queries
	int num_queries;        // number of active pipelined queries
	ssize_t bytes_to_decode{ 0 };   // helper to track bytes to decode

    bool has_more{true};
    bool is_chunked{false};

    BoltValue conn_params;  // map of connection parameters
    BoltState state;        // the state of connection

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;
    BoltView view;
    
    std::string message_string;             
    std::string err_string;

    // utilities
    void Close_Driver();
    void Encode_Pull(const int n);

    

    int Poll_Readable();
    void Poll_Writable();
    void Flush();

    int Decode_Response(u8* view, const size_t bytes);
    bool Recv_Completed(const size_t bytes);

    int Negotiate_Version();
    int Send_Hellov4();
    int Send_Hellov5();
    int Send_Hello();

    inline bool Handle_Hello();

    int DummyS(u8* view, const size_t bytes);
    int Success_Hello(u8* view, const size_t bytes);
    int Success_Run(u8* view, const size_t bytes);
    int Success_Pull(u8* view, const size_t bytes);
    int Success_Record(u8* view, const size_t bytes);

    void DummyF(u8* view, const size_t bytes);
    void Fail_Hello(u8* view, const size_t bytes);
    void Fail_Run(u8* view, const size_t bytes);
    void Fail_Pull(u8* view, const size_t bytes);
    void Fail_Record(u8* view, const size_t bytes);


    using Success_Fn = int (NeoConnection::*)(u8*, const size_t);
    using Fail_Fn = void (NeoConnection::*)(u8*, const size_t);

    Success_Fn success_handler[DRIVER_STATES]{ 
        &NeoConnection::DummyS,
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_Hello,
        &NeoConnection::DummyS, 
        &NeoConnection::Success_Run,
        &NeoConnection::Success_Pull,
		&NeoConnection::Success_Record,
    };

    Fail_Fn fail_handler[DRIVER_STATES]{ 
        &NeoConnection::DummyF,
        &NeoConnection::Fail_Hello,
        &NeoConnection::Fail_Hello,
        &NeoConnection::DummyF,
        &NeoConnection::Fail_Run,
		&NeoConnection::Fail_Pull,
		&NeoConnection::Fail_Record,
	};
};