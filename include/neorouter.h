/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 15th of April 2026, Wednesday.
 * @date updated 15th of April 2026, Wednesday.
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neopool.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
struct RoutingTable
{
	std::vector<NeoCell*> leaders;
	std::vector<NeoCell*> followers;
};


std::atomic<RoutingTable*> g_routing{ nullptr };



//===============================================================================|
//          CLASS
//===============================================================================|
class NeoRouter
{
public:

	NeoRouter(NeoPool& pool_) : pool(pool_) {}

	NeoRouter(NeoRouter&& other)
		: pool(other.pool), rr_leaders(other.rr_leaders.load()), rr_followers(other.rr_followers.load())
	{
	} // end move constructor

	NeoRouter& operator=(NeoRouter&& other)
	{
		if (this != &other)
		{
			pool = std::move(other.pool);
			rr_leaders.store(other.rr_leaders.load());
			rr_followers.store(other.rr_followers.load());
		} // end if not self assign

		return *this;
	} // end move assign

	inline LBStatus Execute(CellCommand& cmd)
	{
		constexpr int MAX_RETRIES = 3;

		for (int i = 0; i < MAX_RETRIES; ++i)
		{
			NeoCell* cell = Acquire(cmd.mode);
			if (!cell) return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);

			LBStatus rc = cell->Start_Session();
			if (!LB_OK(rc))
				return rc;

			rc = cell->Execute_Command(cmd);
			if (!LB_OK(rc))
				return rc;

			LBAction act = LB_Action(rc);

			if (act == LBAction::LB_REROUTE)
			{
				Refresh_Routing();
				continue;
			} // end if refresh routing

			if (act == LBAction::LB_RETRY)
				continue;

			return rc;
		} // end for

		return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);
	} // end Execute

private:

	NeoPool& pool;
	std::atomic<size_t> rr_leaders{ 0 };	// round robin pointer for leader connections
	std::atomic<size_t> rr_followers{ 0 };	// round robin pointer for follower connections


	inline NeoCell* Acquire(QueryMode mode)
	{
		auto* table = g_routing.load(std::memory_order_acquire);
		if (!table) return pool.Acquire();		// fallback

		if (mode == QueryMode::Write)
		{
			auto& v = table->leaders;
			if (v.empty()) return pool.Acquire();

			size_t idx = rr_leaders.fetch_add(1, std::memory_order_relaxed);
			return v[idx % v.size()];
		} // end if leader
		else
		{
			auto& v = table->followers;
			if (v.empty()) return pool.Acquire();

			size_t idx = rr_followers.fetch_add(1, std::memory_order_relaxed);
			return v[idx % v.size()];
		} // end else
	} // end Acquire


	void Refresh_Routing()
	{
		NeoCell* seed = pool.Acquire();
		if (!seed) return;

		/*auto* new_table = seed->connection.Route();
		g_routing.store(new_table, std::memory_order_release);*/
	} // end Refersh_Routing
};