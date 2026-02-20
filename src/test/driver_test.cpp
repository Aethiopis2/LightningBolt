/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 27th of January 2026, Tuesday
 * @date updated 27th of January 2026, Tuesday
 */



 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neodriver.h"
using namespace std;



static std::atomic<int> completed{ 0 };
static std::atomic<int> records{ 0 };

//===============================================================================|
//          FUNCTIONS
//===============================================================================|
int main()
{
    // 1. start driver
	NeoDriver driver("bolt://localhost:7687", Auth::Basic("neo4j", ""));
    
	BoltResult result;
    driver.Execute_Async("MATCH (n) RETURN n LIMIT 1000",
        [](BoltResult& result)
        {
            while (1)
            {
                records.fetch_add(1, std::memory_order_acq_rel);
            }
            completed.fetch_add(1, std::memory_order_acq_rel);
        });

        // 2. wait for completion
    while (completed.load(std::memory_order_acquire) < 1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 3. report
    cout << "Total Records Retrieved: " << records.load(std::memory_order_acquire) << endl;
	return 0;
}