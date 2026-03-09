/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 27th of Feburary 2026, Friday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/tcp_client.h"
#include "bolt/bolt_decoder.h"
#include "bolt/bolt_encoder.h"
#include "bolt/decoder_task.h"
#include "bolt/bolt_auth.h"
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

    LBStatus Negotiate_Version();
    LBStatus Send_Hellov5(TaskState& state);
    LBStatus Send_Hellov4(TaskState& state);
    LBStatus Logon(TaskState& state);
    LBStatus Run(const char* cypher, 
        const BoltValue& params, 
        const BoltValue& extras, 
        const int chunks,
        std::function<void(BoltResult&)> cb = nullptr);
    LBStatus Begin(const BoltValue& options = BoltValue::Make_Map());
    LBStatus Commit(const BoltValue& options = BoltValue::Make_Map());
    LBStatus Rollback(const BoltValue& options = BoltValue::Make_Map());

    int Pull(const int n);
    int Discard(const int n);
    int Telemetry(const int api);
    LBStatus Reset();
    LBStatus Logoff();
    int Goodbye();
    int Ack_Failure();
    int Route(BoltValue routing,
        BoltValue bookmarks = BoltValue::Make_List(),
        const std::string& database = "neo4j",
        BoltValue extra = BoltValue::Make_Map());

    void Terminate();
    void Set_ClientID(const int cli_id);
    void Set_Host_Address(const std::string& host, const std::string& port);

private:

    // connection paramters; kept inside driver
    BoltValue* pauth;       // authentication token
    BoltValue* pextras;     // extra connection parameters

    int client_id;          // optional connection identifer
    int tran_count;		    // number of transactions executed; simulates nesting
	int current_msg_len;    // length of the current message being decoded; used for partial decoding
    int unconsumed_count;   // prevents infinite loops due to Compact and Consume stalls

    bool recv_paused;           // have we paused recv because of mem issues?
    std::atomic<bool> is_done;  // used to notifiy whenever a streaming batch is ready

    LockFreeQueue<DecoderTask> tasks;   // queue of pipelined query requests & responses
    LockFreeQueue<BoltResult> results;  // queue of results ready to be fetched by the user

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;

    Neo4jVerInfo supported_version; // holds major and minor versions for server
    LatencyHistogram latencies;     // latency measurement structure


    //====================
    // utilities
    //====================
    LBStatus Poll_Writable();
    LBStatus Poll_Readable();
    LBStatus Decode_One(DecoderTask& task);
    LBStatus Can_Decode(u8* view, const u32 bytes_remain);
    int Get_Client_ID() const;
    LBStatus Flush();

    bool Is_Record_Done(BoltMessage& summary);
    LBStatus Encode_And_Flush(TaskState s, BoltMessage& v);

    // state based handlers
    inline LBStatus Success_None(DecoderTask& task);
    inline LBStatus Success_Hello(DecoderTask& task);
    inline LBStatus Success_Run(DecoderTask& task);
    inline LBStatus Success_Record(DecoderTask& task);
    inline LBStatus Success_Reset(DecoderTask& task);

    inline LBStatus Handle_Record(DecoderTask& task);
    inline LBStatus Handle_Failure(DecoderTask& task);
    inline LBStatus Handle_Ignored();

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
        &NeoConnection::Success_None,      //simply passes over
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_None,      // contains no meta info
        &NeoConnection::Success_Run,
        &NeoConnection::Handle_Record,
        &NeoConnection::Success_Record,
        &NeoConnection::Success_Reset,  // error?
    };
};