/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 9th of April 2025, Wednesday.
 * @date updated 20th of April 2026, Monday.
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



// forwards
class NeoDriver;
class NeoSession;
class NeoRouter;



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
    friend class NeoDriver;
	friend class NeoSession;
    friend class NeoRouter;

public:

	NeoConnection(bool ssl_enabled,
        const std::string& urls, BoltValue* pauth, BoltValue* pextras);
    ~NeoConnection();

    LBStatus Connect_Neo4j(const int epfd_, const int id);
    LBStatus Decode_Response(u8* ptr, const size_t bytes);

	LBStatus Run(RequestCommand& command);
    LBStatus Begin(RequestCommand& command);
    LBStatus Commit(RequestCommand& command);
    LBStatus Rollback(RequestCommand& command);
    LBStatus Pull(RequestCommand& command);
    LBStatus Discard(RequestCommand& command);
    LBStatus Telemetry(RequestCommand& command);
    LBStatus Reset(RequestCommand& command);
    LBStatus Logoff(RequestCommand& command);
    LBStatus Ack_Failure(RequestCommand& command);
    LBStatus Route(RequestCommand& command);
    LBStatus Goodbye();

    void Terminate();
    void Set_Host_Address(const std::string& host, const std::string& port);
    void Wait_For_Response();

    std::string Get_Hostname() const;
    std::string Get_Port() const;
    std::string Get_Last_Error() const;

private:

    int epfd;               // epoll descriptor
    int client_id;          // optional connection identifer
    int tran_count;		    // number of transactions executed; simulates nesting
    int current_msg_len;    // length of the current message being decoded; used for partial decoding
    int unconsumed_count;   // prevents infinite loops due to Compact and Consume stalls
    int leftover_bytes;     // leftover bytes from previous decode

    bool recv_paused;               // have we paused recv because of mem issues?
    std::atomic<bool> should_wait;  // used to hack the startup on auto retries to avoid waits!
    std::atomic<s64> sync_count;    // tracks responses and used to control flow viz wait()...notify_one().
	BoltResult last_err;    // holds the last error result for retrieval by session

    // connection paramters; kept inside driver
    BoltValue* pauth;       // authentication token
    BoltValue* pextras;     // extra connection parameters

    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;

    BoltEncoder encoder;
    BoltDecoder decoder;
	
    LatencyHistogram latencies;         // latency measurement structure
    Neo4jVerInfo supported_version;     // holds major and minor versions for server

    LockFreeQueue<RequestCommand> commands;  // queue of pending commands for encoding
	LockFreeQueue<DecoderTask> tasks;   // queue of pipelined query tasks for decoding
	LockFreeQueue<BoltResult> results;  // queue of decoded query results for consumption by session


    //====================
    // utilities
    //====================
    bool Is_Record_Done(BoltMessage& summary);

    LBStatus Negotiate_Version();
    LBStatus Handshake(const int epfd, void* pobj, const int id);
    LBStatus Send_Hellov5();
    LBStatus Send_Hellov4();
    LBStatus Logon();

    LBStatus Poll_Writable();
    LBStatus Poll_Readable();
    LBStatus Decode_One(DecoderTask& task);
    LBStatus Can_Decode(u8* view, const u32 bytes_remain);
    LBStatus Flush();
    LBStatus Encode_And_Flush(TaskState s, RequestCommand& command, BoltMessage& v);
    LBStatus Enqueue_Task(TaskState s);
    LBStatus Retry_Encode(BoltMessage&);

    // state based handlers
    LBStatus Success_None(DecoderTask& task);
    LBStatus Success_Hello(DecoderTask& task);
    LBStatus Success_Run(DecoderTask& task);
    LBStatus Success_Record(DecoderTask& task);
    LBStatus Success_Reset(DecoderTask& task);

    LBStatus Handle_Record(DecoderTask& task);
    LBStatus Handle_Failure(DecoderTask& task);
    LBStatus Handle_Ignored(DecoderTask& task);

    void Encode_Pull(const int n);
    void Add_Sync_Count();
	void Sub_Sync_Count();

    BoltMessage Routev43(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database,
        const BoltValue& extra);
    BoltMessage Routev42(const BoltValue& routing,
        const BoltValue& bookmarks,
        const std::string& database);
    BoltMessage Route_Legacy(const BoltValue& routing);

	DecoderTask* Get_Next_Task(const size_t offset, const size_t size);

    // success handler table
    using Success_Fn = LBStatus (NeoConnection::*)(DecoderTask&);
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
        &NeoConnection::Handle_Ignored,     // deals with ignored tasks
    };
};