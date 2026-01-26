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
#include "neocell.h"




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
    std::string url = "bolt://localhost:7687";
    BoltValue basic = Auth::Basic("neo4j", "tobby@melona");

    for (size_t i = 0; i < iterations; i++)
    {
        NeoCell con(url, &basic, nullptr);

        if (int ret; (ret = con.Start(i + 1)) < 0)
        {
            if (ret == -1)
                Dump_Err_Exit("Failed to connect to the server");
            else
                Fatal(con.Get_Last_Error().c_str());
        } // end if 
        Utils::Print("Connected: %d", (int)(i + 1));

        con.Stop();
        Utils::Print("Disconnected");
    } // end outer for

    Utils::Print("Terminated");
} // end main