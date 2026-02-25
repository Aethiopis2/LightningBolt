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
    BoltDecoder* pdec;          // pointer to the decoder for the result
    BoltMessage fields;         // the field names for the record
    BoltMessage summary;        // the summary message at end of records
    BoltMessage err{ BoltValue::Make_Unknown() };   // holds error strings 

    size_t message_count{ 0 };  // count of messages contained within records
	size_t total_bytes{ 0 };     // total bytes consumed by the records
    size_t start_offset{ 0 };   // start of message offset in the pool
    int client_id = 0;

    struct iterator
    {
        BoltValue bv;
        BoltDecoder* pdecoder;
        size_t cursor{ 0 };     // current streaming position in pool

        iterator(BoltDecoder* pd, size_t offset)
            : pdecoder(pd), cursor(offset) 
        { 
			LBStatus rc = pdecoder->Decode(cursor, bv);
            if (LB_OK(rc)) cursor += LB_Aux(rc);
            else
				bv.type = BoltType::Unk;  // mark as unknown on error
		} // end cntr

        BoltValue& operator*() { return bv; }
        iterator& operator++() 
        { 
            LBStatus rc = pdecoder->Decode(cursor, bv);
            if (LB_OK(rc)) cursor += LB_Aux(rc);
            else
                bv.type = BoltType::Unk;  // mark as unknown on error

            return *this; 
		} // end pre-increment
        bool operator!=(const iterator& other) const { return cursor != other.cursor; }
    };

    BoltResult() = default;
    BoltResult(const BoltResult&) = delete;
    BoltResult(BoltResult&&) = default;

    BoltResult& operator=(BoltResult&& other)
    {
        pdec = other.pdec;
        fields = other.fields;
        summary = other.summary;
        message_count = other.message_count;
		total_bytes = other.total_bytes;
        start_offset = other.start_offset;

        other.pdec = nullptr;
        return *this;
    } // end move assign

    iterator begin() { return iterator(pdec, start_offset); }
    iterator end() { return iterator(pdec, start_offset + total_bytes); }

    bool Is_Error() const { return err.msg.type != BoltType::Unk; }
};