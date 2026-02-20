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
    BoltMessage(BoltValue val) : msg(std::move(val)) {}

    std::string ToString() const
    {
        return msg.ToString();
    } // end ToString

    bool Success() const { return msg.type == BoltType::Struct && msg.struct_val.tag == BOLT_SUCCESS; }
    bool Failure() const { return msg.struct_val.tag == BOLT_FAILURE; }
    bool Record() const { return msg.struct_val.tag == BOLT_RECORD; }
    bool Ignored() const { return msg.struct_val.tag == BOLT_IGNORED; }
};