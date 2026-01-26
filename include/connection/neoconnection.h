/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 18th of January 2026, Sunday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/tcp_client.h"
#include "bolt/bolt_decoder.h"
#include "bolt/bolt_encoder.h"
#include "bolt/decoder_task.h"
#include "utils/lock_free_queue.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * @brief neo4j server version info, I only care of major and minor ones;
 *  appears in reverse order in little-endian.
 */
struct Neo4jVerInfo
{
    u8 reserved[2];
    u8 minor;
    u8 major;

    float Get_Version() const
    {
        return static_cast<float>(major) + (static_cast<float>(minor) / 10.0f);
    } // end Get_Version
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

    NeoConnection(const std::string& urls, BoltValue* pauth, BoltValue* pextras);
    ~NeoConnection();

    int Init(const int cli_id = -1);
    int Reconnect();
    int Run(const char* cypher, BoltValue params = BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1,
        std::function<void(BoltResult&)> rscb = nullptr);
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
    void Set_Callbacks(std::function<void(BoltResult&)> rscb);

    std::string Get_Last_Error() const;
    std::string State_ToString() const;

private:

    // connection paramters; kept inside driver
    BoltValue* pauth;       // authentication token
    BoltValue* pextras;     // extra connection parameters

    int client_id;          // connection identifier
    int tran_count;		    // number of transactions executed; simulates nesting
    bool is_open;           // connection flag
    size_t bytes_recvd;     // helper that store's the number of bytes to decode
    std::string err_string; // store's a dump of error

    LockFreeQueue<DecoderTask> tasks;       // queue of pipelined query responses
    LockFreeQueue<BoltResult> results;      // queue of query reults decoded
    Neo4jVerInfo supported_version;         // holds major and minor versions for server

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

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
    inline int Dummy(DecoderTask& task, int& skip);
    inline int Success_Hello(DecoderTask& task, int& skip);
    inline int Success_Run(DecoderTask& task, int& skip);
    inline int Success_Record(DecoderTask& task, int& skip);
    inline int Success_Reset(DecoderTask& task, int& skip);

    inline int Handle_Record(DecoderTask& task, int& skip);
    inline int Handle_Failure(DecoderTask& task, int& skip);
    inline int Handle_Ignored(DecoderTask& task, int& skip);

    void Encode_Pull(const int n);

    BoltMessage Routev43(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database,
        const BoltValue& extra);
    BoltMessage Routev42(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database);
    BoltMessage Route_Legacy(const BoltValue& routing);

    using Success_Fn = int (NeoConnection::*)(DecoderTask&, int&);
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