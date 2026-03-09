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
 * In LightningBolt tasks have states that are used to track the progress of 
 *  pipelined queries. The states are used to determine what the driver is expecting
 *  from the peer and are used to define the action to take when a message is decoded. 
 *  For example, when the state is Run, the driver is expecting a run success/fail/ 
 *  ignored message and will decode accordingly.
 */
enum class TaskState : u8 {
    None,           // special 1: no task, can pop out easy almost same as done
	Hello,          // special 2: used to send HELLO message for v4.x and HELLO+LOGON for v5.x+
    Logon,          // special 3: used in v5.x+ after HELLO
    Logoff,         // special 4: expecting logoff success/fail message

    Run,            // driver is expecting run success/fail/ignored message
    Pull,           // driver is expecting pull success/fail message
    Record,         // driver is in a streaming state; i.e. reading buffer
    Discard,        // driver is expecting discard success/fail message
    Begin,          // driver is expecting begin trx success/fail message
    Commit,         // driver is expecting commit trx success/fail message
    Rollback,       // driver is expecting rollback trx success/fail message
    Route,          // driver is expecting route success/fail message
    Reset,          // driver is expecting reset success/fail message
    Telemetry,      // driver is expecting telemetry success/fail message
    Ack_Failure,    // driver is expecting ack_failure success/fail message
};
constexpr int QUERY_STATES = 16;


/**
 * @brief command types for my cellular model
 */
enum class CellCmdType
{
    Run,
    Begin,
    Commit,
    Rollback,
    Pull,
    Discard,
    Reset,
    Logoff,
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


/**
 * @brief holds the state of a pipelined query along with its view into
 *  the buffer and the decoded results that point right at that buffer
 *  which can be shallow copied to a caller.
 */
struct DecoderTask
{
    TaskState state;        // current state of the query
    BoltView view;          // view into the buffer for this query

    std::chrono::_V2::system_clock::time_point start_clock = 
        std::chrono::high_resolution_clock::now();  // starting point for timer, always now!
    std::function<void(BoltResult&)> cb = nullptr;  // a callback for async procs ideal for web apps.

    DecoderTask() = default;
    DecoderTask(TaskState s) : state(s) { }
	DecoderTask(TaskState s, std::function<void(BoltResult&)> c) : state(s), cb(c) {}
    DecoderTask(const DecoderTask&) = delete;
    DecoderTask(DecoderTask&&) = default;

    DecoderTask& operator=(DecoderTask&& other) = default;
};