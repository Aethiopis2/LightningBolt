/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 24th of December 2025, Tuesday
 * @date updated 18th of January 2026, Sunday
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "neocell.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|




//===============================================================================|
//          CLASS
//===============================================================================|
class NeoCellPool
{
public:

	NeoCellPool(size_t nworkers, std::string& urls,
		BoltValue* pauth, 
		BoltValue* pextras = nullptr);

	LBStatus Start(const bool all_connections = false);
	void Stop();
	NeoCell* Acquire();
	const std::vector<std::unique_ptr<NeoCell>>& Workers() const;

private:

	std::vector<std::unique_ptr<NeoCell>> workers;	// pool of workers
	std::atomic<size_t> idx_counter{ 0 };
};