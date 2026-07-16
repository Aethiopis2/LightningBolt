/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.2
 * @date created 9th of April 2025, Wednesday.
 * @date updated 22nd of March 2026, Sunday.
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
        "     "
    };
};

std::vector<int64_t> durs;


void Test_Record_Fetch()
{
    const size_t iterations = 100;
    std::string url = "bolt://localhost:7687";

    for (size_t i = 0; i < iterations; i++)
    {
        NeoDriver driver(url,
            Auth::Basic("bolt", ""), BoltValue::Make_Map());

		NeoSession session;
        LBStatus rc = driver.Get_Session(session);

        if (!LB_OK(rc))
            Fatal("%s", session.Get_Last_Error().c_str());
        Test test;
        //session.Clear_Histo();

        for (size_t k = 0; k < NUM_TESTS; k++)
        {
            for (u64 j = 0; j < test.rounds[k]; j++)
            {
                auto start = std::chrono::high_resolution_clock::now();
                session.Run(test.cypher[k]);

                BoltResult out;
                int ret = session.Fetch(out);

                Utils::Print("Fields: %s", out.fields.ToString().c_str());
                for (auto v : out)
                {
                    if (!out.error)
                        Utils::Print("Records: %s", v.ToString().c_str());
                    else
                    {
                        Dump_App_Err("%s", v.ToString().c_str());
                        continue;
                    }
                }
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

				/*std::cout << "\nHistogram latencies: ";
				std::cout << "\np50: " << pcell->Percentile(0.50) << " ms\n";
				std::cout << "p95: " << pcell->Percentile(0.95) << " ms\n";
				std::cout << "p99: " << pcell->Percentile(0.99) << " ms\n";
                std::cout << "Wall Latency: " << pcell->Avg_Latency() << " ms\n";*/
				//pcell->Clear_Histo();
            } // end if 
            else
            {
                Utils::Print("No durations recorded.");
                break;
            } // end else

            durs.clear();
        } // end tests nested for

        session.Close();
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