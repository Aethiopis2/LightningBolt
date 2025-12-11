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
    Error,          // driver has encountered errors?  
};
constexpr int DRIVER_STATES = 8;

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
    BoltValue records = BoltValue::Make_List();          // the actual list of records returned
    BoltValue summary;          // the summary message at end of records
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoConnection : public TcpClient
{
public: 

    NeoConnection() = default;
    NeoConnection(BoltValue connection_params, const u64 cli_id = 0);
    ~NeoConnection();
    
    bool Is_Closed() const override;
    int Start();
    void Stop();

    int Run_Query(const char* cypher, BoltValue params=BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1);
    int Fetch(BoltMessage& out);

    int Begin_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Commit_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Rollback_Transaction(const BoltValue& options = BoltValue::Make_Map());

    int Pull(const int n);
    int Reset();
    int Discard(const int n = -1);
    int Telemetry(const int api);
    int Logoff();
    int Goodbye();
    int Ack_Failure();

    void Set_State(BoltState s);
    BoltState Get_State() const;
    u64 Get_Client_ID() const;
    std::string Get_Last_Error() const;
    std::string State_ToString() const;

private: 
    
    u64 client_id;              // connection identifier
    u64 current_qid;            // incremental identifier for queries
	int num_queries;            // number of active pipelined queries
    int transaction_count;      // counts the number of active transactions before committing
    bool has_more;              // sentinel used to control rev loop
	ssize_t bytes_to_decode;    // helper to track bytes to decode
    std::string err_string;     // application error info

    BoltValue conn_params;  // map of connection parameters
    BoltState state;        // the state of connection

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;
    BoltView view;          // slice of the current window into buffer
            
    //====================
    // utilities
    //====================
    void Close_Driver();
    
    bool Poll_Writable();
    int Poll_Readable();
    bool Flush();
    bool Decode_Response(u8* view, const size_t bytes);
    bool Recv_Completed(const size_t bytes);

    int Negotiate_Version();
    int Send_Hellov5();
    int Send_Hellov4();
    void Encode_Pull(const int n);

    inline void Dummy(u8* view, const size_t bytes);
    inline void Success_Hello(u8* view, const size_t bytes);
    inline void Success_Run(u8* view, const size_t bytes);
    inline void Success_Pull(u8* view, const size_t bytes);
    inline void Success_Record(u8* view, const size_t bytes);

    void DummyF(u8* view, const size_t bytes);
    void Fail_Hello(u8* view, const size_t bytes);
    void Fail_Run(u8* view, const size_t bytes);
    void Fail_Pull(u8* view, const size_t bytes);
    void Fail_Record(u8* view, const size_t bytes);

    inline bool Flush_And_Poll(BoltValue& v);

    using Success_Fn = void (NeoConnection::*)(u8*, const size_t);
    using Fail_Fn = void (NeoConnection::*)(u8*, const size_t);

    Success_Fn success_handler[DRIVER_STATES]{ 
        &NeoConnection::Dummy,
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_Hello,
        &NeoConnection::Dummy, 
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