/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 24th of December 2025, Tuesday.
 * @date updated 15th of April 2026, Wednesday.
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/neoconnection.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|




//===============================================================================|
//          CLASS
//===============================================================================|
class NeoPool
{
public:

	NeoPool
	(
		int epfd_, 
		bool ssl,
		bool clustred,
		int id,
		size_t ncells, 
		std::string& urls,
		BoltValue* pauth, 
		BoltValue* pextras = nullptr
	);

	NeoPool(NeoPool&& other) noexcept
		: epfd(other.epfd),
		core_id(other.core_id),
		conns(std::move(other.conns)),   // move, not copy
		rr(other.rr.load())
	{
		// no need to clear other.cells, move already empties it
	}

	NeoPool& operator=(NeoPool&& other) noexcept 
	{
		if (this != &other) 
		{
			epfd = other.epfd;
			core_id = other.core_id;
			conns = std::move(other.conns);   // move, not copy
			rr.store(other.rr.load());
		}
		return *this;
	}

	NeoPool(const NeoPool&) = delete;
	NeoPool& operator=(const NeoPool&) = delete;

	NeoConnection* Acquire();
	inline const std::vector<std::unique_ptr<NeoConnection>>& Connections() const;
	void Close();

private:

	int epfd;
	int core_id;

	std::vector<std::unique_ptr<NeoConnection>> conns;
	std::atomic<size_t> rr{ 0 };
};