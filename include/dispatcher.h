/**
 * @file connection.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Defintion of Dispatcher objects which are responsible for controlling
 *  the in/out message flow and traffic.
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
#include "bolt/bolt_request.h"
#include "bolt/bolt_response.h"





//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief handler object for database requests coming from clients.
 */
class RequestDispatcher
{
public:

    void Start();
    void Enqueue_Read(BoltRequest &&);
    void Enquque_Write(BoltRequest &&);
};



/**
 * @brief handler object for responses coming from neo4j db server encoded as
 *  BoltResponse
 */
class ResponseDispatcher
{
public:

    void Start();
    void Enqueue_Response(BoltResponse &&);
};