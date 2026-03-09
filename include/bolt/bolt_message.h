/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Defintion of BoltMessage
 * 
 * @version 1.0
 * @date created 13th of April 2025, Sunday.
 * @date update 27th of Feburary 2026, Friday.
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"
#include "bolt/boltvalue.h"




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief describes the properties of a bolt message. Every bolt has a signature or
 *  a tag info that identifes the BoltValue type.
 */
struct BoltMessage
{
    u16 chunk_size;
    BoltValue msg;             // bolt encoded msgs usually struct
    u16 padding{0};

    BoltMessage() = default;
    BoltMessage(BoltValue val) : msg(std::move(val)) {}

    std::string ToString() const
    {
        return msg.ToString();
    } // end ToString
};