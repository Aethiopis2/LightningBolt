/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date created 14th of April 2025, Monday.
 * @date updated 2nd of Feburary 2026, Monday.
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