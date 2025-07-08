/**
 * @file boltvalue_pool.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief A specialized pool for BoltValues used by our little decoder for bolt messages
 * 
 * @version 1.0
 * @date 16th of April 2025, Sunday.
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
//#include "boltvalue.h"



//===============================================================================|
//          GLOBALS
//===============================================================================|
constexpr u32 SCRATCH_SIZE = 128'048;     // static array size for scratch buffers
constexpr u32 ARENA_SIZE = 16'368;      // size of the fast growing buffer




//===============================================================================|
//          GLOBALS
//===============================================================================|




//===============================================================================|
//          TYPES
//===============================================================================|
/**
 * @brief this structure provides ulta-high speed in cache memory for decoding 
 *   bolt encoded responses from neo4j server.
 */
template<typename T, size_t N>
struct ScratchBuffer
{
    alignas(64) T data[N];
    size_t size = 0;

    T *Data() { return data; }
    const T *Data() const { return data; }

    /**
     * @brief Get's memory from the buffer as long as count fits within
     *  the size boundary if not returns nullptr.
     * 
     * @param count the count of items space to reserve
     */
    T *Alloc(const size_t count)
    {
        if (N <= count + size)
            return nullptr;
        
        T *ptr = data + size;
        size += count;
        return ptr;
    } // end Alloc

    void Reset() { size = 0; }
    void Release(const size_t count)
    {
        size -= count;
        if (size < 0)
            size = 0;
    } // end Release
};


//===============================================================================|
/**
 * @brief another high speed memory pool used for storage of BoltValue's ready
 *  for action when duty calls during decoding; our enemies the compunded types, 
 *  Lists, Structs, Maps/Dictionaries and many of thier variants thereof.
 */
template<typename T>
struct ArenaAllocator
{
    T *data = nullptr;
    size_t used = 0;
    size_t capacity = 0;

    /**
     * @brief constructor allocate's the arena buffer safe. Large enough for 
     *  most cases; even scratch was delibertaly made large to maximize gain
     */
    ArenaAllocator(const size_t initial_size = ARENA_SIZE)
    {
        data = (T*)malloc(sizeof(T) * initial_size);
        capacity = initial_size;
    } // end contr

    /**
     * @brief destroyer
     */
    ~ArenaAllocator()
    {
        if (data)
            free(data);
        data = nullptr;     // double-tap
    } // end destroyer

    /**
     * @brief Allocate's memory for bolt value's from a pre-allocated 
     *  memory pool that possibly exists in heap at worst or L3 cache at best.
     * 
     * @param count the number of spaces to reserve in BoltValue objects
     */
    T *Alloc(const size_t count)
    {
        while (used + count > capacity)
            Grow(std::max(used + count, capacity * 2));

        T *ptr = data + used;
        used += count;
        return ptr;
    } // end Alloc

    
    /**
     * @brief Grows the Arena by the number of bytes specified by parameter
     *  which is usually double of what it was before.
     * 
     * @param cap the new capacity to set (resize)
     */
    void Grow(const size_t cap)
    {
        T* new_data = (T*)realloc(data, sizeof(T) * cap);
        if (new_data) 
        {
            data = new_data;
            capacity = cap;
        } 
        else 
        {
            // Handle allocation failure
            std::cout << "can't grow to " << cap << " from " << capacity << std::endl;
        }

    } // end Grow

    void Reset() { used = 0; }
    void Release(const size_t count)
    {
        used -= count;
        if (used < 0)
            used = 0;
    } // end Release
};


//===============================================================================|
template<typename T>
struct BoltPool
{
    enum class Source { Scratch, Arena };

    ScratchBuffer<T, SCRATCH_SIZE> scratch;
    ArenaAllocator<T> arena;
    std::vector<Source> allocation_log;  // keep a log of where allocations were made

    void Reset_All() 
    {
        scratch.Reset();
        arena.Reset();
    } // end Reset_All


    void Grow_Arena(size_t n) 
    {
        arena.Grow(n);
    } // end Grow_Area


    T* Alloc(size_t count) {
        if (count == 0) return nullptr;

        size_t scratch_available = SCRATCH_SIZE - scratch.size;

        if (count <= scratch_available) 
        {
            allocation_log.push_back(Source::Scratch);
            return scratch.Alloc(count);
        } // end  if scratch

        // Allocate partially from scratch and the rest from arena
        T* scratch_ptr = nullptr;
        if (scratch_available > 0) 
        {
            scratch_ptr = scratch.Alloc(scratch_available);
        } // end if scratch to the last drop

        T* arena_ptr = arena.Alloc(count - scratch_available);

        // Optionally, copy combined allocation into a single continuous buffer in the arena if needed.
        // For BoltValue this may not be required if we only need views/references.
        (scratch_ptr) ? allocation_log.push_back(Source::Scratch) : 
            allocation_log.push_back(Source::Arena);
        return (scratch_ptr) ? scratch_ptr : arena_ptr;
    } // end alloc

    void Release(const size_t count)
    {
        if (allocation_log.empty()) return;

        Source last = allocation_log.back();
        allocation_log.pop_back();

        if (last == Source::Arena) 
        {
            arena.Release(count);
        } // end if
        else {
            scratch.Release(count);
        } // end else
    } // end Release
};


//===============================================================================|
template<typename T>
inline BoltPool<T>& GetBoltPool() 
{
    thread_local BoltPool<T> pool_instance;
    return pool_instance;
} // end BoltPool 