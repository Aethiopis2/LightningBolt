/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 18th of January 2026, Sunday
 * @date updated 18th of January 2026, Sunday
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "neopool.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief constructor
 */
NeoCellPool::NeoCellPool(size_t nworkers, std::string& urls, BoltValue* pauth,
	BoltValue* pextras)
{
	for (size_t i = 0; i < nworkers; ++i)
		workers.emplace_back(new NeoCell(urls, pauth, pextras));
} // end constructor


/**
 * @brief the function either starts single connection which is the next inline for
 *	processing the next request or starts all connections for an egar style 
 *	connection depending on the value of the parameter passed
 * 
 * @param all_connections a boolean that when set true all connections should start
 * 
 * @return 0 on success or -ve number indicating error
 */
int NeoCellPool::Start(const bool all_connections)
{
	int rc;		// return value from functions

	if (!all_connections)
	{
		// start the next connection on the pool if not already
		//	running
		int idx = idx_counter.load(std::memory_order_acquire) % workers.size();
		if ((rc = workers[idx].get()->Start(idx)) < 0)
			return rc;
	} // end if start a single connection only
	else
	{
		for (auto& w : workers)
		{
			if ((rc = w->Start()) < 0)
				return rc;
		} // end foreach workers
	} // end else start everything

	return 0;
} // end Start


/**
 * @brief stops all workers in the pool
 */
void NeoCellPool::Stop()
{
	for (auto& w : workers)
		w->Stop();
} // end Stop


/**
 * @brief gets a worker from the pool in a round-robin fashion
 */
NeoCell* NeoCellPool::Acquire()
{
	int idx = idx_counter.fetch_add(1, std::memory_order_relaxed) % workers.size();
	return workers[idx].get();
} // end Acquire


/**
 * @brief gets the list of all workers
 */
const std::vector<std::unique_ptr<NeoCell>>& NeoCellPool::Workers() const
{
	return workers;
} // end Workers