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




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
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
    BoltMessage field_names;    // meta info on query usually the field names or labels
    BoltMessage summary_meta;   // summary meta info trailing at the end of records
};


/**
 * @brief the result of a bolt query. It consists of three fields, the fields
 *  names for the query result, a list of records, and a summary detail, as per
 *  driver specs for neo4j
 */
struct BoltResult
{
	BoltValue fields;           // the field names for the record
    BoltValue records;          // the actual list of records returned
    BoltValue summary;          // the summary message at end of records
};


/**
 * LightningBolt states over bolt
 */
enum class ConnectionState : u8 {
    Disconnected,   // driver is disconnected
    Connecting,     // driver is doing TCP connection + version negotiation 
    Logon,          // driver is sending logon info (v5.1+)     
    Ready,          // driver is ready for call and is idle
    Run,            // driver is expecting run success/fail message
    Pull,           // driver is expecting pull success/fail message
    Streaming,      // driver is in a streaming state; i.e. reading buffer
    Error,          // driver has encountered errors?  
};
constexpr int CONNECTION_STATES = 8;


// forward declation
class NeoCell;

//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * brief a connection abstraction. The main query engine, NeoCell, would get to
 *  use this object to send and recv from neo4j server instance. The class contains
 *  members is_open, a boolean flag that is true when connected to a server, an 
 *  optional client_id that is used for identification.
 * 
 * The class makes use of specialized buffer object BoltBuf and high speed
 *  encoder/decoder objects that allows it to sendd/recv from neo4j db via bolt.
 */
class NeoConnection : public TcpClient
{
    friend class NeoCell;

public: 

    NeoConnection();
	NeoConnection(const NeoConnection&) = default;
	NeoConnection& operator=(const NeoConnection&) = default;
	NeoConnection(NeoConnection&&) noexcept = default;
	NeoConnection& operator=(NeoConnection&&) noexcept = default;
    ~NeoConnection();
    
    int Init(BoltValue* params, const int cli_id = -1);
    int Reconnect();
    int Poll_Readable();
    int Get_Client_ID() const;

    bool Flush();
    bool Flush_And_Poll(BoltValue& v);
    bool Is_Closed() const override;

    void Terminate();
    void Set_ClientID(const int cli_id);
    void Set_State(const ConnectionState s);

    std::string Get_Last_Error() const;
    std::string State_ToString() const;

    ConnectionState Get_State() const;

private: 
    
    int client_id;          // connection identifier
    bool has_more;          // sentinel for recv loop
    size_t bytes_recvd;     // helper that store's the number of bytes to decode
    float sversion;         // copy of supported version
    std::string err_string; // store's a dump of error
    std::atomic<ConnectionState> state;       // state of connection


    // storage buffers
    BoltBuf read_buf;
    BoltBuf write_buf;
    BoltView view;              // slice of the current window into buffer
    BoltValue* conn_params;     // pointer to connection parameters

    BoltEncoder encoder;
    BoltDecoder decoder;

            
    //====================
    // utilities
    //====================
    bool Poll_Writable();
    bool Recv_Completed();
    bool Decode_Response(u8* view, const size_t bytes);
    bool Is_Record_Done(BoltMessage& v);

    float Negotiate_Version();
    int Send_Hellov5();
    int Send_Hellov4();

    // state based handlers
    inline int Dummy(u8* view, const size_t size);
    inline int Success_Hello(u8* view, const size_t size);
    inline int Success_Run(u8* view, const size_t size);
    inline int Success_Pull(u8* view, const size_t size);
    inline int Success_Record(u8* view, const size_t size);
    inline int Success_Reset(u8* view, const size_t size);

    inline int Fail_Hello(u8* view, const size_t size);
    inline int Fail_Run(u8* view, const size_t size);
    inline int Fail_Pull(u8* view, const size_t size);
    inline int Fail_Record(u8* view, const size_t size);
    inline int Fail_Reset(u8* view, const size_t size);
	inline int Handle_Ignored(u8* view, const size_t size);

    inline void Decode_Error(u8* view, const size_t size);
    

    using Success_Fn = int (NeoConnection::*)(u8*, const size_t);
    using Fail_Fn = int (NeoConnection::*)(u8*, const size_t);

    Success_Fn success_handler[CONNECTION_STATES]{
        &NeoConnection::Dummy,
        &NeoConnection::Success_Hello,
        &NeoConnection::Success_Hello,
        &NeoConnection::Dummy,
        &NeoConnection::Success_Run,
        &NeoConnection::Success_Pull,
        &NeoConnection::Success_Record,
        &NeoConnection::Success_Reset,  // error?
    };

    Fail_Fn fail_handler[CONNECTION_STATES]{
        &NeoConnection::Dummy,
        &NeoConnection::Fail_Hello,
        &NeoConnection::Fail_Hello,
        &NeoConnection::Dummy,
        &NeoConnection::Fail_Run,
        &NeoConnection::Fail_Pull,
        &NeoConnection::Fail_Record,
		&NeoConnection::Fail_Reset,  // error?
    };
};