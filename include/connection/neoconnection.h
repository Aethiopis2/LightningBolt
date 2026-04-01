/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday.
 * @date updated 22nd of March 2026, Sunday.
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
class NeoCell;


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
/**
 * brief NeoConnection is a TcpClient object that immplements Neo4j supported 
 *  messages using bolt protocol. It makes use of object BoltEncoder, BoltDecoder
 *  and a specialized cache-aligned adaptive buffer, BoltBuf, to encode or decode
 *  bolt packets over tcp stream ssl encrypted or not.
 * 
 * The object isn't generally meant to be used as is, as it delibertaly lacks
 *  state and complete control mechanisims for network flow.
 */
class NeoConnection : public TcpClient
{
    friend class NeoCell;

public:

    NeoConnection(const std::string& urls, BoltValue* pauth, BoltValue* pextras);
    ~NeoConnection();

    LBStatus Handshake(const int epfd, void* pobj, const int id);
    LBStatus Send_Hellov5();
    LBStatus Send_Hellov4();
    LBStatus Logon();
    LBStatus Run(const char* cypher, 
        const BoltValue& params, 
        const BoltValue& extras, 
        const int chunks);
    LBStatus Begin(const BoltValue& options = BoltValue::Make_Map());
    LBStatus Commit(const BoltValue& options = BoltValue::Make_Map());
    LBStatus Rollback(const BoltValue& options = BoltValue::Make_Map());

    LBStatus Pull(const int n);
    LBStatus Discard(const int n);
    LBStatus Telemetry(const int api);
    LBStatus Reset();
    LBStatus Logoff();
    LBStatus Goodbye();
    LBStatus Ack_Failure();
    LBStatus Route(BoltValue routing,
        BoltValue bookmarks = BoltValue::Make_List(),
        const std::string& database = "neo4j",
        BoltValue extra = BoltValue::Make_Map());

    void Terminate();
    void Set_Host_Address(const std::string& host, const std::string& port);

private:

    int client_id;          // optional connection identifer
    int tran_count;		    // number of transactions executed; simulates nesting
    int current_msg_len;    // length of the current message being decoded; used for partial decoding
    int unconsumed_count;   // prevents infinite loops due to Compact and Consume stalls

    bool recv_paused;           // have we paused recv because of mem issues?
    std::atomic<bool> should_wait;  // used to hack the startup on auto retries to avoid waits!

    // connection paramters; kept inside driver
    BoltValue* pauth;       // authentication token
    BoltValue* pextras;     // extra connection parameters

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;

    LockFreeQueue<DecoderTask> tasks;   // queue of pipelined query responses
    Neo4jVerInfo supported_version;     // holds major and minor versions for server


    //====================
    // utilities
    //====================
    bool Is_Record_Done(BoltMessage& summary);

    LBStatus Negotiate_Version();
    LBStatus Poll_Writable();
    LBStatus Poll_Readable();
    LBStatus Decode_One(BoltResult& result);
    LBStatus Can_Decode(u8* view, const u32 bytes_remain);
    LBStatus Flush();
    LBStatus Encode_And_Flush(TaskState s, BoltMessage& v);
    inline LBStatus Enqueue_Task(TaskState s);
    LBStatus Retry_Encode(BoltMessage&);

    // state based handlers
    inline LBStatus Success_None(BoltResult& task);
    inline LBStatus Success_Hello(BoltResult& task);
    inline LBStatus Success_Run(BoltResult& task);
    inline LBStatus Success_Record(BoltResult& task);
    inline LBStatus Success_Reset(BoltResult& task);

    inline LBStatus Handle_Record(BoltResult& task);
    inline LBStatus Handle_Failure(BoltResult& task);
    inline LBStatus Handle_Ignored(BoltResult& task);

    void Encode_Pull(const int n);

    BoltMessage Routev43(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database,
        const BoltValue& extra);
    BoltMessage Routev42(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database);
    BoltMessage Route_Legacy(const BoltValue& routing);


    // success handler table
    using Success_Fn = LBStatus (NeoConnection::*)(BoltResult&);
    Success_Fn success_handler[QUERY_STATES]
    {
        &NeoConnection::Success_None,       // TaskState::None - do nothing program or is done
        &NeoConnection::Success_Hello,      // TaskState::Hello - handles hello message
        &NeoConnection::Success_Hello,      // TaskState::Logon - handles logon message
        &NeoConnection::Success_None,       // TaskState::Logoff - do nothing
        &NeoConnection::Success_Run,        // TaskState::Run - handles SUCCESS message for RUN message
        &NeoConnection::Handle_Record,      // TaskState::Pull - handles RECORD message for PULL message
        &NeoConnection::Success_Record,     // TaskState::Record - handles SUCCESS for RECORD message
        &NeoConnection::Success_None,       // TaskState::Discard - SUCCESS for discard message
        &NeoConnection::Success_None,       // TaskState::Begin - success handler for BEGIN message
        &NeoConnection::Success_Reset,      // TaskState::Commit - success handler for COMMIT message
        &NeoConnection::Success_Reset,      // TaskState::Rollback - success handler for ROLLBACK message
        &NeoConnection::Success_Reset,      // TaskState::Route - success handler for ROUTE message
        &NeoConnection::Success_Reset,      // TaskState::Reset - success handler for RESET message
        &NeoConnection::Success_None,       // TaskState::Telemetry - do nothing handler for TELMETRY message
        &NeoConnection::Success_None,       // TaskState::Ack_Failure - do nothing handler for v1 Ack_Failure
    };
};