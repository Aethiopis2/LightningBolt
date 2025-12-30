/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 12th of Decemeber 2025, Friday
 * 
 * @copyright Copyright (c) 2025
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/tcp_client.h"
#include "bolt/bolt_decoder.h"
#include "bolt/bolt_encoder.h"
#include "utils/lock_free_queue.h"




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
    BoltMessage fields;           // the field names for the record
    BoltMessage records;          // the actual list of records returned
    BoltMessage summary;          // the summary message at end of records
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
    BoltResult result;          // the result for this query
};


/**
 * @brief holds the state of a pipelined query along with its view into
 *  the buffer
 */
struct QueryInfo
{
    QueryState state;       // current state of the query
    BoltView view;          // view into the buffer for this query
};


/**
 * @brief neo4j server version info, I only care of major and minor ones;
 *  appears in reverse order in little-endian.
 */
struct Neo4jVerInfo
{
    u8 reserved[2];
    u8 minor;
    u8 major;
};


//===============================================================================|
//          CLASS
//===============================================================================|
// forward declation
class NeoCell;


/**
 * brief a connection abstraction. The main query engine, NeoCell, would get to
 *  use this object to send and recv from neo4j server instance. The class contains
 *  members is_open, a boolean flag that is true when connected to a server, an 
 *  optional client_id that is used for identification.
 * 
 * The class makes use of specialized buffer object BoltBuf and high speed
 *  encoder/decoder objects that allows it to send/recv from neo4j db via bolt.
 */
class NeoConnection : public TcpClient
{
    friend class NeoCell;

public: 

    explicit NeoConnection(BoltValue params);
    ~NeoConnection();
    
    int Init(const int cli_id = -1);
    int Reconnect();
    int Run(const char* cypher, BoltValue params = BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1);
    int Fetch(BoltMessage& out);

    int Begin(const BoltValue& options = BoltValue::Make_Map());
    int Commit(const BoltValue& options = BoltValue::Make_Map());
    int Rollback(const BoltValue& options = BoltValue::Make_Map());
    int Pull(const int n);
    int Discard(const int n);
    int Telemetry(const int api);
    int Reset();
    int Logoff();
    int Goodbye();
    int Ack_Failure();
    int Route(BoltValue routing,
        BoltValue bookmarks = BoltValue::Make_List(),
        const std::string& database = "neo4j",
        BoltValue extra = BoltValue::Make_Map());

    bool Is_Closed() const override;

    void Terminate();
    void Set_ClientID(const int cli_id);
	void Set_Host_Address(const std::string& host, const std::string& port);

    std::string Get_Last_Error() const;
    std::string State_ToString() const;

private: 
    
    int client_id;          // connection identifier
	int tran_count;		    // number of transactions executed; simulates nesting  
    bool has_more;          // sentinel for recv loop
	bool is_open;           // connection flag
    size_t bytes_recvd;     // helper that store's the number of bytes to decode

    std::string err_string; // store's a dump of error
	LockFreeQueue<QueryInfo> query_states;       // queue of pipelined queries
    Neo4jVerInfo supported_version;             // holds major and minor versions for server

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;
    BoltValue conn_params;     // pointer to connection parameters

    BoltEncoder encoder;
    BoltDecoder decoder;

            
    //====================
    // utilities
    //====================
    int Poll_Writable();
    int Poll_Readable();
    int Decode_Response(u8* view, const size_t bytes);
    int Get_Client_ID() const;
    int Send_Hellov5();
    int Send_Hellov4();

    bool Flush();
    bool Recv_Completed();
    bool Is_Record_Done(BoltMessage& v);
    bool Encode_And_Flush(QueryState s, BoltMessage& v);
    bool Negotiate_Version();

    // state based handlers
    inline int Dummy(BoltView& view);
    inline int Success_Hello(BoltView& view);
    inline int Success_Run(BoltView& view);
    inline int Success_Record(BoltView& view);
    inline int Success_Reset(BoltView& view);

    inline int Handle_Record(BoltView& view);
	inline int Handle_Failure(BoltView& view);
	inline int Handle_Ignored(BoltView& view);

    void Encode_Pull(const int n);

    BoltMessage Routev43(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database,
        const BoltValue& extra);
    BoltMessage Routev42(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database);
    BoltMessage Route_Legacy(const BoltValue& routing);

    using Success_Fn = int (NeoConnection::*)(BoltView&);
    Success_Fn success_handler[QUERY_STATES]{
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_Reset,      // contains no meta info
        &NeoConnection::Success_Run,
        &NeoConnection::Handle_Record,
        &NeoConnection::Success_Record,
        &NeoConnection::Success_Reset,  // error?
    };
};