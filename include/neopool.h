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
#include "neocellworker.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|




//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCellPool
{
public:

	/**
	 * @brief constructor
	 */
	NeoCellPool(size_t nworkers, BoltValue conn_params)
	{
		for (size_t i = 0; i < nworkers; ++i)
		{
			workers.emplace_back(new NeoCellWorker(conn_params));
		} // end for nworkers
	} // end constructor


	/**
	 * @brief starts all workers in the pool
	 */
	void Start()
	{
		for (auto& w : workers)
			w->Start();
	} // end Start


	/**
	 * @brief stops all workers in the pool
	 */
	void Stop()
	{
		for (auto& w : workers)
			w->Stop();
	} // end Stop


	/**
	 * @brief gets a worker from the pool in a round-robin fashion
	 */
	NeoCellWorker* Acquire()
	{
		size_t i = rr.fetch_add(1, std::memory_order_relaxed);
		return workers[i % workers.size()].get();
	} // end Acquire

private:

	std::vector<std::unique_ptr<NeoCellWorker>> workers;	// pool of workers
	std::atomic<size_t> rr{ 0 };
};