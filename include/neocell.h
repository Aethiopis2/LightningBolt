/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 18th of January 2026, Sunday
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
    BoltValue params = BoltValue::Make_Map();   // params for run, begin, commit and rollback
    BoltValue extras = BoltValue::Make_Map();   // params for run
    BoltValue Routes;       // list of routes for route
    int n = -1;             // size for fetching

    ResultCallback cb = nullptr;    // a callback for async procs ideal for web apps.
    EncodeCallback ecb = nullptr;   // a callback to encoder
};


namespace Auth
{
    inline BoltValue Basic(const std::string& user, const std::string& password)
    {
        return BoltValue({
            mp("scheme", "basic"),
            mp("principal", user.c_str()),
            mp("credentials", password.c_str())
            });
    } // end Basic
} // end AuthToken


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCell
{
public:

    NeoCell(const std::string& urls, BoltValue* pauth, BoltValue* pextras);
    ~NeoCell();

    int Start(const int id = 1);
    int Enqueue_Request(CellCommand&& cmd);
    int Fetch(BoltResult& result);

    void Stop();
    std::string Get_Last_Error() const;

private:

    int read_ret;           // store's return codes from read thread
    int write_ret;          // return values from the corresponding write

    std::atomic<bool> running;  // thread loop controller
    std::atomic<bool> esleep;   // when true encoder thread is sleeping.
    std::atomic<bool> dsleep;   // when true it as well means decoder thread is sleeping
    std::atomic<bool> twait;    // decoder task wait, when true thread should wait
    std::thread encoder_thread; // writer thread id
    std::thread decoder_thread; // reader therad id

	NeoConnection connection;           // a connection instance; either standalone or routed
	LockFreeQueue<CellCommand> equeue;  // request queue for the cell
    
	void Encoder_Loop();
	void Decoder_Loop();
    void EWake();
    void DWake();
    void Sleep(std::atomic<bool>& s);
    void Wait_Task();
    void Set_Running(const bool state);

    bool Is_Running() const;
};