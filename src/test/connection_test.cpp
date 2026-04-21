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
        std::string con = "";
        NeoDriver driver(con,
            Auth::Basic("neo4j", "tobby@melona"));

        NeoSession session;
        LBStatus rc = driver.Get_Session(session);
        if (!LB_OK(rc))
            Fatal("Failed to get session %d", (int)(i + 1));

        Utils::Print("Connected %d times", (int)(i + 1));

        session.Close();
        Utils::Print("Disconnected");
    } // end outer for

    Utils::Print("Terminated");
} // end main