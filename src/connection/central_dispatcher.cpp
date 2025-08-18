/**
 * @file central_dispatcher.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief 
 * @version 1.0
 * @date 14th of May 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */

//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <emmintrin.h>
#include "connection/central_dispatcher.h"
#include "bolt/bolt_encoder.h"
#include "bolt/bolt_decoder.h"
#include "utils/errors.h"





//===============================================================================|
//         CLASS
//===============================================================================|
/**
 * @brief Initializes the connection pool, threads, and internal queues.
 *  Launches the encoder, decoder, and polling threads.
 *  Aborts if no connection could be successfully established.
 * 
 * @param connection_string http formatted string: http://hostname:port@username:password
 *  with/without http:// pre-text
 * @param connection_count the number of connections stored in pool; is determined
 *  by a formula that uses 50% of hardware threads for itself and upper
 *  protocols.
 */
void CentralDispatcher::Init(const std::string &connection_string, 
    const size_t connection_count)
{
    //Shutdown();  // idempotent and safe to call even if not started

    shutting_down.store(false);
    ref_count.store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < connection_count; i++)
    {
        auto conn = std::make_shared<NeoConnection>(connection_string, i);
        if (conn->Start() < 0)
        {
            Dump_Err("connect failed");
            continue;
        } // end if

        Add_Ref();
        connection_pool.emplace_back(std::move(conn));
    } // end for 

    //Print("Inside constructor: %d", Get_Ref());
    if (connection_pool.empty())
        Fatal("all connections failed");

    // (Re)initialize pipe
    if (controlfds[0] != -1) close(controlfds[0]);
    if (controlfds[1] != -1) close(controlfds[1]);
    if (pipe(controlfds) < 0)
        Fatal("pipe failed");
    
    encoder_thread = std::thread(&CentralDispatcher::Dispatch_Encoder, this);
    decoder_thread = std::thread(&CentralDispatcher::Decoder_Loop, this);
    poll_thread = std::thread(&CentralDispatcher::Poll_Loop, this);
} // end Init


//===============================================================================|
/**
 * @brief Queues a new request to be picked up by encoder/decoder threads.
 * 
 * @param req the request containg the query and possibly its own callback
 */
void CentralDispatcher::Submit_Request(std::shared_ptr<BoltRequest> req)
{
    Add_Ref();
    request_queue.Enqueue(std::move(req));
} // end Submit_Request


//===============================================================================|
/**
 * @brief Queues a new response to be picked up by encoder/decoder threads.
 * 
 * @param res the response structure
 */
void CentralDispatcher::Submit_Response(std::shared_ptr<DecoderTask> task)
{
    response_queue.Enqueue(std::move(task));
} // end Submit_Response


//===============================================================================|
/**
 * @brief Signals all dispatch threads to terminate and waits for graceful shutdown.
 *  Also triggers poller pipe to wake and exit.
 */
void CentralDispatcher::Shutdown()
{
    // drain all connections first
    Wait_Ref();

    shutting_down.store(true);
    if (encoder_thread.joinable()) encoder_thread.join();
    if (decoder_thread.joinable()) decoder_thread.join();

    write(controlfds[1], "x", 1);
    if (poll_thread.joinable()) poll_thread.join();

    if (controlfds[0] != -1)
    {
        close(controlfds[0]);
        controlfds[0] = -1;
    } // end if

    if (controlfds[1] != -1)
    {
        close(controlfds[1]);
        controlfds[1] = -1;
    } // end if

    connection_pool.clear();
} // end Shutdown


//===============================================================================|
/**
 * @returns Returns the next available READY connection (round-robin) or nullptr 
 */
std::shared_ptr<NeoConnection> CentralDispatcher::Get_Connection()
{
    size_t count = connection_pool.size();
    size_t start = next_conn.fetch_add(1) % count;

    for (size_t i = 0; i < count; ++i)
    {
        size_t index = (start + i) % count;
        auto& conn = connection_pool[index];

        if (conn->Get_State() == BoltState::Ready)
        {
            return conn;
        }
    }

    return nullptr; // No ready connection found
} // end Get_Connection


//===============================================================================|
/** 
 * @returns the connection at the given index 
 */
std::shared_ptr<NeoConnection> CentralDispatcher::Get_Connection(const size_t index)
{
    if (index >= connection_pool.size())
        return nullptr;     // out of bounds

    return connection_pool[index]; // give it back.
} // end Get_Connection


//===============================================================================|
/**
 * @brief invoke's the next connection fetch, not really useful in multithreaded
 *  net apps.
 */
// BoltValue CentralDispatcher::Fetch()
// {
//     std::shared_ptr<NeoConnection> conn = Get_Connection();
//     return conn->Fetch();
// } // end Fetch


//===============================================================================|
/**
 * @brief Waits for I/O events using poll() and delegates readable events to connections.
 *  Controlled via an internal pipe to allow thread-safe wakeup and shutdown.
 */
void CentralDispatcher::Poll_Loop()
{
    int i, n_ready;
    while (!shutting_down)
    {
        std::vector<struct pollfd> pfds;
        pfds.push_back({controlfds[0], POLLIN, 0});


        for (i = 0; i < connection_pool.size(); i++)
        {
            pfds.push_back(connection_pool[i]->Get_Pollfd());
        } // end for

        n_ready = POLL(pfds.data(), pfds.size(), -1);
        if (n_ready == 0)
        {
            continue;
        }
        
        if (n_ready < 0)
        {
            Dump_Err("poll");
            break;
        } // end if

        if (pfds[0].revents & POLLIN)
        {
            char buf[64];
            read(controlfds[0], buf, 64);   // drain
            break;
        } // end if

        for (i = 1; i < pfds.size(); i++)
        {
            if (pfds[i].revents & POLLIN)
            {
                connection_pool[i-1]->Poll_Readable();
            } // end if
        } // end for
    } // end while

    // cleanup
    for (auto& conn : connection_pool)
        conn->Stop();
} // end Poll_Loop


//===============================================================================|
/**
 * @brief Dequeues requests and runs them on READY connections.
 */
void CentralDispatcher::Dispatch_Encoder()
{
    while (!shutting_down || !request_queue.Is_Empty())
    {
        std::shared_ptr<BoltRequest> req; 
        auto r = request_queue.Dequeue();
        if (r.has_value())
            req = r.value();

        if (req)
        {
            auto conn = Get_Connection();
            if (conn)
                conn->Run_Query(req);
            else
            {
                request_queue.Enqueue(std::move(req));
            }
        } // end if
        else 
        {
            // if (shutting_down.load(std::memory_order_acquire))
            //     break;
            // else
                _mm_pause();
        } // end else
    } // end while
} // end Encoder_Loop


//===============================================================================|
/**
 * @brief Dequeues decoded Bolt responses and dispatches to the appropriate connection
 */
void CentralDispatcher::Decoder_Loop()
{
    while (!shutting_down || !response_queue.Is_Empty())
    {
        std::shared_ptr<DecoderTask> task;
        auto t = response_queue.Dequeue();
        if (t.has_value())
            task = t.value();
            
        if (task)
        {
            if (task->conn->Get_State() != BoltState::Disconnected) 
            {
                task->conn->Decode_Response(task->view, task->bytes);
                Sub_Ref();
                //Print("Inside Decoder loop: %d", Get_Ref());
            }
            else {response_queue.Enqueue(std::move(task));}
        } // end if
        else 
        {
            //_mm_pause();
            std::this_thread::yield();
        } // end else
    } // end while
} // end Decoder_Loop


//===============================================================================|
void CentralDispatcher::Scout_Loop()
{
    
} // end Scout_Loop


//===============================================================================|
void CentralDispatcher::Add_Ref()
{
    ref_count.fetch_add(1, std::memory_order_relaxed);
} // end Scout_Loop


//===============================================================================|
void CentralDispatcher::Sub_Ref()
{
    u64 prev = ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1)
        ref_count.notify_all();
} // end Scout_Loop


//===============================================================================|
void CentralDispatcher::Wait_Ref()
{
    u64 prev = Get_Ref();
    while (prev > 0)
    {
        ref_count.wait(prev);
        prev = Get_Ref();
    } // end while
} // end Scout_Loop


//===============================================================================|
u64 CentralDispatcher::Get_Ref() const
{
    return ref_count.load(std::memory_order_acquire);
} // end Scout_Loop