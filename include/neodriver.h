/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday.
 * @date updated 4th of March 2026, Wednesday.
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neopool.h"



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
constexpr static int MAX_EVENTS = 1024;
constexpr static int POOL_SIZE = 1;


enum class DbMode
{
    Read,
    Write
};

struct Session
{
    std::string query;
    std::vector<std::string> bookmarks;
    std::string db;
    DbMode mode;
};

//===============================================================================|
//          CLASS
//===============================================================================|
class NeoDriver
{
public:

    NeoDriver(const std::string& urls, BoltValue auth,
        BoltValue extra = BoltValue::Make_Map(),
        const int pool_size_ = POOL_SIZE);
    ~NeoDriver();

    LBStatus Execute_Async(std::function<void(BoltResult&)> cb, const char* query,
        BoltValue&& params = BoltValue::Make_Map(), BoltValue&& extra = BoltValue::Make_Map());
    LBStatus Execute(const char* query, BoltValue&& params = BoltValue::Make_Map(),
        BoltValue&& extra = BoltValue::Make_Map());
    int Fetch(BoltResult& result);

    void Close();
    void Set_Pool_Size(const int nsize);
    int Get_Pool_Size() const;

    std::string Get_Last_Error() const;
    NeoCell* Get_Session();
    NeoCellPool* Get_Pool();

private:

    std::string _urls;       // raw unfiltered url string for database connection
    BoltValue _auth;         // authentication token
    BoltValue _extras;       // any extra connection params (power user mode, not me).

    int pool_size;
    int epfd;                   // event file descriptor for polling
    int exit_fd;                // used for exits in epoll
    u64 next_client_id;         // id for the next connection in the pool
    LBStatus last_rc;           // store's the last return value which maybe an error
    epoll_event events[MAX_EVENTS];   // epoll events array

    std::string last_err;       // last error string 
    std::thread poll_thread;    // polls ready connections
    std::atomic<bool> looping;

    NeoCellPool* pool;          // pointer to an instance of pool

    void Poll_Read();

    struct RouteTable
    {
        std::string writer;     // address to leader in a cluster
        std::vector<std::string> readers;   // bunch of replica + followers in a cluster
        std::vector<std::string> routes;    // redundant from our perpective but can mask out replicas
        std::string database;               // which database is in the cluster
        s64 ttl;                // time to live, route refresh rate as defined by server
    };

    RouteTable route_table;     // instance
};