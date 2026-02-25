/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 20th of Feburary 2026, Friday
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
 * @brief command types for my cellular model
 */
enum class CellCmdType
{
    Run,
    Begin,
    Commit,
    Rollback,
    Pull,
    Discard,
    Reset,
    Logoff,
};


/**
 * @brief command types and their corresponding parameters understood by the cell.
 *  The structure is meant to capture the layout of different API's offered by
 *  the connection object.
 */
struct CellCommand
{
    CellCmdType type;       // the command types, see enum above

    std::string cypher;     // the query string in relation to run command
    BoltValue Routes;       // list of routes for route
    int n = -1;             // size for fetching
    BoltValue params = BoltValue::Make_Map();   // params for run, begin, commit and rollback
    BoltValue extras = BoltValue::Make_Map();   // params for run

    std::function<void(BoltResult&)> cb = nullptr;    // a callback for async procs ideal for web apps.
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCell
{
    friend LBStatus LB_Handle_Status(LBStatus, NeoCell*);

public:

    NeoCell(const std::string& urls, BoltValue* pauth, BoltValue* pextras);
    ~NeoCell();

    LBStatus Start(const int id = 1);
    LBStatus Run(const std::string& cypher, BoltValue&& param);

    int Enqueue_Request(CellCommand&& cmd);
    int Fetch(BoltResult& result);
    int Get_Socket() const;
    int Get_Try_Count() const;
    int Get_Max_Try_Count() const;

    u64 Percentile(double p) const;
    u64 Wall_Latency() const;

    bool Is_Connected() const;
    std::string Get_Last_Error() const;

    void Stop();
    void DWake();
    void Clear_Histo();
	bool Can_Retry();
    void Set_Retry_Count(const int n);

private:

	int try_count;          // number of connection attempts, resets on successful connection or exhaustion
    int max_tries;          // the maximum number of retries allowed; default to 5

    std::atomic<bool> running;  // thread loop controller
    std::atomic<int> esleep;   // when true encoder thread is sleeping.
    std::atomic<int> dsleep;   // when true it as well means decoder thread is sleeping
    std::string last_error;     // a string version of last error occured either from neo4j or internal

    std::thread encoder_thread; // writer thread id
    std::thread decoder_thread; // reader therad id

    NeoConnection connection;           // a connection instance; either standalone or routed
    LockFreeQueue<CellCommand> equeue;  // request queue for the cell
    LockFreeQueue<DecoderTask> tasks;   // queue of pipelined query responses
	LockFreeQueue<BoltResult> rqueue;   // queue of results ready to be fetched by the user


    void Encoder_Loop();
    void Decoder_Loop();
    void EWake();
    void Sleep(std::atomic<int>& s);
    void Set_Running(const bool state);

    bool Is_Running() const;
	LBStatus Decode_Response(u8* ptr, const size_t bytes);
};