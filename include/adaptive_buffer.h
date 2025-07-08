/**
 * @file adaptive-buffer.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Defintion of BoltDecoder object
 * 
 * @version 1.0
 * @date 13th of April 2025, Sunday.
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief a special buffer class that expands its memory size by looking at the
 *  traffic for driver requests. i.e. simulates runtime ajustment of buffer so as
 *  to leverage the power L1/L2/L3 hardware cashes plus SIMD instructions for 
 *  ultra-high performance.
 */
class AdaptiveBuffer
{
public: 

    AdaptiveBuffer(const size_t initial = 65536);

    u8 *Data();
    size_t Size() const;
    void Advance(const size_t);
    void Ensure_Capacity(size_t needed);
    void Compact();
    void Write(const u8 *ptr, const size_t len);
    u8* Write_Ptr();
    size_t Write_Capacity() const;

private:

    std::vector<u8> buffer;
    size_t read_offset = 0;
    size_t write_offset = 0;
};