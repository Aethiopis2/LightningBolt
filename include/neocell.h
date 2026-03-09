/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 27th of Feburary 2026, Friday
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
    int Get_Retry_Count() const;
    int Get_Max_Retry_Count() const;
    int Get_ClientID() const;

    u64 Percentile(double p) const;
    u64 Wall_Latency() const;

    bool Can_Retry();
    bool Is_Connected() const;
    std::string Get_Last_Error() const;

    void Stop();
    void Clear_Histo();
    void Set_Max_Retry_Count(const int n);
    void Reset_Retry();

private:

	int retry_count;        // number of connection attempts, resets on successful connection or exhaustion
    int max_retries;        // the maximum number of retries allowed; default to 5
    int leftover_bytes;     // leftover bytes from previous decode
    int epfd;               // epoll descriptor

	std::atomic<int> last_rc;   // store's the last return value which maybe an error
    std::string err_desc;       // a string version of last error occured either from neo4j or internal

    NeoConnection connection;               // a connection instance; either standalone or routed
    LockFreeQueue<CellCommand> requests;    // queue of requests, allows for retry.
   

	void Consume_Read_Buffer(const size_t bytes);
	void Reset_Read_Buffer();
    u8* Get_Read_Buffer_Read_Ptr();

    LBStatus Handshake(const int id);
    LBStatus Poll_Read();
    LBStatus Execute_Command(CellCommand& cmd);
	LBStatus Decode_Response(u8* ptr, const size_t bytes);
};