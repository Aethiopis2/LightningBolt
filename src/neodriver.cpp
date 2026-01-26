/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday
 * @date updated 18th of January 2026, Sunday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neodriver.h"



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
const int POOL_SIZE = 1;



//===============================================================================|
//          DEFINITON
//===============================================================================|
NeoDriver::NeoDriver(const std::string& urls, BoltValue auth, BoltValue extras)
	: urls(urls), auth(auth), pool(POOL_SIZE, this->urls, &this->auth, &this->extras),
	pool_size(POOL_SIZE)
{
	this->extras = BoltValue::Make_Map();

	// make sure every key in the extra map is in lowercase letters
	size_t items = extras.map_val.key_offset + extras.map_val.size;
	for (size_t v = extras.map_val.size, k = extras.map_val.key_offset; k < items; k++, v++)
	{
		BoltValue* bv = GetBoltPool<BoltValue>()->Get(v);
		std::string key = Utils::String_ToLower(GetBoltPool<BoltValue>()->Get(k)->ToString());
		this->extras.Insert_Map(key, *bv);
	} // end for copy

	// create epoll instance for polling
	epfd = epoll_create1(0);	// no flags, no checks

	// start the polling thread
	looping.store(true, std::memory_order_acquire);
	poll_thread = std::thread(&NeoDriver::Poll_Read, this);
} // end constructor


/**
 * @brief destructor
 */
NeoDriver::~NeoDriver() {}


int NeoDriver::Execute_Async(std::string& query, std::function<void(BoltResult&)> cb,
	std::map<std::string, std::string> params)
{
	// get the next instance from the pool, and execute on that
	NeoCell* cell = pool.Acquire();
	if (!cell)
		return -3;  // error acquiring cell

	// make sure its connected first, if not connect
	if (!cell->Is_Connected())
	{
		int rc = cell->Start();
		if (rc < 0)
		{
			pool.Stop();
			return rc;    // error starting connection
		} // end if error starting

		// register it to epoll for it to actively poll for events
		epoll_event ev = {};
		ev.events = EPOLLIN | EPOLLET;
		ev.data.ptr = cell;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, cell->Get_Socket(), &ev) < 0)
		{
			pool.Stop();
			return -4;  // error adding to epoll
		} // end if error adding to epoll
	} // end if not connected

	// now form query parameters and pass to cell
	CellCommand cmd;
	cmd.type = CellCmdType::Run;
	cmd.cypher = "UNWIND range(1,100) AS n RETURN n";
	cmd.params = BoltValue::Make_Map();
	cmd.extras = BoltValue::Make_Map();
	cmd.cb = cb;

	// just pass to pool
	return cell->Enqueue_Request(std::move(cmd));
} // end Execute_Async



void NeoDriver::Set_Pool_Size(const int nsize)
{
	pool_size = nsize;
} // end Set_Pool_Size


int NeoDriver::Get_Pool_Size() const
{
	return pool_size;
} // end Get_Pool_Size


void NeoDriver::Poll_Read()
{
	while (looping.load(std::memory_order_acquire))
	{
		int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000); // 1 second timeout
		for (int n = 0; n < nfds; ++n)
		{
			NeoCell* cell = static_cast<NeoCell*>(events[n].data.ptr);
			if (events[n].events & EPOLLIN)
			{
				// ready to read
				cell->DWake();	// wake the decoder thread
			} // end if readable
		} // end for nfds
	} // end while looping
} // end Poll_Read