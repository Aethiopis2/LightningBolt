/**
 * @file decoder_task.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief 
 * @version 1.0
 * @date 14th of May 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/neoconnection.h"
#include "bolt/bolt_message.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * LightningBolt states over bolt
 */
enum class QueryState : u8 {
    Connection,     // special 1: used during HELLO 
    Logon,          // special 2: used in v5.x+ after HELLO
    Logoff,         // special 3: expecting logoff success/fail message

    Run,            // driver is expecting run success/fail message
    Pull,           // driver is expecting pull success/fail message
    Streaming,      // driver is in a streaming state; i.e. reading buffer
    Discard,        // driver is expecting discard success/fail message
    Begin,          // driver is expecting begin trx success/fail message
    Commit,         // driver is expecting commit trx success/fail message
    Rollback,       // driver is expecting rollback trx success/fail message
    Route,          // driver is expecting route success/fail message
    Reset,          // driver is expecting reset success/fail message
    Telemetry,      // driver is expecting telemetry success/fail message
    Ack_Failure,    // driver is expecting ack_failure success/fail message
    Error,          // driver has encountered errors  
};
constexpr int QUERY_STATES = 15;



/**
 * @brief the result of a bolt query. It consists of three fields, the fields
 *  names for the query result, a list of records, and a summary detail, as per
 *  driver specs for neo4j
 */
struct BoltResult
{
    BoltMessage fields;     // the field names for the record
    std::vector<BoltValue> records;      // the actual list of records returned
    BoltMessage summary;    // the summary message at end of records
    BoltValue error = BoltValue::Make_Unknown();        // used when error occurs
    int messages{ 0 };      // count of messages contained within records
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
};


//struct ConnectionStat
//{
//    std::chrono::_V2::system_clock::time_point encode_time;
//    std::chrono::_V2::system_clock::time_point decode_time;
//    std::chrono::_V2::system_clock::time_point end;
//    u64 total_bytes_written{ 0 };
//    u64 total_bytes_read{ 0 };
//    double avg_bytes_written{ 0 };
//    double avg_bytes_read{ 0 };
//};