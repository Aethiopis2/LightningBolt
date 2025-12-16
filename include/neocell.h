/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 12th of Decemeber 2025, Friday
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/neoconnection.h"
#include "utils/lock_free_queue.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|



//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCell
{
public:

    NeoCell(BoltValue params);
    ~NeoCell();

    int Start();
    /*int Run(const char* cypher, BoltValue params = BoltValue::Make_Map(),
        BoltValue extras = BoltValue::Make_Map(), const int chunks = -1);
    int Fetch(BoltMessage& out);

    int Begin_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Commit_Transaction(const BoltValue& options = BoltValue::Make_Map());
    int Rollback_Transaction(const BoltValue& options = BoltValue::Make_Map());

    int Pull(const int n);
    int Reset();
    int Discard(const int n);
    int Telemetry(const int api);*/
    int Logoff();
    int Goodbye();
    //int Ack_Failure();

    std::string Get_Last_Error() const;
    std::string State_ToString() const;

    void Stop();


private:

    float sup_version;      // the supported bolt version set from server
    bool standalone_mode;   // one of the two modes; false = routed clusters, true = standalone
    bool has_more;          // sentinel used to control rev loop

    int cluster_count;      // count of clusters (0 based)
    int num_queries;        // number of active pipelined queries
    int transaction_count;  // counts the number of active transactions before committing

    std::string err_string; // application error info

    BoltValue conn_params;    // store's map of connection parameters for later use
    

    // connections
    NeoConnection writer;           // leader role in clustered mode or core connection standalone
    LockFreeQueue<NeoConnection> readers;   // bunch of connections for followers + replica roles

    struct RouteTable
    {
        std::string writer;     // address to leader in a cluster
        std::vector<std::string> readers;   // bunch of replica + followers in a cluster
        std::vector<std::string> routes;    // redundant from our perpective but can mask out replicas
        std::string database;               // which database is in the cluster
        s64 ttl;                // time to live, route refresh rate as defined by server
    };

    RouteTable route_table;     // instance

    //====================
    // utilities
    //====================
    void Encode_Pull(const int n);
};