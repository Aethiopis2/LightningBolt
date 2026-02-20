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
 * @brief authentication schemes supported by neo4j server and understood by the cell.
 */
static constexpr char* SCHEME_BASIC = "basic";
static constexpr char* SCHEME_KERBEROS = "kerberos";
static constexpr char* SCHEME_BEARER = "bearer";
static constexpr char* SCHEME_NONE = "none";

static constexpr char* SCHEME_STRING = "scheme";
static constexpr char* PRINCIPAL_STRING = "principal";
static constexpr char* CREDENTIALS_STRING = "credentials";
static constexpr char* EXTRAS_STRING = "extras";


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


/**
 * @brief helper functions to create authentication tokens for different
 *  schemes supported by neo4j server. The functions return a BoltValue
 *  of type Map with the right keys and values for the given scheme.
 */
namespace Auth
{
    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  basic authentication scheme supported by neo4j server. The map contains
     *  the following keys: "scheme", "principal" and "credentials" with their
     *  corresponding values.
     * 
	 * @param user the username for basic authentication
	 * @param password the password for basic authentication
     * 
	 * @return a BoltValue of type Map with the right keys and values for basic
	 */
    inline BoltValue Basic(const char* user, const char* password)
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_BASIC),
            mp(PRINCIPAL_STRING, user),
            mp(CREDENTIALS_STRING, password)
            });
    } // end Basic


    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  kerberos authentication scheme supported by neo4j server. The map contains
     *  the following keys: "scheme" and "credentials" with their corresponding
     *  values.
     * 
	 * @param base64_ticket a base64 encoded kerberos ticket for authentication
     * 
	 * @return a BoltValue of type Map with the right keys and values for kerberos
	 */
    inline BoltValue Kerberos(const char* base64_ticket)
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_KERBEROS),
            mp(CREDENTIALS_STRING, base64_ticket)
            });
	} // end Kerberos


    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  bearer authentication scheme supported by neo4j server. The map contains
     *  the following keys: "scheme" and "credentials" with their corresponding
     *  values.
     * 
	 * @param token a bearer token for authentication
     * 
	 * @return a BoltValue of type Map with the right keys and values for bearer
	 */
    inline BoltValue Bearer(const char* token)
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_BEARER),
            mp(CREDENTIALS_STRING, token)
            });
    } // end Bearer


    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  none authentication scheme supported by neo4j server. The map contains
     *  the following key: "scheme" with its corresponding no value.
     * 
	 * @return a BoltValue of type Map with the right keys and values for none
	 */
    inline BoltValue None()
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_NONE)
            });
	} // end None
} // end AuthToken


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
    u64 Get_Connection_Time() const;
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
    std::atomic<bool> esleep;   // when true encoder thread is sleeping.
    std::atomic<bool> dsleep;   // when true it as well means decoder thread is sleeping
    std::string last_error;     // a string version of last error occured either from neo4j or internal

    std::thread encoder_thread; // writer thread id
    std::thread decoder_thread; // reader therad id

    NeoConnection connection;           // a connection instance; either standalone or routed
    
    s64 connect_duration;               // microseconds it took to tcp connect +/- ssl connect + version negotiation + HELLO +/- LOGON
    LockFreeQueue<CellCommand> equeue;  // request queue for the cell

    void Encoder_Loop();
    void Decoder_Loop();
    void EWake();
    void Sleep(std::atomic<bool>& s);
    void Set_Running(const bool state);

    bool Is_Running() const;
};