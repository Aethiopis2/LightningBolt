/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday
 * @date updated 18th of January 2026, Sunday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neopool.h"



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
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
        BoltValue extra = BoltValue::Make_Map());
    ~NeoDriver();

    int Execute(std::string& query, std::map<std::string, std::string> params = {});
    int Execute_Async(std::string& query, ResultCallback cb,
        std::map<std::string, std::string> params = {});
    int Fetch(BoltResult& result);

    void Set_Pool_Size(const int nsize);
    int Get_Pool_Size() const;

private:

    std::string urls;       // raw unfiltered url string for database connection
    BoltValue auth;         // authentication token
    BoltValue extras;       // any extra connection params (power user mode, not me).

    int pool_size;
    std::thread poll_thread;    // polls ready connections
    std::atomic<bool> looping;

    NeoCellPool pool;           // instance of pool

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