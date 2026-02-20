/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday
 * @date updated 10th of Feburary 2026, Tuesday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neopool.h"



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
constexpr int MAX_EVENTS = 1024;


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
        const int pool_size_ = 0);
    ~NeoDriver();

    int Execute(std::string& query, std::map<std::string, std::string> params = {});
    int Execute_Async(std::string query, std::function<void(BoltResult&)> cb,
        std::map<std::string, std::string> params = {});
    int Fetch(BoltResult& result);

    void Close();
    void Set_Pool_Size(const int nsize);
    int Get_Pool_Size() const;

    std::string Get_Last_Error() const;
    NeoCell* Get_Session();
    NeoCellPool* Get_Pool();

private:

    std::string urls;       // raw unfiltered url string for database connection
    BoltValue auth;         // authentication token
    BoltValue extras;       // any extra connection params (power user mode, not me).

    int pool_size;
    int epfd;                   // event file descriptor for polling
    int exit_fd;                // used for exits in epoll
    LBStatus last_rc;           // store's the last return value which maybe an error
    epoll_event events[MAX_EVENTS];   // epoll events array

    std::string last_err;       // last error string 
    std::thread poll_thread;    // polls ready connections
    std::atomic<bool> looping;

    NeoCellPool pool;           // instance of pool

    void Poll_Read();
    LBStatus Start_Session(NeoCell* pcell);

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