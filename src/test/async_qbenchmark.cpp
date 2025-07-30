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
#include <numeric>
using namespace std;



CentralDispatcher g_dispatcher;
std::vector<int64_t> durs;

//===============================================================================|
//          FUNCTIONS
//===============================================================================|
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
            while (pconn->Fetch(out) > 0)
                Print("%s", out.ToString().c_str());
            
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
    const u64 num_query = 1000;

    for (size_t i=0; i < 1; i++)
    {
        g_dispatcher.Init("bolt://localhost:7687", pool_size);
        
        for (u64 j = 0; j < num_query; j++)
        {
            Run_Test_Query(j);
        }

        g_dispatcher.Shutdown();
    }
    if (!durs.empty()) 
    {
        int64_t total = std::accumulate(durs.begin(), durs.end(), int64_t{0});
        int64_t avg = total / static_cast<int64_t>(durs.size());
        Print("Average duration: %lld ms", avg);
    } else 
    {
        Print("No durations recorded.");
    }
    Print("Terminated");
}