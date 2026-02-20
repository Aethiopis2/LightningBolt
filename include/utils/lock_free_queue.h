/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date created 14th of May 2025, Wednesday
 * @date updated 16th of Feburary 2026, Monday
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <optional>






//===============================================================================|
//         CLASS
//===============================================================================|
/**
 * @brief fixed sized ring buffer aka lock free queue. Class makes  use of atomic
 *  members to make it thread safe, and template for generic purposes.
 */
template<typename T, size_t Capacity = 8192>
class LockFreeQueue
{
public:

    /**
     * @brief constructor alloc memory and all
     */
    LockFreeQueue()
        : buffer(Capacity), head(0), tail(0)
    {
        for (auto& slot : buffer) 
            slot.full.store(false, std::memory_order_relaxed);
    } // end Constructor


    /**
     * @brief places the next item into queue based as const ref
     *
     * @param item to place of type T
     *
     * @return true on success
     */
    bool Enqueue(const T& item)
    {
        size_t pos = tail.load(std::memory_order_relaxed);
        size_t next = (pos + 1) & (Capacity - 1);

        if (next == head.load(std::memory_order_acquire))
            return false;

        buffer[pos].value = item;
        buffer[pos].full.store(true, std::memory_order_release);
        tail.store(next, std::memory_order_relaxed);
        return true;
    } // end Enqueue


    /**
     * @brief places rvalue into queue or moves
     *
     * @param item to place of type T
     *
     * @return true on success
     */
    bool Enqueue(T&& item)
    {
        size_t pos = tail.load(std::memory_order_relaxed);
        size_t next = (pos + 1) & (Capacity - 1);

        if (next == head.load(std::memory_order_acquire))
            return false;

        buffer[pos].value = std::move(item);
        buffer[pos].full.store(true, std::memory_order_release);
        tail.store(next, std::memory_order_relaxed);
        return true;
    } // end Enqueue


    /**
     * @brief removes the first element from queue and updates
     *  the head position to next element.
     *
     * @return std::optional T 
     */
    std::optional<T> Dequeue()
    {
        size_t pos = head.load(std::memory_order_relaxed);
        if (!buffer[pos].full.load(std::memory_order_acquire)) 
        {
            return std::nullopt;
        } // end if

        T item = std::move(buffer[pos].value);
        buffer[pos].full.store(false, std::memory_order_release);
        head.store((pos + 1) & (Capacity - 1), std::memory_order_release);
        return item;
    } // end Dequeue


    /**
     * @brief returns a reference to the front item without dequeuing it
	 */
    std::optional<std::reference_wrapper<T>> Front()
    {
        size_t pos = head.load(std::memory_order_relaxed);
        if (!buffer[pos].full.load(std::memory_order_acquire))
            return std:: nullopt;
        
        return std::ref(buffer[pos].value);
    } // end front


    /**
	 * @brief access item at given index without dequeuing it
     */
    std::optional<std::reference_wrapper<T>> operator[](const size_t index)
    {
        size_t pos = head.load(std::memory_order_relaxed) + index;
        if (!buffer[pos].full.load(std::memory_order_acquire))
            return std:: nullopt;

        return std::ref(buffer[pos].value);
    } // end operator[]


    /**
     * @brief returns true if queue is empty
     */
    bool Is_Empty() const 
    {
        return head.load(std::memory_order_relaxed) == tail.load(std::memory_order_relaxed);
    } // end Is_Empty


    /**
     * @brief returns the size of the queue or number of items contained
     */
    size_t Size() const 
    {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        return (t + Capacity - h) & (Capacity - 1);
    } // end Size


    /**
     * @brief clears the queue to empty state
     */
    void Clear()
    {
        head.store(0, std::memory_order_release);
        tail.store(0, std::memory_order_release);
    } // end Clear

private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");

    struct Slot
    {
        std::atomic<bool> full;
        alignas(64) T value;
    };

    std::vector<Slot> buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
};