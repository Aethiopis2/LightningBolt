/**
 * @file bolt_decoder.h
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
#include "bolt/bolt_buf.h"
#include "bolt/bolt_message.h"
#include "bolt/boltvalue_pool.h"
#include "bolt/bolt_jump_table.h"
#include "utils/utils.h"
#include "utils/errors.h"





//===============================================================================|
//          MACRO
//===============================================================================|




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief a helper bolt message encoder class.
 */
class BoltDecoder
{
friend class BoltValue;

public:

    BoltDecoder(BoltBuf &b) : buf(b) {}

    void Decode(BoltValue &out)
    {
		out.buf = &buf;
        u8* start_pos = buf.Read_Ptr();
        u8* pos = start_pos;
        size_t size = buf.Size();
        
        while (size > pos - start_pos) 
        {
            u8 tag = *pos;
            jump_table[tag](pos, out);
            if (out.type == BoltType::Unk)
            {
                Dump_Err("Unkown type decoded.");
                return;
            } // end if
        } // end while
        
        buf.Consume(size);
    } // end Decode


    int Decode(u8* view_start, BoltValue& v)
    {
		v.buf = &buf;
        u16 chunk = *((u16*)view_start);
        u16 chunk_size = htons(chunk);

        u8* pos = view_start + 2;
        while (chunk_size > (pos - view_start))
        {
            u8 tag = *pos;
            if (!jump_table[tag](pos, v))
            {
                error_string = "Unexpected tag: " + std::to_string(tag);
                return -1;
            } // end if
        } // end while

        u32 consumed = chunk_size + 2;
        if (*(u16*)(pos) == 0x00)
            consumed += 2;

        return consumed;
    } // end Decode
    
    /**
     * 
     */
    void Decode(BoltMessage& msg)
    {
		msg.msg.buf = &buf;
        u16 chunk = *((u16*)buf.Read_Ptr());
        msg.chunk_size = htons(chunk);
        buf.Consume(2);
        
        u8* start = buf.Read_Ptr();
        u8* pos = start;
        while (msg.chunk_size > (pos - start))
        {
            u8 tag = *pos;
            jump_table[tag](pos, msg.msg);
        } // end while
        
        if (*(u16*)(buf.Read_Ptr()) == 0x00)
            buf.Consume(2 + msg.chunk_size);
        else 
            buf.Consume(msg.chunk_size);
    } // end Decode overloaded


    /**
     * 
     */
    int Decode(u8* view_start, BoltMessage &msg)
    {
		msg.msg.buf = &buf;
        u16 chunk = *((u16*)view_start);
        msg.chunk_size = htons(chunk);

        u8* pos = view_start + 2;
        while (msg.chunk_size > (pos - view_start))
        {
            u8 tag = *pos;
            if (!jump_table[tag](pos, msg.msg))
            {
                error_string = "Unexpected tag: " + std::to_string(tag);
                return -1;
            } // end if
        } // end while

        u32 consumed = msg.chunk_size + 2;
        if (*(u16*)(pos) == 0x00)
            consumed += 2;

        return consumed;
    } // end Decode overloaded

    /**
     * @brief returns a string representaion of the last
     *  decoding error encountered
     */
    std::string Get_Last_Error() const 
    {
        return error_string;
    } // end Get_Last_Error

private:

    BoltBuf &buf;
    std::string error_string;
}; 