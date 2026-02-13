/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday
 * @date updated 10th of Feburary 2026, Tuesday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include <sys/eventfd.h>
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
	exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

	struct epoll_event ev {};
	ev.events = EPOLLIN;
	ev.data.fd = exit_fd;

	epoll_ctl(epfd, EPOLL_CTL_ADD, exit_fd, &ev);

	// start the polling thread
	looping.store(true, std::memory_order_acquire);
	poll_thread = std::thread(&NeoDriver::Poll_Read, this);
} // end constructor


/**
 * @brief destructor
 */
NeoDriver::~NeoDriver() 
{
	static size_t last_offset = auth.pool->Get_Last_Offset() > extras.pool->Get_Last_Offset() ?
		auth.pool->Get_Last_Offset() : extras.pool->Get_Last_Offset();
	Release_Pool<BoltValue>(last_offset);
	Close();
} // end destructor


int NeoDriver::Execute(std::string& query, std::map<std::string, std::string> params)
{
	// get the next instance from the pool, and execute on that
	NeoCell* cell = pool.Acquire();
	if (!cell)
		return -3;  // error acquiring cell

	// make sure its connected first, if not connect
	LBStatus rc = Start_Session(cell);
	if (!LB_OK(rc))
		return -1;

	CellCommand cmd;
	cmd.type = CellCmdType::Run;
	cmd.cypher = query;
	cmd.params = BoltValue::Make_Map();
	cmd.extras = BoltValue::Make_Map();

	// just pass to pool
	return cell->Enqueue_Request(std::move(cmd));
} // end Execute


int NeoDriver::Execute_Async(std::string query, std::function<void(BoltResult&)> cb,
	std::map<std::string, std::string> params)
{
	// get the next instance from the pool, and execute on that
	NeoCell* cell = pool.Acquire();
	if (!cell)
		return -3;  // error acquiring cell

	// make sure its connected first, if not connect
	LBStatus rc = Start_Session(cell);
	if (!LB_OK(rc))
		return -1;

	// now form query parameters and pass to cell
	CellCommand cmd;
	cmd.type = CellCmdType::Run;
	cmd.cypher = query;
	cmd.params = BoltValue::Make_Map();
	cmd.extras = BoltValue::Make_Map();
	cmd.cb = cb;

	// just pass to pool
	return cell->Enqueue_Request(std::move(cmd));
} // end Execute_Async
 


void NeoDriver::Close()
{
	u64 my_exit = 1;

	pool.Stop();
	write(exit_fd, &my_exit, sizeof(my_exit));
	if (poll_thread.joinable()) poll_thread.join();

	CLOSE(exit_fd);
	CLOSE(epfd);
} // end Close


void NeoDriver::Set_Pool_Size(const int nsize)
{
	pool_size = nsize;
} // end Set_Pool_Size


int NeoDriver::Get_Pool_Size() const
{
	return pool_size;
} // end Get_Pool_Size


std::string NeoDriver::Get_Last_Error() const
{
	LBDomain domain = LBDomain(LB_Domain(last_rc));
	if (domain == LBDomain::LB_DOM_SYS || domain == LBDomain::LB_DOM_SSL)
		return LB_Error_String(last_rc);
	else if (domain == LBDomain::LB_DOM_NEO4J)
		return last_err;
} // end Get_Last_Error


NeoCell* NeoDriver::Get_Session()
{
	NeoCell* pcell = pool.Acquire();
	last_rc = Start_Session(pcell);

	if (!LB_OK(last_rc))
	{
		last_err = pcell->Get_Last_Error();
		return nullptr;
	} // end if Get_Session

	return pcell;
} // end pcell


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
				if (events[n].data.fd == exit_fd)
				{
					uint64_t val;
					read(exit_fd, &val, sizeof(val)); // clear it
					looping.store(false, std::memory_order_relaxed);
					break;
				}
				// ready to read
				cell->DWake();	// wake the decoder thread
			} // end if readable
		} // end for nfds
	} // end while looping
} // end Poll_Read


/**
 * @brief starts a new connection with neo4j server or session. Once connected
 *	it registers the socket for epoll activity, so that our thread wakes up
 *	only when socket is ready.
 * 
 * @param pcell pointer to a cell/session object
 * 
 * @return LB_OK on success. On fail LB_RETRY or LB_FAIL depending on
 *	the stack that failed.
 */
LBStatus NeoDriver::Start_Session(NeoCell* pcell)
{
	if (pcell->Is_Connected()) return LB_Make();

	LBStatus rc = pcell->Start();
	if (!LB_OK(rc)) 
		return rc;

	// register it to epoll for it to actively poll for events
	epoll_event ev = {};
	ev.events = EPOLLIN | EPOLLET;
	ev.data.ptr = pcell;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, pcell->Get_Socket(), &ev) < 0)
	{
		return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_SYS,
			LBCode::LB_CODE_NONE, errno);
	} // end if error adding to epoll

	return rc;
} // end Start_Session