/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 24th of December 2025, Tuesday
 * @date updated 24th of Decemeber 2025, Tuesday
 *
 * @copyright Copyright (c) 2025
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neocell.h"
#include "utils/lock_free_queue.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
using CellCallback = void(*)(int result, void* user);
using FetchCallback = void(*)(BoltMessage* msg, int status, void* user);


enum class CellCmdType 
{
    Run,
    Begin,
    Commit,
    Rollback,
    Pull,
    Discard,
    Reset,
    Goodbye,
    Fetch
};


struct CellCommand 
{
    CellCmdType type;

    std::string cypher;
    BoltValue params;
    BoltValue extras;
    int n = -1;

    CellCallback  cb = nullptr;
    FetchCallback fetch_cb = nullptr;
    void* user = nullptr;
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCellWorker
{
public:

    /**
     * @brief constructor
	 */
    explicit NeoCellWorker(BoltValue params)
        : cell(std::move(params)) { }


    /**
	 * @brief starts the worker thread
     */
    void Start()
    {
        running.store(true);
        worker_thread = std::thread(&NeoCellWorker::Worker_Loop, this);
	} // end Start


    /**
     * @brief stops the worker thread and joins it
	 */
    void Stop()
    {
        running.store(false);
        if (worker_thread.joinable())
            worker_thread.join();
	} // end Stop


    /**
	 * @brief enqueues a command into the command queue
     */
    void Enqueue(CellCommand&& cmd)
    {
        while (!queue.Enqueue(std::move(cmd)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
	} // end Enqueue_Command

private:

	NeoCell cell;                           // the underlying NeoCell instance
	std::atomic<bool> running{ false };     // a loop control flag
    std::thread worker_thread;              // thread handle
	LockFreeQueue<CellCommand> queue;       // command queue; handles multiple requests


    /**
     * @brief the main worker loop that processes commands from the queue
	 */
    void Worker_Loop()
    {
		cell.Start();

        while (running.load())
        {
            auto cmd_opt = queue.Dequeue();
            if (!cmd_opt.has_value())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
			} // end if no cmd

            Execute(cmd_opt.value());
		} // end while running

		cell.Stop();
	} // end Worker_Loop


    /**
	 * @brief executes a given command on the NeoCell instance
     */
    void Execute(CellCommand& cmd)
    {
        int rc = 0;

        switch (cmd.type)
        {
        case CellCmdType::Run:
            rc = cell.Run(
                cmd.cypher.c_str(),
                std::move(cmd.params),
                std::move(cmd.extras),
                cmd.n
            );
            if (cmd.cb) cmd.cb(rc, cmd.user);
            break;

        case CellCmdType::Begin:
            rc = cell.Begin(cmd.params);
            if (cmd.cb) cmd.cb(rc, cmd.user);
            break;

        case CellCmdType::Commit:
            rc = cell.Commit(cmd.params);
            if (cmd.cb) cmd.cb(rc, cmd.user);
            break;

        case CellCmdType::Fetch:
            Async_Fetch(cmd);
            break;

        default:
            break;
		} // end switch
	} // end Execute


    /**
     * @brief asynchronously fetches records and invokes the callback for each record
	 */
    void Async_Fetch(CellCommand& cmd) 
    {
        BoltMessage msg;

        while (true) 
        {
            int rc = cell.Fetch(msg);

            if (rc > 0) 
            {
                if (cmd.fetch_cb)
                    cmd.fetch_cb(&msg, 0, cmd.user);
			} // end if more records
            else if (rc == 0) 
            {
                if (cmd.fetch_cb)
                    cmd.fetch_cb(nullptr, 0, cmd.user);
                break;
			} // end else if done
            else 
            {
                if (cmd.fetch_cb)
                    cmd.fetch_cb(nullptr, rc, cmd.user);
                break;
			} // end else error
		} // end while
	} // end Async_Fetch
};