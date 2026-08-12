/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 14th of May 2025, Wednesday.
 * @date updated 20th of April 2026, Monday.
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
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
    Ignored,        // driver task has been ignored
};
constexpr int QUERY_STATES = 16;


/**
 * @brief command types for connection, helps preserve contexts for sessions and
 *  helps for retries and routing decisions. For example, if a connection receives a run command, it can
 *  use this information to determine how to handle the request and what state to expect in the response.
 */
enum class RequestCmdType
{
    Run,
    Begin,
    Commit,
    Rollback,
    Pull,
    Discard,
	Route,
    Reset,
    Logoff
};



enum class QueryMode : u8
{
    Auto,
    Read,
    Write
};


/**
 * @brief command types and their corresponding parameters understood by the connection.
 *  The structure is meant to capture the layout of different API's offered by
 *  the connection object.
 */
struct RequestCommand
{
    RequestCmdType type;       // the command types, see enum above
    QueryMode mode;

    int retry_count{ 0 };		// how many times we've retried for this request on failed attempts
    int n = -1;                 // size for fetching

    std::string cypher;     // the query string in relation to run command
    std::string database;   // database name for begin trx

    BoltValue routes;       // list of routes for route
    BoltValue param = BoltValue::Make_Map();   // params for run, begin, commit and rollback
    BoltValue extra = BoltValue::Make_Map();   // params for run
	BoltValue bookmark = BoltValue::Make_List();  // list of bookmarks for begin trx
	
    std::function<void(BoltResult&)> cb = nullptr;       // callback for async

    // constructors
    RequestCommand() = default;
    RequestCommand(RequestCmdType tp) : type(tp) {}
    RequestCommand(RequestCmdType tp, std::function<void(BoltResult&)>& callback) : type(tp), cb(std::move(callback)) {}
	RequestCommand(RequestCmdType tp, const char* c, int nfetch = -1, 
        BoltValue _param = BoltValue::Make_Map(), BoltValue _extra = BoltValue::Make_Map()) :
        type(tp), cypher(c), n(nfetch), param(std::move(param)), extra(std::move(_extra)) {}
};


/**
 * @brief a view points at the next row/value to decode in a single
 *  bolt request/query. Since Bolt returns pipelined requests in the order
 *  sent, we can queue each response for processing in a ring buffer.
 */
struct BoltView
{
	size_t start = -1;          // the start of the view into the buffer
    size_t offset = 0;         // the cursor offset
    size_t size = 0;        // the size of view into buffer

    size_t next_offset = 0;
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
	BoltResult result;      // decoded result for this query
    bool done{ false };         // when true, streaming is done and task can be dequeued.
    std::function<void(LBStatus, BoltResult&)> cb = nullptr;       // async callback into the session

    DecoderTask() = default;
    DecoderTask(TaskState s) : state(s) {}
    DecoderTask(TaskState s, std::function<void(LBStatus, BoltResult&)> fn) : state(s), cb(fn) {}
    DecoderTask(const DecoderTask&) = delete;
    DecoderTask(DecoderTask&&) = default;

    DecoderTask& operator=(DecoderTask&& other) = default;
};