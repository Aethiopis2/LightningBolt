/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 15th of Feburary 2026, Sunday
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
#include "utils/red_stats.h"




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

    LBStatus Init(const int cli_id = -1);
    LBStatus Reconnect();
    int Run(const char* cypher, BoltValue params = BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1,
        std::function<void(BoltResult&)> rscb = nullptr);

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

    void Terminate();
    void Set_ClientID(const int cli_id);
    void Set_Host_Address(const std::string& host, const std::string& port);
    void Set_Callbacks(std::function<void(BoltResult&)> rscb);

    std::string State_ToString() const;

private:

    // connection paramters; kept inside driver
    BoltValue* pauth;       // authentication token
    BoltValue* pextras;     // extra connection parameters

    int client_id;          // connection identifier
    int tran_count;		    // number of transactions executed; simulates nesting
    int prev_remaining = 0;

    std::atomic<bool> is_done;  // determines if the next decoding batch is done

    LockFreeQueue<DecoderTask> tasks;       // queue of pipelined query responses
    LockFreeQueue<BoltResult> results;      // queue of query reults decoded
    Neo4jVerInfo supported_version;         // holds major and minor versions for server

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;

    LatencyHistogram latencies;


    //====================
    // utilities
    //====================
    LBStatus Poll_Writable();
    LBStatus Poll_Readable();
    LBStatus Can_Decode(u8* view, const u32 bytes_remain);
    LBStatus Decode_Response(u8* view, const u32 bytes);
    int Get_Client_ID() const;
    LBStatus Send_Hellov5();
    LBStatus Send_Hellov4();
    LBStatus Flush();

    bool Is_Record_Done(DecoderTask& v);
    bool Encode_And_Flush(QueryState s, BoltMessage& v);
    LBStatus Negotiate_Version();

    // state based handlers
    inline LBStatus Success_Hello(DecoderTask& task);
    inline LBStatus Success_Run(DecoderTask& task);
    inline LBStatus Success_Record(DecoderTask& task);
    inline LBStatus Success_Reset(DecoderTask& task);

    inline LBStatus Handle_Record(DecoderTask& task);
    inline LBStatus Handle_Failure(DecoderTask& task);
    inline LBStatus Handle_Ignored(DecoderTask& task);

    void Encode_Pull(const int n);
    void Wait_Task();
    void Wake();

    BoltMessage Routev43(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database,
        const BoltValue& extra);
    BoltMessage Routev42(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database);
    BoltMessage Route_Legacy(const BoltValue& routing);

    LBStatus Retry_Encode(BoltMessage&);

    using Success_Fn = LBStatus (NeoConnection::*)(DecoderTask&);
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