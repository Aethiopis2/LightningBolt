/**
 * @file lock_free_queue.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief 
 * @version 1.0
 * @date 14th of May 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <optional>
#include "connection/neoconnection.h"
#include "bolt/bolt_request.h"
#include "bolt/decoder_task.h"





//===============================================================================|
//         CLASS
//===============================================================================|
template<typename T, size_t Capacity = 8192>
class LockFreeQueue
{
public:

    LockFreeQueue()
        : buffer(Capacity), head(0), tail(0)
    {
        for (auto& slot : buffer) 
            slot.full.store(false, std::memory_order_relaxed);
    } // end Constructor


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

    std::optional<std::reference_wrapper<T>> Front()
    {
        size_t pos = head.load(std::memory_order_relaxed);
        if (!buffer[pos].full.load(std::memory_order_acquire))
            return std:: nullopt;
        
        return std::ref(buffer[pos].value);
    } // end front

    std::optional<T> operator[](const size_t index)
    {
        if (!buffer[index].full.load(std::memory_order_acquire))
            return std:: nullopt;

        return buffer[index].value;
    } // end operator[]


    bool Is_Empty() const 
    {
        return head.load(std::memory_order_relaxed) == tail.load(std::memory_order_relaxed);
    } // end Is_Empty


    size_t Size() const 
    {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        return (t + Capacity - h) & (Capacity - 1);
    } // end Size


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