/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday.
 * @date updated 22nd of March 2026, Sunday.
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/neoconnection.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * @brief command types and their corresponding parameters understood by the cell.
 *  The structure is meant to capture the layout of different API's offered by
 *  the connection object.
 */
struct CellCommand
{
    CellCmdType type;       // the command types, see enum above

    const char* cypher;     // the query string in relation to run command
    int n = -1;             // size for fetching

    BoltValue Routes;       // list of routes for route
    BoltValue param = BoltValue::Make_Map();   // params for run, begin, commit and rollback
    BoltValue extra = BoltValue::Make_Map();   // params for run
    BoltResult result;      // gets the result here
    std::function<void(BoltResult&)> cb;       // callback for async

    // constructors
    CellCommand() = default;
    CellCommand(CellCmdType tp) : type(tp) {}
};

// forwards
class NeoDriver;

//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCell
{
	friend class NeoDriver;
    friend LBStatus LB_Handle_Status(LBStatus, NeoCell*);

public:

    NeoCell(int epfd_, const std::string& urls, BoltValue* pauth, BoltValue* pextras);
    ~NeoCell();

    LBStatus Start_Session(const int id = 1);
    LBStatus Run_Async(std::function<void(BoltResult&)> cb,
        const char* query,
        BoltValue&& param = BoltValue::Make_Map(), 
        BoltValue&& extra = BoltValue::Make_Map());
    LBStatus Run(const char* query, BoltValue&& param = BoltValue::Make_Map(),
        BoltValue&& extra = BoltValue::Make_Map());
    LBStatus Fetch(BoltResult& result);

    int Get_Socket() const;
    int Get_Connection_Retry_Count() const;
    int Get_Max_Connection_Retry_Count() const;
    int Get_Request_Retry_Count() const;
    int Get_Max_Request_Retry_Count() const;
    int Get_ClientID() const;

    u64 Percentile(double p) const;
    u64 Avg_Latency() const;

    bool Can_Retry_Connect();
    bool Can_Retry_Request();
    bool Should_Wait() const;
    bool Is_Connected() const;
    std::string Get_Last_Error() const;

    void Stop();
    void Clear_Histo();
    void Set_Max_Connection_Retry_Count(const int n);
    void Reset_Connection_Retry();
    void Set_Max_Request_Retry_Count(const int n);
    void Reset_Request_Retry();
    void Set_Wait(const bool wait = false);
    void Wait_Response();
    void Add_Ref();
    void Sub_Ref();

private:

	int connection_retry_count; // number of connection attempts, resets on successful connection or exhaustion
    int max_connection_retries; // the maximum number of retries allowed; default to 12
    int req_retry_count;        // retry count used for neo4j requests
    int max_req_retries;        // total allowed number of retries for a request
    int leftover_bytes;         // leftover bytes from previous decode
    int epfd;                   // epoll descriptor

	std::atomic<int> last_rc;   // store's the last return value which maybe an error
    std::atomic<s64> resp_ref;  // tracks responses and used to control flow viz wait()...notify_one().
    std::string err_desc;       // a string version of last error occured either from neo4j or internal

    NeoConnection connection;               // a connection instance; either standalone or routed
    LockFreeQueue<CellCommand> requests;    // queue of requests, allows for retry.
    LatencyHistogram latencies;             // latency measurement structure
   
	void Consume_Read_Buffer(const size_t bytes);
	void Reset_Read_Buffer();
    u8* Get_Read_Buffer_Read_Ptr();

    inline bool Can_Retry(int& _count, const int max_count);

    LBStatus Poll_Read();
    LBStatus Execute_Command(CellCommand& cmd);
	LBStatus Decode_Response(u8* ptr, const size_t bytes);
};