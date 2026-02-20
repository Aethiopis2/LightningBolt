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
#include "utils/errors.h"
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
    const size_t iterations = 10;

    for (size_t i = 0; i < iterations; i++)
    {
        NeoDriver driver("bolt://localhost:7687",
            Auth::Basic("neo4j", "tobby@melona"));
        NeoCell* pcell = driver.Get_Session();

        if (!pcell)
            Fatal("%s", driver.Get_Last_Error().c_str());
        Test test;

        for (size_t k = 0; k < NUM_TESTS; k++)
        {
            for (u64 j = 0; j < test.rounds[k]; j++)
            {
                auto start = std::chrono::high_resolution_clock::now();
                pcell->Enqueue_Request({ CellCmdType::Run, test.cypher[k] });

                BoltResult out;
                int ret = pcell->Fetch(out);
                if (out.Is_Error())
                {
                    Fatal("%s", out.err.ToString().c_str());
                    break;
                } // end if

                
                Utils::Print("Fields: %s", out.fields.ToString().c_str());
                for (auto v : out)
                    Utils::Print("Records: %s", v.ToString().c_str());
                Utils::Print("Summary: %s", out.summary.ToString().c_str());

                auto end = std::chrono::high_resolution_clock::now();
                durs.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
            } // end nested for

            if (!durs.empty())
            {
                int64_t total = accumulate(durs.begin(), durs.end(), int64_t{ 0 });
                int64_t avg = total / static_cast<int64_t>(durs.size());
                Utils::Print("cypher: %s%s\truns: %dx\tAvg time: %lld \u00B5s", test.cypher[k],
                    test.spaces[k], test.rounds[k], avg);
            } // end if 
            else
            {
                Utils::Print("No durations recorded.");
                break;
            } // end else

            durs.clear();
        } // end tests nested for

        driver.Close();
        cout << endl;
    } // end outer for
} // end Test_Record_Fetch


int main()
{
    Utils::Print_Title();

	Utils::Print("Testing Record Fetch...");
	Test_Record_Fetch();

    Utils::Print("Terminated");
} // end main