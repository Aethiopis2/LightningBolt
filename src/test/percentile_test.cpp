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
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include "neopool.h"




//===============================================================================|
//          FUNCTIONS
//===============================================================================|
// -------------------- globals --------------------

std::atomic<int> completed{0};
std::atomic<int> records{0};

static inline uint64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// -------------------- timing context --------------------

struct TimingCtx
{
    uint64_t start_ns;
    int slot;
};

// -------------------- storage --------------------

constexpr int QUERY_COUNT = 1000;
std::vector<uint64_t> latencies_ns(QUERY_COUNT);

// -------------------- callbacks --------------------

void QueryLatencyCallback(int rc, void* userdata)
{
    auto* ctx = static_cast<TimingCtx*>(userdata);

    uint64_t end = now_ns();
    latencies_ns[ctx->slot] = end - ctx->start_ns;

    completed.fetch_add(1, std::memory_order_relaxed);

    delete ctx; // safe: owned by callback
}

void FetchCallbackFn(BoltMessage* msg, int status, void*)
{
    if (msg)
        records.fetch_add(1, std::memory_order_relaxed);
}

// -------------------- main test --------------------

int main()
{
    NeoCellPool pool(
        4,
        BoltValue({
            mp("host", "localhost:7687"),
            mp("username", "neo4j"),
            mp("password", "tobby@melona"),
            mp("encrypted", "false")
        })
    );

    pool.Start();

    std::atomic<int> index{0};

    auto wall_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < QUERY_COUNT; ++i)
    {
        NeoCellWorker* cell = pool.Acquire();

        int slot = index.fetch_add(1, std::memory_order_relaxed);

        auto* ctx = new TimingCtx{
            .start_ns = now_ns(),
            .slot = slot
        };

        {
            CellCommand cmd;
            cmd.type = CellCmdType::Run;
            cmd.cypher = "UNWIND range(1,100) AS n RETURN n";
            cmd.params = BoltValue::Make_Map();
            cmd.extras = BoltValue::Make_Map();
            cmd.cb = QueryLatencyCallback;
            cmd.user = ctx;
            cell->Enqueue(std::move(cmd));
        }

        {
            CellCommand cmd;
            cmd.type = CellCmdType::Fetch;
            cmd.fetch_cb = FetchCallbackFn;
            cell->Enqueue(std::move(cmd));
        }
    }

    // wait for completion
    while (completed.load(std::memory_order_relaxed) < QUERY_COUNT)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    auto wall_end = std::chrono::high_resolution_clock::now();
    pool.Stop();

    // -------------------- stats --------------------

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        wall_end - wall_start
    ).count();

    std::sort(latencies_ns.begin(), latencies_ns.end());

    auto p50 = latencies_ns[QUERY_COUNT * 50 / 100];
    auto p95 = latencies_ns[QUERY_COUNT * 95 / 100];
    auto p99 = latencies_ns[QUERY_COUNT * 99 / 100];

    auto ns_to_ms = [](uint64_t ns) {
        return double(ns) / 1e6;
    };

    std::cout << "Queries:   " << QUERY_COUNT << "\n";
    std::cout << "Records:   " << records.load() << "\n";
    std::cout << "Wall(ms):  " << total_ms << "\n";
    std::cout << "QPS:       " << (QUERY_COUNT * 1000.0 / total_ms) << "\n\n";

    std::cout << "Latency percentiles (ms)\n";
    std::cout << "P50: " << ns_to_ms(p50) << "\n";
    std::cout << "P95: " << ns_to_ms(p95) << "\n";
    std::cout << "P99: " << ns_to_ms(p99) << "\n";
}