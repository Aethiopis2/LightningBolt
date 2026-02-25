/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date created 13th of April 2025, Sunday.
 * @date updated 9th of Feburary 2026, Monday.
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "neoerr.h"
#include "bolt/bolt_buf.h"
#include "bolt/bolt_message.h"
#include "bolt/bolt_jump_table.h"





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

    /**
	 * @brief decode's objects according to bolt protocol and returns
	 *  the decoded value via out parameter.
     * 
	 * @param out reference to output decoded value
     * 
	 * @note testing purpose only, use Decode(u8* view_start, BoltMessage &out)
     * @returns LB_OK_INFO containing the number of bytes decoded or fail as protocol
     *  violation.
	 */
    LBStatus Decode(BoltValue &out)
    {
		out.buf = &buf;
        u8* start_pos = buf.Read_Ptr();
        u8* pos = start_pos;
        size_t size = buf.Size();
        
        while (size > pos - start_pos) 
        {
            u8 tag = *pos;
            if (!jump_table[tag](pos, out))    
                return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT, 
                    LBCode::LB_CODE_PROTO);
        } // end while
        
        buf.Consume(size);
        return LB_OK_INFO(size);
    } // end Decode


    /**
	 * @brief Decodes a bolt value from a given view pointer, usually this is
     *  from the recv buffer. 
     * 
	 * @param view_start pointer to the start of the view
	 * @param v reference to output decoded value
     * 
	 * @deprecated use Decode(u8* view_start, BoltMessage &out)
     * @returns LB_OK_INFO containing the number of bytes decoded or fail as protocol
     *  violation.
	 */ 
    LBStatus Decode(u8* view_start, BoltValue& v)
    {
		v.buf = &buf;
        u16 chunk = *((u16*)view_start);
        u16 chunk_size = ntohs(chunk);

        u8* pos = view_start + 2;
        while (chunk_size > (pos - view_start))
        {
            u8 tag = *pos;
            if (!jump_table[tag](pos, v))
                return -1;
        } // end while

        u32 consumed = chunk_size + 2;
        if (*(u16*)(pos) == 0x00)
            consumed += 2;

        return LB_OK_INFO(consumed);
    } // end Decode
    

    /**
	 * @brief decode's a bolt message from the internal buffer
     * 
	 * @param msg reference to output decoded message
     * 
	 * @deprecated use overloaded Decode with view pointer
     * @returns LB_OK_INFO containing the number of bytes decoded or fail as protocol
     *  violation.
     */
    LBStatus Decode(BoltMessage& msg)
    {
		msg.msg.buf = &buf;
        u16 chunk = *((u16*)buf.Read_Ptr());
        msg.chunk_size = ntohs(chunk);
        buf.Consume(2);
        
        u8* start = buf.Read_Ptr();
        u8* pos = start;
        while (msg.chunk_size > (pos - start))
        {
            u8 tag = *pos;
            if (!jump_table[tag](pos, msg.msg))
                return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT,
                    LBCode::LB_CODE_PROTO);
        } // end while
        

        if (*(u16*)(buf.Read_Ptr()) == 0x00)
            msg.chunk_size += 2;
      
        buf.Consume(msg.chunk_size);
        return LB_OK_INFO(msg.chunk_size);
    } // end Decode overloaded


   /**
     * @brief Decodes a bolt message from a given view pointer, usually this is
     *  the recv buffer. The function does not partial decode and expects the
     *  caller to make sure buffer is pointing at a full bolt message.
     *
     * @param view_start pointer to the start of the view
     * @param msg reference to output decoded message
     *
     * @returns LB_OK_INFO containing the number of bytes decoded or fail as protocol
     *  violation.
     */
    LBStatus Decode(u8* view_start, BoltMessage &msg)
    {
		msg.msg.buf = &buf;
        u16 chunk = *((u16*)view_start);
        msg.chunk_size = ntohs(chunk);

        u8* pos = view_start + 2;
        while (msg.chunk_size > (pos - view_start))
        {
            u8 tag = *pos;
            if (!jump_table[tag](pos, msg.msg))
                return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_BOLT, 
                    LBCode::LB_CODE_PROTO);
        } // end while

        u32 consumed = msg.chunk_size + 2;
        if (*(u16*)(pos) == 0x00)
            consumed += 2;

        return LB_OK_INFO(consumed);
    } // end Decode overloaded

    
    /**
     * @brief Decodes a bolt message from the internal buffer starting at the given offset.
     *
     * @param offset the offset in the internal buffer to start decoding from
     * @param bv reference to output decoded bolt value
     *
     * @returns LB_OK_INFO containing the number of bytes decoded or fail as protocol
     *  violation.
	 */ 
    LBStatus Decode(size_t offset, BoltValue& bv)
    {
        BoltMessage msg;
        LBStatus rc = Decode(buf.Data() + offset, msg);
        bv = std::move(msg.msg);
        return rc;
	} // end Decode with offset


//private:

    BoltBuf& buf;
}; 