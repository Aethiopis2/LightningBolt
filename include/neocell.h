/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 12th of Decemeber 2025, Friday
 *
 * @copyright Copyright (c) 2025
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "connection/neoconnection.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
using ResultCallback = void(*)(int result, void* user);

struct RequestParams
{
    std::string cypher;
    BoltValue params;
    BoltValue extras;

    ResultCallback results_func = nullptr;
};



//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCell
{
public:

    NeoCell(BoltValue params);
    ~NeoCell();

    int Start(const int id = 1);
	int Enqueue_Request(RequestParams&& req);

    void Stop();
    void Set_Running(const bool state);

	bool Is_Running() const;
    std::string Get_Last_Error() const;

private:

    std::atomic<bool> running;          // thread loop controller
    std::atomic<bool> is_shutting_down; // signals if process is shutting down
	std::atomic<int> qcount;            // tracks the active queries in progress, and 
        // acts as a signal controller to read/write threads and close processes

    std::thread write_thread;       // writer thread id
    std::thread read_thread;        // reader therad id
	NeoConnection connection;       // a connection instance; either standalone or routed
	LockFreeQueue<RequestParams> request_queue;   // request queue for the cell
    
	void Write_Loop();
	void Read_Loop();
    void Add_QCount();
    void Sub_QCount();
	void Sleep_Thread();
    void Wait_Thread();

    int Get_QCount() const;

    ResultCallback results_function;
    //struct RouteTable
    //{
    //    std::string writer;     // address to leader in a cluster
    //    std::vector<std::string> readers;   // bunch of replica + followers in a cluster
    //    std::vector<std::string> routes;    // redundant from our perpective but can mask out replicas
    //    std::string database;               // which database is in the cluster
    //    s64 ttl;                // time to live, route refresh rate as defined by server
    //};

    //RouteTable route_table;     // instance
};