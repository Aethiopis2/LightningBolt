/**
 * @file addicion.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief implementation detials for BoltDecoder object
 * 
 * @version 1.0
 * @date 14th of April 2025, Monday.
 * 
 * @copyright Copyright (c) 2025
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
struct BoltValue;




//===============================================================================|
//          EXTERNS
//===============================================================================|
using DecodeFn = bool (*)(u8*&, BoltValue&);
extern const DecodeFn jump_table[256];