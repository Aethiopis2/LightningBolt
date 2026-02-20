/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.2
 * @date created 9th of April 2025, Wednesday
 * @date updated 25th of June 2026, Sunday.
 */



 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neodriver.h"
#include "utils/errors.h"




//===============================================================================|
//          GLOBALS
//===============================================================================|



//===============================================================================|
//          FUNCTIONS
//===============================================================================|
int main()
{
    Utils::Print_Title();
    const size_t iterations = 10000;

    for (size_t i = 0; i < iterations; i++)
    {
        NeoDriver driver("bolt://localhost:7687",
            Auth::Basic("neo4j", ""));

        NeoCell* pcell = driver.Get_Session();
        if (!pcell)
            Fatal("%s", driver.Get_Last_Error().c_str());

        Utils::Print("Connected %d times and completed in %ld milliseconds",
            (int)(i + 1), pcell->Get_Connection_Time());

        driver.Close();
        Utils::Print("Disconnected");
    } // end outer for

    Utils::Print("Terminated");
} // end main