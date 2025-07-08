/**
 * @file central_dispatcher.h
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
#include <shared_mutex>
#include "connection/neoconnection.h"
#include "bolt/bolt_request.h"
#include "bolt/decoder_task.h"
#include "utils/lock_free_queue.h"






//===============================================================================|
//         CLASS
//===============================================================================|
class CentralDispatcher
{
public:
    static CentralDispatcher& Instance()
    {
        static CentralDispatcher instance;
        return instance;
    } // end Instance

    CentralDispatcher() = default;
    void Init(const std::string &connection_string, 
        const size_t connection_count = std::thread::hardware_concurrency());
    void Submit_Request(std::shared_ptr<BoltRequest> req);
    void Submit_Response(std::shared_ptr<DecoderTask> task);
    void Shutdown();
    // BoltValue Fetch();

    void Add_Ref();
    void Sub_Ref();
    u64 Get_Ref() const;
    
    std::shared_ptr<NeoConnection> Get_Connection();
    std::shared_ptr<NeoConnection> Get_Connection(const size_t index);
    //bool Enqueue(std::unique_ptr<BoltRequest> req, u64 client_id);

private:
    
    int controlfds[2];
    std::atomic<u64> ref_count;

    LockFreeQueue<std::shared_ptr<BoltRequest>> request_queue;
    LockFreeQueue<std::shared_ptr<DecoderTask>> response_queue;

    std::vector<std::shared_ptr<NeoConnection>> connection_pool;
    //std::shared_mutex connections_mutex;
    std::atomic<size_t> next_conn{0};
    std::atomic<bool> shutting_down{false};

    std::thread encoder_thread;
    std::thread decoder_thread;
    std::thread poll_thread;

    void Poll_Loop();
    void Dispatch_Encoder();
    void Decoder_Loop();
    void Scout_Loop();
    
    void Wait_Ref();
};