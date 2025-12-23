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
#include "neopool.h"
using namespace std;



static std::atomic<int> completed{ 0 };
static std::atomic<int> records{ 0 };

//===============================================================================|
//          FUNCTIONS
//===============================================================================|
void QueryCallback(int rc, void*) 
{
    if (rc == 0)
        completed.fetch_add(1, std::memory_order_relaxed);
} // end QueryCallback

void FetchCallbackFn(BoltMessage* msg, int status, void*) 
{
    if (msg)
    {
        //std::cout << msg->ToString() << "\n";
        records.fetch_add(1, std::memory_order_relaxed);
    }
} // end FetchCallbackFn


int main() 
{
    constexpr int QUERY_COUNT = 1000;
    NeoCellPool pool(4, BoltValue({
            mp("host", "localhost:7687"),
            mp("username", "neo4j"),
            mp("password", "tobby@melona"),
            mp("encrypted", "false")
        }));
    pool.Start();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < QUERY_COUNT; ++i) {
        NeoCellWorker* cell = pool.Acquire();

        {
            CellCommand cmd;
            cmd.type = CellCmdType::Run;
            cmd.cypher = "UNWIND range(1,100) AS n RETURN n";
            cmd.params = BoltValue::Make_Map();
            cmd.extras = BoltValue::Make_Map();
            cmd.cb = QueryCallback;
            cell->Enqueue(std::move(cmd));
        }

        {
            CellCommand cmd;
            cmd.type = CellCmdType::Fetch;
            cmd.fetch_cb = FetchCallbackFn;
            cell->Enqueue(std::move(cmd));
        }
    }

    while (completed.load() < QUERY_COUNT)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto end = std::chrono::high_resolution_clock::now();
    pool.Stop();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Queries: " << QUERY_COUNT << "\n";
    std::cout << "Records: " << records.load() << "\n";
    std::cout << "Time(ms): " << ms << "\n";
    std::cout << "QPS: " << (QUERY_COUNT * 1000.0 / ms) << "\n";
}