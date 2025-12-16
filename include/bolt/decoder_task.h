/**
 * @file decoder_task.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief 
 * @version 1.0
 * @date 14th of May 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/neoconnection.h"



//===============================================================================|
//         CLASS
//===============================================================================|
//class NeoConnection;


struct DecoderTask
{
    //NeoConnection* conn;
    u8* view;
    size_t bytes;           // total bytes received
};