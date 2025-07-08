/**
 * @file main.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief stress testing bolt encoder and decoder speeds
 * @version 1.2
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <chrono>
#include "connection/central_dispatcher.h"
#include "bolt/bolt_response.h"
using namespace std;



CentralDispatcher g_dispatcher;
std::vector<int64_t> durs;

//===============================================================================|
//          FUNCTIONS
//===============================================================================|
void callback(const std::vector<BoltMessage>& res)
{

} // end callback


void Run_Test_Query(u64 client_id) {
    auto start = std::chrono::high_resolution_clock::now();

    BoltRequest req(
        "MATCH (n) RETURN n LIMIT 10",
        BoltRequest::QueryType::READ,
        BoltValue::Make_Map(),
        BoltValue::Make_Map(),
        [client_id, start](NeoConnection* pconn) {
            // std::cout << "Client " << client_id << " got response:\n";
            BoltMessage out;
            while (pconn->Fetch(out) > 0);
                // Print("%s", out.ToString().c_str());
            
            auto end = std::chrono::high_resolution_clock::now();
            durs.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
            return 0;
        },
        client_id
    );
    g_dispatcher.Submit_Request(std::move(std::make_shared<BoltRequest>(req)));
}


int main() 
{
    Print_Title();
    
    const int pool_size = 1;
    const u64 num_query = 10;

    for (size_t i=0; i < 1; i++)
    {
    g_dispatcher.Init("localhost:7687@neo4j:mariam@tobby@melona", pool_size);
    shared_ptr<NeoConnection> con = g_dispatcher.Get_Connection(0);
    
    for (u64 i = 0; i < num_query; i++)
    {
        Run_Test_Query(i);
        // auto start = std::chrono::high_resolution_clock::now();
        // con->Run_Query(std::make_shared<BoltRequest>(
        //     BoltRequest("MATCH (n) RETURN n LIMIT 10", BoltRequest::QueryType::READ)));

        // BoltMessage out;
        // while (con->Fetch(out) > 0);
        //     // Print("%s", out.ToString().c_str());

        // auto end = std::chrono::high_resolution_clock::now();
        // durs.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    }

    g_dispatcher.Shutdown();
    }
    int64_t total = 0;
    for (size_t j = 0; j < durs.size(); j++)
        total += durs[j];

    Print("Took %d ms", durs.size() > 0 ? total / durs.size() : 0);
    Print("Terminated");
}