/**
 * @brief small test module to test speed of query running and fetching
 * 
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
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
#include "neodriver.h"
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


void Test_Record_Fetch()
{
    const size_t iterations = 1;

    for (size_t i = 0; i < iterations; i++)
    {
        NeoDriver driver("bolt://localhost:7687",
            Auth::Basic("neo4j", "tobby@melona"));
        NeoCell con(BoltValue({
            mp("host", "localhost:7687"),
            mp("username", "neo4j"),
            mp("password", ""),
            mp("tls", "false")
            }));

        if (int ret; (ret = con.Start()) < 0)
        {
            if (ret == -1)
                Dump_Err_Exit("Failed to connect to the server");
            else
                Fatal(con.Get_Last_Error().c_str());
        } // end if 


        Test test;

        for (size_t k = 0; k < NUM_TESTS; k++)
        {
            /*std::chrono::_V2::system_clock::time_point start;*/
            for (u64 j = 0; j < test.rounds[k]; j++)
            {
                auto start = std::chrono::high_resolution_clock::now();
                con.Enqueue_Request({ CellCmdType::Run, test.cypher[k] });

                BoltResult out;
                int ret = con.Fetch(out);
                if (ret < 0)
                {
                    Fatal(con.Get_Last_Error().c_str());
                    break;
                } // end if

                
                /*Print("Fields: %s", out.fields.ToString().c_str());
                for (auto& v : out.records)
                    Print("Records: %s", v.ToString().c_str());
                Print("Summary: %s", out.summary.ToString().c_str());*/

                auto end = std::chrono::high_resolution_clock::now();
                durs.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
            } // end nested for

            if (!durs.empty())
            {
                int64_t total = accumulate(durs.begin(), durs.end(), int64_t{ 0 });
                int64_t avg = total / static_cast<int64_t>(durs.size());
                Print("cypher: %s%s\truns: %dx\tAvg time: %lld \u00B5s", test.cypher[k],
                    test.spaces[k], test.rounds[k], avg);
            } // end if 
            else
            {
                Print("No durations recorded.");
                break;
            } // end else

            durs.clear();
        } // end tests nested for

        con.Stop();
        cout << endl;
    } // end outer for
} // end Test_Record_Fetch


int main()
{
    Print_Title();

	Print("Testing Record Fetch...");
	Test_Record_Fetch();

    Print("Terminated");
} // end main