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
#include <cmath>
#include "connection/neoconnection.h"
#include "bolt/bolt_result.h"




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


/**
 * @brief holds the state of a pipelined query along with its view into
 *  the buffer and the decoded results that point right at that buffer
 *  which can be shallow copied to a caller.
 */
struct DecoderTask
{
    QueryState state;       // current state of the query
    BoltView view;          // view into the buffer for this query
    BoltResult result;      // results of bolt values
	int prev_bytes{ 0 };    // bytes left over from the previous batch, used for streaming queries
	bool is_done{ false };  // indicates if the task is done processing, used for streaming queries 

    std::chrono::_V2::system_clock::time_point start_clock = 
        std::chrono::high_resolution_clock::now();  // starting point for timer, always now!
    std::function<void(BoltResult&)> cb = nullptr;  // a callback for async procs ideal for web apps.

    DecoderTask() = default;
    DecoderTask(QueryState s) : state(s) { }
	DecoderTask(QueryState s, std::function<void(BoltResult&)> c) : state(s), cb(c) {}
    DecoderTask(const DecoderTask&) = delete;
    DecoderTask(DecoderTask&&) = default;

    DecoderTask& operator=(DecoderTask&& other) = default;
};