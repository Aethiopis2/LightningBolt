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
#include "basics.h"




//===============================================================================|
//         CLASS
//===============================================================================|
template<typename T>
class BlockingQueue
{
public:

    void Push(const T& item)
    {
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (closed)
                return;

            que.push(item);
        } // end lock

        cond.notify_one();
    } // end Push


    std::optional<T> Pop()
    {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [&]() { return !que.empty() || closed; });

        if (!que.empty()) 
        {
            T item = std::move(que.front());
            que.pop();
            return item;
        } // end if

        return std::nullopt; // closed and empty
    } // end Pop

    size_t Size() const
    {
        return que.size();
    } // end Size

    
    void Close() 
    {
        {
            std::unique_lock<std::mutex> lock(mutex);
            closed = true;
        } // 

        cond.notify_all(); // wake all waiting threads
    } // end Close


private:

    std::queue<T> que;
    std::mutex mutex;
    std::condition_variable cond;
    bool closed = false;
};