/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday.
 * @date updated 17th of March 2026, Tuesday.
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <sys/eventfd.h>
#include "neodriver.h"




//===============================================================================|
//          DEFINITON
//===============================================================================|
/**
 * @brief constructor
 */
NeoDriver::NeoDriver(const std::string& urls, BoltValue auth, BoltValue extras,
	const int pool_size_)
	: _urls(urls), _auth(std::move(auth)), pool(nullptr),
	pool_size(pool_size_ <= 0 ? POOL_SIZE : pool_size_)
{
	next_client_id = 0;
	_extras = BoltValue::Make_Map();

	// make sure every key in the extra map is in lowercase letters
	size_t items = extras.map_val.key_offset + extras.map_val.size;
	for (size_t v = extras.map_val.size, k = extras.map_val.key_offset; k < items; k++, v++)
	{
		BoltValue* bv = GetBoltPool<BoltValue>()->Get(v);
		std::string key = Utils::String_ToLower(GetBoltPool<BoltValue>()->Get(k)->ToString());
		_extras.Insert_Map(key, *bv);
	} // end for copy

	// create epoll instance for polling
	int n = std::thread::hardware_concurrency();
	epfds.resize(n);
	poll_threads.resize(n);

	looping.store(true, std::memory_order_release);
	for (int i = 0; i < n; i++)
	{
		epfds[i] = epoll_create1(0);	// no flags, no checks
		poll_threads[i] = std::thread(&NeoDriver::Poll_Read, this, epfds[i]);
		pool = new NeoCellPool(epfds[i], pool_size, _urls, &_auth, &_extras);
	} // end for
} // end constructor


/**
 * @brief destructor
 */
NeoDriver::~NeoDriver() 
{
	// release all memory allocated for auth and extras
	static size_t last_offset = 
		_auth.pool->Get_Last_Offset() > _extras.pool->Get_Last_Offset() ?
		_auth.pool->Get_Last_Offset() : _extras.pool->Get_Last_Offset();
	Release_Pool<BoltValue>(last_offset);
	Close();

	// kill pool pointer
	delete pool;
	pool = nullptr;
} // end destructor


/**
 * @brief executes async query using the next connection from the pool. If the connection is already
 *	connected it skips session start and executes the query. The function also passes the callback
 *	address to the cells Run_Async function.
 * 
 * @param cb the callback function to invoke per every stream ready
 * @param query the query to execute.
 * @param params parameters for cypher query above
 * @param extra info for cypher like r/w, db name, bookmarks etc.
 * 
 * @return LB_OK on success, alas LB_FAIL.
 */
LBStatus NeoDriver::Execute_Async(std::function<void(BoltResult&)> cb, 
	const char* query, 
	BoltValue&& params, BoltValue&& extra)
{
	NeoCell* pcell = pool->Acquire();
	if (!pcell) return LB_Make
	(
		LBAction::LB_FAIL,
		LBDomain::LB_DOM_DRIVER
	);

	LBStatus rc = pcell->Start_Session(++next_client_id);
	if (!LB_OK(rc))
	{
		last_err = pcell->err_desc;
		return rc;
	} // end if no session

	rc = pcell->Run_Async
	(
		cb, 
		query, 
		std::move(params), 
		std::move(extra)
	);
	if (!LB_OK(rc))
		last_err = pcell->err_desc;
	return rc;
} // end Execute_Async


/**
 * @brief this is the sync version of Execute_Async. It basically invokes Execute_Async
 *	with the first parameter or callback set to null; therefore caller can manually fetch
 *	later on via, Fetch().
 * 
 * @param query the query to execute.
 * @param params parameters for cypher query above
 * @param extra info for cypher like r/w, db name, bookmarks etc.
 * 
 * @return LB_OK on success, alas LB_FAIL.
 */
LBStatus NeoDriver::Execute(const char* query, BoltValue&& params,
	BoltValue&& extra)
{
	// invoke the async method with no callbacks
	return Execute_Async(nullptr, query, std::move(params), std::move(extra));
} // end Execute


void NeoDriver::Close()
{
	u64 my_exit = 1;
	exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

	struct epoll_event ev {};
	ev.events = EPOLLIN;
	ev.data.fd = exit_fd;

	pool->Stop();
	for (int i = 0; i < epfds.size(); i++)
	{
		epoll_ctl(epfds[i], EPOLL_CTL_ADD, exit_fd, &ev);
		write(exit_fd, &my_exit, sizeof(my_exit));
		if (poll_threads[i].joinable()) poll_threads[i].join();
		CLOSE(epfds[i]);
	} // end if 

	CLOSE(exit_fd);
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
	return last_err;
} // end Get_Last_Error


NeoCell* NeoDriver::Get_Session()
{
	NeoCell* pcell = pool->Acquire();
	last_rc = pcell->Start_Session(++next_client_id);

	if (!LB_OK(last_rc))
	{
		last_err = pcell->Get_Last_Error();
		return nullptr;
	} // end if Get_Session

	return pcell;
} // end pcell


NeoCellPool* NeoDriver::Get_Pool()
{
	for (int i = 0; i < pool->Workers().size(); i++)
	{
		auto* p = pool->Acquire();
		p->Start_Session(next_client_id++);
	}
		
	return pool;
} // end Get_Pool


void NeoDriver::Poll_Read(int epfd)
{
	while (looping.load(std::memory_order_acquire))
	{
		int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000); // 1 second timeout
		for (int n = 0; n < nfds; ++n)
		{
			NeoCell* pcell = static_cast<NeoCell*>(events[n].data.ptr);
			if (events[n].events & EPOLLIN)
			{
				if (events[n].data.fd == exit_fd)
				{
					uint64_t val;
					read(exit_fd, &val, sizeof(val)); // clear it
					looping.store(false, std::memory_order_relaxed);
					break;
				}

				// ready to read, read it in and push task to decoder
				LBStatus rc = LB_Make(LBAction::LB_HASMORE);
				while (LB_Action(rc) == LBAction::LB_HASMORE)
				{
					rc = pcell->Poll_Read();
					if (!LB_OK(rc))
					{
						if (LBAction(LB_Action(rc)) == LBAction::LB_FAIL)
						{
							pcell->Stop();	// kill it
							break;
						} // end if fail
						else break; // should be wait only
					} // end if error

					// now begin decoding, if we have a full message
					rc = pcell->Decode_Response(pcell->Get_Read_Buffer_Read_Ptr(), LB_Aux(rc));
					/*if (pcell->tasks.Is_Empty() && pcell->requests.Is_Empty())
						pcell->Reset_Read_Buffer();*/
				} // end while
			} // end if readable
		} // end for nfds
	} // end while looping
} // end Poll_Read