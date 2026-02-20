/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com) 
 * 
 * @version 1.0
 * @date created 19th of Feburary 2026, Wednesday
 * @date created 19th of Feburary 2026, Wednesday
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "bolt/bolt_message.h"
#include "bolt/boltvalue_pool.h"





//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief the result of a bolt query. It consists of three fields, the fields
 *  names for the query result, a list of records, and a summary detail, as per
 *  driver specs for neo4j
 */
struct BoltResult
{
    BoltPool<BoltValue>* pool;  // pointer to the pool
    BoltMessage fields;         // the field names for the record
    BoltMessage summary;        // the summary message at end of records
    BoltMessage err{ BoltValue::Make_Unknown() };   // holds error strings 

    size_t message_count{ 0 };  // count of messages contained within records
    size_t start_offset{ 0 };   // start of message offset in the pool
    int client_id = 0;

    struct iterator
    {
        BoltValue* ptr;
        BoltPool<BoltValue>* pool;
        size_t cursor{ 0 };             // current streaming position in pool

        iterator(BoltPool<BoltValue>* po, BoltValue* p, size_t offset)
            : pool(po), ptr(p), cursor(offset) { }

        BoltValue& operator*() { return *ptr; }
        iterator& operator++() { ptr = pool->Get(++cursor); return *this; }
        bool operator!=(const iterator& other) const { return ptr != other.ptr; }
    };

    BoltResult() = default;
    BoltResult(const BoltResult&) = delete;
    BoltResult(BoltResult&&) = default;
    ~BoltResult()
    {
        if (pool)
            Release_Pool<BoltValue>(start_offset);
    } // destroy what's on the pool

    BoltResult& operator=(BoltResult&& other)
    {
        pool = other.pool;
        fields = other.fields;
        summary = other.summary;
        message_count = other.message_count;
        start_offset = other.start_offset;

        other.pool = nullptr;
        return *this;
    } // end move assign

    iterator begin() { return iterator(pool, pool->Get(start_offset), start_offset); }
    iterator end() { return iterator(pool, pool->Get(start_offset + message_count), start_offset); }

    bool Is_Error() const { return err.msg.type != BoltType::Unk; }
};