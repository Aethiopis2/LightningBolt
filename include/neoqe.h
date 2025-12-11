/**
 * @brief definition of NeoQE, Neo Query Engine. The QE can compose of multiple
 *  connections which are useful during routes and clustred server access.
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
#include "connection/neoconnection.h"
#include "utils/lock_free_queue.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * LightningBolt states over bolt
 */
enum class QueryState : u8 {
    Negotiation,    // driver is doing version negotiation 
    Logon,          // driver is sending logon info (v5.1+)     
    Ready,          // driver is ready for call and is idle
    Run,            // driver is expecting run success/fail message
    Pull,           // driver is expecting pull success/fail message
    Streaming,      // driver is in a streaming state; i.e. reading buffer
    Error,          // driver has encountered errors?  
};
constexpr int QUERY_STATES = 7;


//===============================================================================|
/**
 * @brief a view points at the next row/value to decode in a single
 *  bolt request/query. Since Bolt returns pipelined requests in the order
 *  sent, we can queue each response for processing in a ring buffer.
 */
//struct BoltView
//{
//    u8* cursor;                 // the current address
//    u64 offset;                 // the cursor offset
//    size_t size;                // the size of view into buffer
//    BoltMessage field_names;    // meta info on query usually the field names or labels
//    BoltMessage summary_meta;   // summary meta info trailing at the end of records
//};
//
//
////===============================================================================|
///**
// * @brief the result of a bolt query. It consists of three fields, the fields
// *  names for the query result, a list of records, and a summary detail, as per
// *  driver specs for neo4j
// */
//struct BoltResult
//{
//    BoltValue fields;           // the field names for the record
//    BoltValue records;          // the actual list of records returned
//    BoltValue summary;          // the summary message at end of records
//};

//===============================================================================|
//          CLASS
//===============================================================================|
class NeoQE
{
public:

    NeoQE(BoltValue connection_params);
    ~NeoQE();

    int Start();
    void Stop();

    /*int Run(const char* cypher, BoltValue params = BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1);
    int Fetch(BoltMessage& out);

    int Begin_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Commit_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Rollback_Transaction(const BoltValue& options = BoltValue::Make_Map());

    int Pull(const int n);
    int Reset();
    int Discard(const int n);
    int Telemetry(const int api);
    int Logoff();
    int Goodbye();
    int Ack_Failure();

    std::string Get_Last_Error() const;*/

private:

    bool cluster_mode;      // one of the two modes; true = routed clusters, false = standalone
    int num_queries;        // number of active pipelined queries
    int transaction_count;  // counts the number of active transactions before committing
    bool has_more;          // sentinel used to control rev loop
    ssize_t bytes_to_decode;    // helper to track bytes to decode
    std::string err_string; // application error info

    QueryState state;       // tracks the state of QE
    //BoltView view;          // slice of the current window into buffer

    // connections
    //NeoConnection writer;       // writer for cluster mode
    //LockFreeQueue<NeoConnection> readers;   // bunch of readers and replica's for readers


    //====================
    // utilities
    //====================
    /*bool Decode_Response(u8* view, const size_t bytes);
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
    };*/
};