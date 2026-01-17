/**
 * @file bolt_message.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Defintion of BoltMessage
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
    //BoltMessage(BoltValue &val) : msg(val) {}
    BoltMessage(BoltValue val) : msg(val) {}
    //~BoltMessage() = default;

    std::string ToString() const
    {
        return msg.ToString();
    } // end ToString
    
    // void Set_Message(u8 signature, BoltValue *val)
    // {
    //     tag = signature;
    //     msg = val;
    // } // end Set_Message

    // bool Is_Success() const { return tag == 0x70; }
    // bool Is_Failure() const { return tag == 0x7E; }
    // bool Is_Record() const { return tag == 0x71; }
    // bool Is_Ignored() const { return tag == 0x7E; }

    // //std::string Debug_Str() const;      // for logging
};