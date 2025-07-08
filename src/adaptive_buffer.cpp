/**
 * @file adaptive-buffer.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief implementation detials for Adaptive buffer
 * 
 * @version 1.0
 * @date 14th of April 2025, Monday.
 * 
 * @copyright Copyright (c) 2025
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "adaptive_buffer.h"




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief initializes the buffer with preset buffer size
 */
AdaptiveBuffer::AdaptiveBuffer(const size_t initial)
{
    buffer.resize(initial);
} // end Constructor



//===============================================================================|
/**
 * @brief return's the address of the current read_offset in the buffer
 */
u8 *AdaptiveBuffer::Data()
{
    return buffer.data() + read_offset;
} // end Data



//===============================================================================|
/**
 * @brief return's the current buffer size from the start of it's offset to read
 */
size_t AdaptiveBuffer::Size() const
{
    return write_offset - read_offset;
} // end Size



//===============================================================================|
/**
 * @brief advances the read offset by the number of bytes provided in the param
 * 
 * @param n the size in bytes to advance read
 */
void AdaptiveBuffer::Advance(size_t n)
{
    read_offset += n;
    if (read_offset > write_offset)
        read_offset = write_offset; // reading should start from write offset
} // end Advance



//===============================================================================|
/**
 * @brief it either moves a chunk of memory forwards in a ring-buffer function or
 *  resets the read/write heads if buffer becomes full. This allows efficent use
 *  of buffer and maniuplate the hardware cache.
 */
void AdaptiveBuffer::Compact()
{
    if (read_offset > 0 && read_offset != write_offset)
    {
        size_t remaining = Size();
        memmove(buffer.data(), buffer.data() + read_offset, remaining);
        write_offset = remaining;
        read_offset = 0;
    } // end if
    else if (read_offset == write_offset) {
        read_offset = write_offset = 0;     // buffer full
    } // end else if
} // end Compact



//===============================================================================|
/**
 * @brief ensure's the remaining buffer size from the write head is enough to 
 *  store what's needed. If it exceededs it, it Compact() buffer and test if not
 *  resizes the buffer in chunks.
 */
void AdaptiveBuffer::Ensure_Capacity(size_t needed)
{
    size_t remaining = buffer.size() - write_offset;
    if (remaining < needed) 
    {
        Compact();
        remaining = buffer.size() - write_offset;
        
        if (remaining < needed) 
        {
            size_t new_capacity = std::max(buffer.size() * 2, write_offset + needed);
            buffer.resize(new_capacity);
        } // end if nested
    } // end if 
} // end Ensure_Capacity



//===============================================================================|
/**
 * @brief return's the remaining storage capacity of the buffer
 */
size_t AdaptiveBuffer::Write_Capacity() const 
{
    return buffer.size() - write_offset;
} // end Write_Capacity



//===============================================================================|
/**
 * @brief return's the start of the buffer write offset
 */
u8* AdaptiveBuffer::Write_Ptr()
{
    return buffer.data() + write_offset;
} // end Write_Ptr



//===============================================================================|
/**
 * @brief write the number of bytes provided from the offset position and updates
 *  the offset.
 */
void AdaptiveBuffer::Write(const u8 *ptr, const size_t len)
{
    // Ensure there's enough capacity
    if (write_offset + len > buffer.size()) 
    {
        // grow the buffer if needed
        Ensure_Capacity(write_offset + len); 
    } // end if

    iCpy(buffer.data() + write_offset, ptr, len);
    write_offset += len;
} // end Write