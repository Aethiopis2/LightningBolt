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



//===============================================================================|
//          GLOBALS
//===============================================================================|
constexpr u32 SCRATCH_SIZE = 512;     // static array size for scratch buffers
constexpr u32 ARENA_SIZE = 1024;      // size of the fast growing buffer




//===============================================================================|
//          GLOBALS
//===============================================================================|




//===============================================================================|
//          TYPES
//===============================================================================|
/**
 * @brief A scratch buffer that is used to store BoltValues temporarily
 *  for fast access and encoding.
 * 
 * @tparam T the type of data to store in the buffer
 * @tparam N the size of the buffer
 */
template<typename T, size_t N>
struct ScratchBuffer
{
    alignas(64) T data[N];
    size_t size = 0;


    /**
     * @brief Get's memory from the buffer as long as count fits within
     *  the size boundary if not returns 0.
     * 
     * @param count the count of items space to reserve
     */
    size_t Alloc(const size_t count)
    {
        if (count == 0 || count + size > N)
            return size_t(-1);  // can't allocate more than the buffer size
        
        size_t offset = size;
        size += count;
        return offset;
    } // end Alloc


    /**
     * @brief Get's memory from the buffer as long as count fits within
     *  the size boundary if not returns nullptr.
     * 
     * @param offset the offset to get the data from
     */
    T* Get(const size_t offset)
    {
        if (offset >= N)
            return nullptr;  // out of bounds

        return data + offset;
    } // end Get


    /**
     * @brief Get's memory from the buffer as long as count fits within
     *  the size boundary if not returns 0.
     * 
     * @param offset the offset to get the data from
     */
    const T* Get(const size_t offset) const
    {
        if (offset >= N)
            return nullptr;  // out of bounds

        return data + offset;
    } // end Get


    /**
     * @brief Resets the buffer size to 0.
     */
    void Reset() { size = 0; }


    /**
     * @brief Releases the specified number of items from the buffer.
     * 
     * @param count the number of items to release
     */
    void Release(const size_t count)
    {
        size = (count > size) ? 0 : size - count;
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
        if (!data) 
        {
            std::runtime_error("Failed to allocate memory for ArenaAllocator");
            return;
        } // end if failed

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
        used = 0;
        capacity = 0;
    } // end destroyer


    /**
     * @brief Allocate's memory for bolt value's from a pre-allocated 
     *  memory pool that possibly exists in heap at worst or L3 cache at best.
     * 
     * @param count the number of spaces to reserve in BoltValue objects
     */
    size_t Alloc(const size_t count)
    {
        if (count == 0)
            return size_t(-1);  // can't allocate more than the buffer size

        while (used + count > capacity)
            Grow(std::max(used + count, capacity * 2));

        size_t offset = used;
        used += count;
        return offset;
    } // end Alloc

    
    /**
     * @brief Grows the Arena by the number of bytes specified by parameter
     *  which is usually double of what it was before.
     * 
     * @param cap the new capacity to set (resize)
     */
    void Grow(const size_t new_cap)
    {
        T* new_data = static_cast<T*>(realloc(data, sizeof(T) * new_cap));
        if (!new_data) 
        {
            std::runtime_error("Failed to grow ArenaAllocator");
            return;
        } // end if failed

        data = new_data;
        capacity = new_cap;
    } // end Grow


    /**
     * @brief Resets the arena allocator to its initial state.
     */
    void Reset() { used = 0; }


    /**
     * @brief Releases the specified number of items from the arena.
     * 
     * @param count the number of items to release
     */
    void Release(const size_t count)
    {
        used = (count > used) ? 0 : used - count;
    } // end Release


    /**
     * @brief Get's memory from the buffer as long as count fits within
     *  the size boundary if not returns nullptr.
     * 
     * @param offset the offset to get the data from
     */
    T* Get(const size_t offset)
    {
        if (offset >= capacity)
            return nullptr;  // out of bounds

        return data + offset;
    } // end Get


    /**
     * @brief Get's memory from the buffer as long as count fits within
     *  the size boundary if not returns nullptr.
     * 
     * @param offset the offset to get the data from
     */
    const T* Get(const size_t offset) const
    {
        if (offset >= capacity)
            return nullptr;  // out of bounds

        return data + offset;
    } // end Get
};


//===============================================================================|
/**
 * @brief A pool for BoltValues that uses a scratch buffer and an arena allocator
 *  to manage memory efficiently.
 * 
 * @tparam T the type of data to store in the pool
 */
template<typename T>
struct BoltPool
{
    struct Allocation 
    {
        size_t offset;  // global offset
        size_t count;   // how many items were allocated
    };

    ScratchBuffer<T, SCRATCH_SIZE> scratch;
    ArenaAllocator<T> arena;
    std::vector<Allocation> allocation_log;


    /**
     * @brief Resets the pool, clearing all allocations and buffers.
     */
    void Reset_All() 
    {
        scratch.Reset();
        arena.Reset();
        allocation_log.clear();
    } // end Reset_All


    /**
     * @brief Allocates count elements. Returns global offset.
     */
    size_t Alloc(size_t count) 
    {
        if (count == 0) return size_t(-1);

        size_t scratch_available = SCRATCH_SIZE - scratch.size;
        size_t offset = scratch.size;

        if (count <= scratch_available) 
        {
            // Fully in scratch
            scratch.Alloc(count);
            allocation_log.push_back({ offset, count });
            return offset;
        } // end if scratch available

        // Part in scratch, rest in arena
        size_t used_from_scratch = scratch_available;
        size_t used_from_arena = count - used_from_scratch;

        // Scratch
        if (used_from_scratch > 0)
            offset = scratch.Alloc(used_from_scratch);

        // Arena
        size_t arena_local_offset = arena.Alloc(used_from_arena);
        if (arena_local_offset == size_t(-1)) return size_t(-1);

        if (used_from_scratch == 0) offset = arena_local_offset + scratch.size;
        allocation_log.push_back({ offset, count });
        return offset;
    } // end Alloc


    /**
     * @brief Releases the most recent allocation (LIFO)
     * 
     * @param clear_all special command to explicitly free the buffer
     */
    void Release(const bool clear_all) 
    {
        if (clear_all)
        {
            allocation_log.clear();
            Reset_All();
        } // end if

        if (allocation_log.empty()) return;

        Allocation last = allocation_log.back();
        allocation_log.pop_back();

        size_t start = last.offset;
        size_t count = last.count;

        if (start + count <= SCRATCH_SIZE) 
        {
            // All in scratch
            scratch.Release(count);
        } // end if
        else if (start >= SCRATCH_SIZE) 
        {
            // All in arena
            arena.Release(count);
        } // end else if
        else 
        {
            // Spans both
            size_t used_in_scratch = SCRATCH_SIZE - start;
            size_t used_in_arena  = count - used_in_scratch;
            scratch.Release(used_in_scratch);
            arena.Release(used_in_arena);
        } // end else
    } // end Release


    /**
     * @brief Returns raw pointer to element at a global offset.
     */
    T* Get(size_t global_offset) 
    {
        if (global_offset < SCRATCH_SIZE)
            return scratch.Get(global_offset);
        else
            return arena.Get(global_offset - SCRATCH_SIZE);
    } // end Get


    /**
     * @brief Returns raw pointer to element at a global offset.
     */
    const T* Get(size_t global_offset) const 
    {
        if (global_offset < SCRATCH_SIZE)
            return scratch.Get(global_offset);
        else
            return arena.Get(global_offset - SCRATCH_SIZE);
    } // end Get


    /**
     * @brief returns the last offset position in the pool
     */
    size_t Get_Last_Offset() const 
    {
		return scratch.size + arena.used;
	} // end Get_Last_Offset
};


//===============================================================================|
/**
 * @brief Get's a thread-local BoltPool instance for the specified type.
 * 
 * @tparam T the type of data to store in the pool
 * @return BoltPool<T>* pointer to the thread-local BoltPool instance
 */
template<typename T>
inline BoltPool<T>* GetBoltPool() 
{
    thread_local BoltPool<T> pool_instance;
    return &pool_instance;
} // end BoltPool 