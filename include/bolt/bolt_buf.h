/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 15th of April 2025, Tuesday.
 * @date updated 4th of March 2026, Wednesday.
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
 * @brief a buffer stat utility structure that allows BoltBuf to become adaptive
 *  to receive traffic trends. It uses discrete-time leaky integrator or  
 *  exponential moving average (EMA), y[n]=αy[n−1]+(1−α)⋅prev_ema
 *  to track changes in network traffic and adapt to trends. Its biased towards 
 *  growth and less of shrink to avoid trashing buffer. This allows for adaptive
 *  buffer that relies on network traffic to mainitain its own optimum size.
 */
struct BufferStats
{
    size_t last_bytes_recvd = 0;
    double ema_recv = 0.0f;

    static constexpr double alpha = 0.2;            // EMA smoothing factor
    static constexpr double grow_threshold = 0.85;  // aggressive growth
    static constexpr double shrink_theshold = 0.30; // conservative growth

    static constexpr int grow_hits_required = 3;   
    static constexpr int shrink_hits_required = 32;

    int grow_hits = 0;
    int shrink_hits = 0;
    
    /**
     * @brief computes the next Exponential Moving Average with pre-kooked constant
     *  values for better estimation of buffer growth.
     */
    void Update(const size_t bytes_this_cycle)
    {
        last_bytes_recvd = bytes_this_cycle;
        ema_recv = alpha * bytes_this_cycle + (1 - alpha) * ema_recv;
    } // end Update

    
    /**
     * @brief decideds to grow based on the calculated ema_recv value. When ema_recv
     *  value exceeds the growth threshold and growth hit count exceeds the required
     *  the buffer can be grow. This allows for growth based on recv trend and not 
     *  just random spikes.
     *
     * @param capacity the buffer storage size
     *
     * @return true if buffer can grow alas false if not
     */
    bool Evaluate_Grow(const size_t capacity)
    {

#ifdef _DEBUG
        std::cout << "Evaluating buffer growth: " <<
            "EMA: " << ema_recv << ", capacity: " <<
            capacity << ", growth threshold: " <<
            grow_threshold << ", cap * grow_threshold: " << 
            capacity * grow_threshold << std::endl;
#endif

        if (ema_recv > capacity * grow_threshold)
        {
            ++grow_hits;
            shrink_hits = 0;
            if (grow_hits >= grow_hits_required)
            {
                grow_hits = 0;
                return true;
            } // end if grow
            else grow_hits = 0;
        } // end if ema_recv grow

        return false;
    } // end Evaluate_Grow


    /**
     * @brief decides to shrink based on current ema_recv value and shrink count. 
     *  Should both exceed their thresholds then buffer shrinks to a lower size.
     * 
     * @param capacity the buffer storage size
     * 
     * @return true if allowed to shrink alas false.
     */
    bool Evaluate_Shrink(const size_t capacity)
    {

#ifdef _DEBUG
        std::cout << "Evaluating buffer shrink: " <<
            "EMA: " << ema_recv << ", capacity: " <<
            capacity << ", shrink threshold: " <<
            shrink_theshold << ", cap * shrink_threshold: " <<
            capacity * shrink_theshold << std::endl;
#endif

        if (ema_recv < capacity * shrink_theshold)
        {
            ++shrink_hits;
            grow_hits = 0;
            if (shrink_hits >= shrink_hits_required)
            {
                shrink_hits = 0;
                return true;
            } // end if shrinking fo real
            else shrink_hits = 0;
        } // end if shrinking

        return false;
    } // end Evaluate_Shrink
};


//===============================================================================|
/**
 * @brief defines a recv buffer structure for bolt PackStream with growing and/or
 *  shrinking capacity optimized for hardware viz cache alignmened storage and 
 *  prefetch hints.
 */
class alignas(CACHE_LINE_SIZE) BoltBuf
{
public:

    BoltBuf(const size_t _capacity = 8192)
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

    
    /**
     * @brief return's the head of the write ptr
     */
    inline u8 *Write_Ptr()
    {
        Prefetch_Write();
        return data + write_offset;
    } // end Write_Ptr

    
    /**
     * @brief return's the head of the read ptr
     */
    inline u8 *Read_Ptr()
    {
        Prefetch_Read();
        return data + read_offset;
    } // end Read_Ptr

    
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

    
    /**
     * @brief reset's the reading/writing offsets to 0
     */
    inline void Reset()
    {
        read_offset = write_offset = 0;
    } // end Reset

    
    /**
     * @brief reset's the read head to 0.
     */
    inline void Reset_Read()
    {
        read_offset = 0;
    } // end Reset_Read
    
    
    /**
     * @brief return's the bytes contained in the buffer ready for processing
     */
    inline size_t Size()
    {
        return write_offset - read_offset;
    } // end Size

    
    /**
     * @brief return's the storage capacity
     */
    inline size_t Capacity()
    {
        return capacity;
    } // end Capacity

    
    /**
     * @brief return's true if buffer is empty
     */
    inline bool Empty() const 
    {
        return (write_offset - read_offset) == 0;
    } // end Empy

    
    /**
     * @brief return's a pointer to data
     */
    inline u8* Data() 
    {
        return data;
    } // end Data 

    
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

    
    /**
	 * @brief doubles the buffer size if possible by allocating cache 
     *  aligned region of memory. Once allocated copies the old data
     *  back and arranges read/write offsets in accordance.
     * 
	 * @return 0 on success and -1 on failure
     */
    inline int Grow()
    {
        size_t new_capacity = capacity << 1;

        auto new_raw_ptr = Allocate_Aligned(new_capacity);
        if (!new_raw_ptr)
			return -1;      // failed to allocate

        u8* new_data = new_raw_ptr.get();
        size_t used = write_offset - read_offset;

#ifdef _DEBUG
        std::cout << "Buffer grow: " << capacity <<
            " --> " << new_capacity << std::endl;
#endif

        //if (used > 0)
            iCpy(new_data, data, capacity);

        raw_ptr = std::move(new_raw_ptr);
        data = new_data;
        capacity = new_capacity;

        /*write_offset = used;
        read_offset = 0;*/
		return 0;  // success
    } // end Grow

    
    /**
     * @brief shrinks the buffer to twice the used capacity or clamps it at
     *  a defined MIN_CAPACITY which is 65k in this imp, 16-bit is max message size.
     *  Afterwards it adjustes the offsets to the newly sized buffer on success.
     * 
     * @return 0 on success and -1 on fail.
	 */
    inline int Shrink()
    {
        if (capacity == MIN_CAPACITY)
            return 0;

        size_t used = write_offset - read_offset;
        size_t target_capacity = Align_Capacity(std::max(used << 1, MIN_CAPACITY));

        auto new_raw_ptr = Allocate_Aligned(target_capacity);
        if (!new_raw_ptr)
            return -1;

#ifdef _DEBUG
        std::cout << "Buffer shrink: " << capacity <<
            " --> " << std::max(used << 1, MIN_CAPACITY) << std::endl;
#endif

        u8* new_data = new_raw_ptr.get();
        if (used > 0)
            iCpy(new_data, data + read_offset, used);

        raw_ptr = std::move(new_raw_ptr);
        data = new_data;
        capacity = target_capacity;

        write_offset = used;
        read_offset = 0;
        return 0;
    } // end Shrink

    
    /**
     * @brief return's the writable size remaining in the buffer
	 */
    inline size_t Writable_Size() const 
    {
        return capacity - write_offset - TAIL_SIZE;
    } // end Writable_Size
    
    
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

    
    /**
     * @brief Update's the statistics that helps decide buffer grow/shrink
     */
    inline void Update_Stat(const size_t this_cycle_bytes)
    {
        stat.Update(this_cycle_bytes);
    } // end Update_Stat

    
    /**
     * @brief gets the current write offset
     */
    inline size_t Get_Write_Offset() const 
    {
        return write_offset;
	} // end Get_Write_Offset

    
    /**
     * @brief gets the current read offset
	 */
    inline size_t Get_Read_Offset() const 
    {
        return read_offset;
	} // end Get_Read_Offset

    
    /**
     * @brief updates the ema_recv value for buffer stats on every call, and
     *  decides if buffer should grow, shrink or neither based on recv cycle trends.
     */
    inline void Adaptive_Tick(size_t bytes_this_cycle)
    {
        stat.Update(bytes_this_cycle);
        if (stat.Evaluate_Grow(capacity)) Grow();
        else if (stat.Evaluate_Shrink(capacity)) Shrink();
    } // end Adaptive_Tick


    /**
     * @brief compacts the buffer towards the start to shove up space for
     *  recv. The function copies whats left towards the start and resets
     *  the read offset and and adjusts the write to the end of whats left.
     */
    inline bool Compact()
    {
        if (read_offset == 0) return false;     // compacted already

        if (write_offset > read_offset)
        {
            size_t whats_left = write_offset - read_offset;
            iCpy(Data(), Read_Ptr(), whats_left);

            read_offset = 0;
            write_offset = whats_left;
        } // end if saftey net

        return true;
    } // end Compact


private:

    size_t capacity;
    std::unique_ptr<uint8_t[], decltype(&std::free)> raw_ptr;
    u8 *data;
    size_t write_offset;
    size_t read_offset;
    BufferStats stat;


    
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