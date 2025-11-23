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



//===============================================================================|
//          GLOBALS
//===============================================================================|
const int NUM_TESTS = 4;

struct Test
{
    const char* cypher[NUM_TESTS] = {
        "RETURN 1",
        "RETURN 1",
        "UNWIND RANGE(1, 1000) AS r RETURN r",
        "MATCH (n) RETURN n LIMIT 10        "
    };
    const int rounds[NUM_TESTS] = { 10, 1000, 100, 100 };
    const char* spaces[NUM_TESTS] = {
        "                            ",
        "                            ",
        " ",
        "       "
    };
};

std::vector<int64_t> durs;




//===============================================================================|
//          FUNCTIONS
//===============================================================================|
int main() 
{
    Print_Title();
    
    Test test;
    const size_t iterations = 1;

    for (size_t i = 0; i < iterations; i++)
    {
        NeoConnection con("localhost:7687@neo4j:username@password");
        if (con.Start() < 0)
            return 0;

        Test test;

        for (size_t k = 0; k < NUM_TESTS; k++)
        {
            for (u64 j = 0; j < test.rounds[k]; j++)
            {
                auto start = std::chrono::high_resolution_clock::now();
                con.Run_Query(test.cypher[k]);

                BoltMessage out;
                while (con.Fetch_Sync(out) > 0);
                    // Print("%s", out.ToString().c_str());

                auto end = std::chrono::high_resolution_clock::now();
                durs.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
            } // end nested for

            if (!durs.empty()) 
            {
                int64_t total = std::accumulate(durs.begin(), durs.end(), int64_t{0});
                int64_t avg = total / static_cast<int64_t>(durs.size());
                Print("cypher: %s%s\truns: %dx\tAvg time: %lld µs", test.cypher[k],
                    test.spaces[k], test.rounds[k], avg);
            } // end if 
            else 
            {
                Print("No durations recorded.");
            } // end else

            durs.clear();
        } // end tests nested for

        con.Stop();
        cout << endl;
    } // end outer for

    Print("Terminated");
} // end main