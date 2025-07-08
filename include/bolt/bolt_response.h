/**
 * @file connection.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Definition of BoltResponse object which is used to help queue bolt message
 *  responses sent from neo4j database. 
 * 
 * @version 1.0
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "bolt/bolt_message.h"





//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief A light weight, high performace, zero copy structure that is used to
 *  manage bolt responses.
 */
struct BoltResponse
{
    // enum class Status { Success, Failure, Ignored };

    // Status status;
    // std::string message; // Error message or success info
    std::vector<BoltMessage> responses;    // the decoded message chunk (whatever it is)

    // std::vector<std::unordered_map<std::string, BoltValue>> records; // List of key-value records
    // std::unordered_map<std::string, BoltValue> metadata;

    // BoltResponse(BoltMessage m)
    //     :rsp(m) {}

        
    // BoltResponse(Status s, std::string msg = "")
    //     : status{s}, message{std::move(msg)} {}

    // bool success = false;
    // std::string status_message;
    // u64 request_id;                 // debugging purposes

    // // a bolt response may contain multiple rows with named columns
    // std::vector<std::unordered_map<std::string, std::string>> rows;
    // std::unordered_map<std::string, std::string> metadata;      // meta info



    // /**
    //  * @brief adds the next received row mapped to column vs value into the rows
    //  *  member for decoding.
    //  * 
    //  * @param row map of column vs value passed as a single row 
    //  */
    // void Add_Row(std::unordered_map<std::string, std::string> &&row)
    // {
    //     rows.emplace_back(std::move(row));
    // } // end Add_Row
};