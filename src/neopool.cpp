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
NeoPool::NeoPool
(
	int epfd_,
	bool ssl,
	bool clustered_,
	int id,
	size_t ncells, 
	std::string& urls, 
	BoltValue* pauth, 
	BoltValue* pextras)
	: epfd(epfd_), core_id(id), clustred(clustered_)
{
	conns.reserve(ncells);
	for (size_t i = 0; i < ncells; ++i)
	{
		conns.emplace_back(new NeoConnection(ssl, urls, pauth, pextras));
	} // end for
} // end constructor


/**
 * @brief returns the next cell in the pool based on the acquire function pointer 
 *	function address set by the constructor.
 */
NeoConnection* NeoPool::Acquire(const ROLE role)
{
	if (!clustred || role == ROLE::AUTO)
	{
		size_t idx = rr.fetch_add(1, std::memory_order_relaxed);
		return conns[idx % conns.size()].get();
	} // end if normal
	else
	{
		size_t count = 0;
		size_t idx = rr.fetch_add(1, std::memory_order_relaxed);
		NeoConnection* pc = conns[idx % conns.size()].get();
		while (pc->Get_Connection_Role() != role && count++ < conns.size())
		{
			idx = rr.fetch_add(1, std::memory_order_relaxed);
			pc = conns[idx % conns.size()].get();
		} // end while

		return pc;
	} // end else
} // end Acquire


/**
 * @brief gets the list of all cells
 */
inline const std::vector<std::unique_ptr<NeoConnection>>& NeoPool::Connections() const
{
	return conns;
} // end cells


/**
 * @brief closes all cells in the pool by invoking their Stop() method.
 */
void NeoPool::Close()
{
	for (auto& con : conns)
	{
		con->Terminate();
	} // end for
} // end Close