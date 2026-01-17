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
#include "neocell.h"




//===============================================================================|
//          GLOBALS
//===============================================================================|



//===============================================================================|
//          FUNCTIONS
//===============================================================================|
int main() 
{
    Print_Title();
    const size_t iterations = 10000;

    for (size_t i = 0; i < iterations; i++)
    {
        NeoCell con(BoltValue({
			mp("host", "localhost:7687"),
			mp("username", "neo4j"),
			mp("password", ""),
			mp("tls", "false")
            }));

        if (int ret; (ret = con.Start(i+1)) < 0)
        {
            if (ret == -1)
				Dump_Err_Exit("Failed to connect to the server");
			else 
				Fatal(con.Get_Last_Error().c_str());
        } // end if 
        Print("Connected: %d", (int)(i+1));

        con.Stop();
        Print("Disconnected");
    } // end outer for

    Print("Terminated");
} // end main