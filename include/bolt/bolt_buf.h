/**
 * @file bolt_buf.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Defintion of a buffer used for high speed bolt encoding/decoding operaton
 * 
 * @version 1.0
 * @date 15th of April 2025, Tuesday.
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once



//===============================================================================|
//          MACROS
//===============================================================================|
#if defined(__GNUC__) || defined(__clang__)

#define BOLT_PREFETCH(addr) __builtin_prefetch(addr, 1, 3)
#define CACHE_LINE_SIZE     64

#elif defined(_MSC_VER)

#include <xmmintrin.h>
#define BOLT_PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_TO)
#define CACHE_LINE_SIZE     64

#else 

#define BOLT_PREFETCH(addr) ((void)0)
#define CACHE_LINE_SIZE     64

#endif





//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"
#include <memory>
#include <cassert>
#include "utils/utils.h"




//===============================================================================|
//          GLOBALS
//===============================================================================|
static constexpr size_t MIN_CAPACITY = 65'536 + 4;  // chunk size + tail
static constexpr size_t TAIL_SIZE = 1024;           // an emergency tail region





//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief a little utility structure that aids BoltBuf in keeping some stats so
 *  as to decide if there is a need to grow/shrink based on recent traffic.
 */
struct BufferStats
{
    size_t last_bytes_recvd = 0;
    double ema_recv = 0.0f;

    static constexpr double alpha = 0.2;    // EMA smoothing factor
    static constexpr double grow_threshold = 0.8;
    static constexpr double shrink_theshold = 0.8;

    //===============================================================================|
    /**
     * @brief computes the next Exponential Moving Average with pre-kooked constant
     *  values for better estimation of buffer growth.
     */
    void Update(const size_t bytes_this_cycle)
    {
        last_bytes_recvd = bytes_this_cycle;
        ema_recv = alpha * bytes_this_cycle + (1 - alpha) * ema_recv;
    } // end Update

    //===============================================================================|
    /**
	 * @brief determines if buffer should grow based on recent traffic
     */
    bool Should_Grow(const size_t capacity) const
    {
        return ema_recv > capacity * grow_threshold;
    } // end Should_Grow

    //===============================================================================|
    /**
     * @brief determines if buffer should shrink based on recent traffic
	 */
    bool Should_Shrink(const size_t capacity) const 
    {
        return ema_recv < capacity * shrink_theshold;
    } // end Should_Shrink
};


//===============================================================================|
/**
 * @brief defines an optimized buffer aligned to take advantage of hardware
 *  cache alignment for maximum speed and output.
 */
class alignas(CACHE_LINE_SIZE) BoltBuf
{
public:

    BoltBuf(const size_t _capacity = 65'536 * 4)
        : capacity{Align_Capacity(_capacity)},
          raw_ptr{Allocate_Aligned(capacity)},
          data{raw_ptr.get()},
          write_offset{0}, read_offset{0} {
            assert(data);
    } // end Bolt Buf

    BoltBuf(const BoltBuf&) = delete;
    BoltBuf& operator=(const BoltBuf&) = delete;
    BoltBuf(BoltBuf&&) noexcept = default;
    BoltBuf& operator=(BoltBuf&&) noexcept = default;

    //===============================================================================|
    /**
     * @brief return's the head of the write ptr
     */
    inline u8 *Write_Ptr()
    {
        Prefetch_Write();
        return data + write_offset;
    } // end Write_Ptr

    //===============================================================================|
    /**
     * @brief return's the head of the read ptr
     */
    inline u8 *Read_Ptr()
    {
        Prefetch_Read();
        return data + read_offset;
    } // end Read_Ptr

    //===============================================================================|
    /**
     * @brief shifts the write pointer n bytes forwards
     * 
     * @param n the number of bytes to advance
     */
    inline void Advance(size_t n)
    {
        write_offset += n;
        assert(write_offset <= capacity);
    } // end Advance

    //===============================================================================|
    /**
     * @brief notifies read_offset the number of bytes consumed
     * 
     * @param n the number of bytes consumed
     */
    inline void Consume(size_t n)
    {
        read_offset += n;
        assert(read_offset <= capacity);    /* convert to something less firghtnening in production */
    } // end Consume

    //===============================================================================|
    /**
     * @brief reset's the reading/writing offsets to 0
     */
    inline void Reset()
    {
        read_offset = write_offset = 0;
    } // end Reset

    //===============================================================================|
    /**
     * @brief reset's the read head to 0.
     */
    inline void Reset_Read()
    {
        read_offset = 0;
    } // end Reset_Read
    
    //===============================================================================|
    /**
     * @brief return's the bytes contained in the buffer ready for processing
     */
    inline size_t Size()
    {
        return write_offset - read_offset;
    } // end Size

    //===============================================================================|
    /**
     * @brief return's the storage capacity
     */
    inline size_t Capacity()
    {
        return capacity;
    } // end Capacity

    //===============================================================================|
    /**
     * @brief return's true if buffer is empty
     */
    inline bool Empty() const 
    {
        return (write_offset - read_offset) == 0;
    } // end Empy

    //===============================================================================|
    /**
     * @brief return's a pointer to data
     */
    inline u8* Data() 
    {
        return data;
    } // end Data 

    //===============================================================================|
    /**
     * @brief writes the supplied number of bytes into the buffer
     * 
     * @param data to write
     * @param len length of data to write
     */
    inline void Write(const u8 *dat, const size_t len)
    {
        Ensure_Space(len);
        iCpy(data + write_offset, dat, len);
        write_offset += len;
    } // end Write

    //===============================================================================|
    /**
     * @brief moves the write head forwards/backwards by the number of 
     *  bytes specified in the param
     * 
     * @param len bytes to skip forward or back
     */
    inline void Skip(const size_t len)
    {
        size_t temp = write_offset + len;
        if (temp >= 0 && temp < capacity)
            write_offset += len;
    } // end Skip

    //===============================================================================|
    /**
     * @brief writes data at a specific position in the buffer
     * 
     * @param pos position to write at
     * @param ptr pointer to data
     * @param len length of data
	 */
    inline void Write_At(const u32 pos, const u8 *ptr, const size_t len)
    {
        if (pos + len <= capacity)
            iCpy(&data[pos], ptr, len);
    } // end Write_At

    //===============================================================================|
    /**
     * @brief 
     * 
     * @param n sizeof the chunk requested
     */
    inline void Grow(const size_t n)
    {
        if (!stat.Should_Grow(capacity) && (write_offset + n <= capacity - TAIL_SIZE))
            return; 
        
        size_t new_capacity = capacity << 1;
        while (write_offset + n > new_capacity - TAIL_SIZE)
            new_capacity <<= 1;

        auto new_raw_ptr = Allocate_Aligned(new_capacity);
        if (!new_raw_ptr)
            return;

        u8* new_data = new_raw_ptr.get();
        iCpy(new_data, data, write_offset);

        raw_ptr = std::move(new_raw_ptr);
        data = new_data;
        capacity = new_capacity;
    } // end Grow

    //===============================================================================|
    /**
     * @brief shrinks the buffer based on recent traffic stats
	 */
    inline void Shrink()
    {
        if (!stat.Should_Shrink(capacity))
            return;

        size_t used = write_offset - read_offset;
        size_t target_capacity = Align_Capacity(std::max(used << 1, MIN_CAPACITY));

        auto new_raw_ptr = Allocate_Aligned(target_capacity);
        if (!new_raw_ptr)
            return;

        u8* new_data = new_raw_ptr.get();
        if (used > 0)
            iCpy(new_data, data + read_offset, used);

        raw_ptr = std::move(new_raw_ptr);
        data = new_data;
        capacity = target_capacity;

        write_offset = used;
        read_offset = 0;
    } // end Shrink

    //===============================================================================|
    /**
     * @brief return's the writable size remaining in the buffer
	 */
    inline size_t Writable_Size() const 
    {
        return capacity - write_offset - TAIL_SIZE;
    } // end Writable_Size
    
    //===============================================================================|
    /**
     * @brief appends another BoltBuf into this one
     * 
     * @param encoded the buffer to append
	 */
    inline void Append(BoltBuf& encoded)
    {
        if (write_offset + encoded.Size() > capacity)
            if (!Try_Grow())
                return;

        iCpy(data + write_offset, encoded.Data(), encoded.Size());
        write_offset += encoded.Size();
    } // end Append

    //===============================================================================|
    /**
     * @brief Update's the statistics that helps decide buffer grow/shrink
     */
    inline void Update_Stat(const size_t this_cycle_bytes)
    {
        stat.Update(this_cycle_bytes);
    } // end Update_Stat

    //===============================================================================|
    /**
     * @brief gets the current write offset
     */
    inline size_t Get_Write_Offset() const 
    {
        return write_offset;
	} // end Get_Write_Offset

    //===============================================================================|
    /**
     * @brief gets the current read offset
	 */
    inline size_t Get_Read_Offset() const 
    {
        return read_offset;
	} // end Get_Read_Offset

private:

    size_t capacity;
    std::unique_ptr<uint8_t[], decltype(&std::free)> raw_ptr;
    u8 *data;
    size_t write_offset;
    size_t read_offset;
    BufferStats stat;


    //===============================================================================|
    /**
     * @brief tries to grow the buffer capacity
	 */
    inline bool Try_Grow()
    {
        size_t new_capacity = capacity << 1;
        auto new_ptr = Allocate_Aligned(new_capacity);
        if (!new_ptr)
            return false;

        iCpy(new_ptr.get(), data, write_offset);  // preserve written data
        raw_ptr = std::move(new_ptr);
        data = raw_ptr.get();
        capacity = new_capacity;
        return true;
    } // end Try_Grow


    //===============================================================================|
    /**
     * @brief ensures there is enough space to write required bytes
     * 
	 * @param required number of bytes required
     */
    inline bool Ensure_Space(const size_t required)
    {
        if (Writable_Size() < required)
            if (!Try_Grow()) return false;

        return true;
    } // end Ensure_Space

    
    /**
     * @brief aligns the storage capacity based on the size provided to
     *  meet hardware cache alignment requirements for high speed processing
     * 
     * @param n size of storage
     */
    static size_t Align_Capacity(size_t n)
    {
        return ((n + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
    } // end align_capacity


    /**
     * @brief Allocates an aligned buffer size
     * 
     * @param size of the buffer 
     */
    static std::unique_ptr<uint8_t[], decltype(&std::free)> Allocate_Aligned(size_t size)
    {
        void *ptr = nullptr;
        if (posix_memalign(&ptr, CACHE_LINE_SIZE, size) != 0)
            return {nullptr, std::free};
        return {reinterpret_cast<u8*>(ptr), std::free};
    } // end Allocate_Aligned


    /**
     * @brief hints the cpu to prefetch reads so as to prepare the cache
     *  and reduce misses
     */
    inline void Prefetch_Read() const 
    {
        BOLT_PREFETCH(data + read_offset);
    } // end Prefetch_Read

    
    /**
     * @brief hints the cpu to prefetch during writes
     */
    inline void Prefetch_Write() const 
    {
        BOLT_PREFETCH(data + write_offset);
    } // end Prefech_Write
};