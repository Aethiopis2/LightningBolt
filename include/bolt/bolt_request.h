/**
 * @file connection.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Defintion of BoltRequest object which is the other counter part of 
 *  BoltResponse object.
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
#include "bolt/bolt_message.h"





//===============================================================================|
//          CLASS
//===============================================================================|
struct BoltResponse;            // a little forward declaration
struct NeoConnection;


/**
 * @brief A light weight, high performace, zero copy structure that is used to
 *  manage bolt responses.
 */
typedef struct BoltRequest_Type
{
    std::string cypher;     // query string
    BoltValue parameters;   // parametrs for cypher string
    BoltValue extras;       // dictionary of extra parameters for requests
    u64 client_id;          // client identifer should we require it

    // concurrency flag, on read we want parell processing while writes need
    //  to be serialized for maximum safety.
    enum class QueryType{ READ, WRITE };   
    QueryType type; 

    std::function<void(NeoConnection*)> On_Complete;     // when request completes
        // we notify client using function in true async mode

    BoltRequest_Type(std::string query, QueryType qt, 
        BoltValue params = BoltValue::Make_Map(), 
        BoltValue exts = BoltValue::Make_Map(), 
        std::function<void(NeoConnection*)> callback = nullptr, u64 id = 0)
        :cypher{std::move(query)}, parameters(params), extras(exts), On_Complete{std::move(callback)},
        type{qt}, client_id{id}
    {
    } // and cntr
} BoltRequest, *BoltRequest_Ptr;