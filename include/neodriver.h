/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday.
 * @date updated 15th of April 2026, Wednesday.
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "neopool.h"
#include "neosession.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
constexpr static int MAX_EVENTS = 1024;
constexpr static int POOL_SIZE = 1;



/**
 * @brief structure that allows different contexts per core and allows for 
 *  NUMA aware pool structures.
 */
class CoreContext
{
public:

    int core_id;
    int epfd;
    int exit_fd;

    NeoPool pool;
    NeoSession session;

    std::thread poll_thread;
    std::atomic<bool> running{ true };

    CoreContext
    (
        int id,
        int epfd_,
		int exit_fd_,
        NeoPool&& pool_
    ) : core_id(id), epfd(epfd_), exit_fd(exit_fd_), 
		pool(std::move(pool_)), session(pool)
    {
        struct epoll_event ev {};
        ev.events = EPOLLIN;
        ev.data.fd = exit_fd;

        epoll_ctl(epfd, EPOLL_CTL_ADD, exit_fd, &ev);
	} // end constructor

	CoreContext(const CoreContext&) = delete;
	CoreContext& operator=(const CoreContext&) = delete;

    CoreContext(CoreContext&& other) noexcept
        : core_id(other.core_id), epfd(other.epfd), exit_fd(other.exit_fd),
          pool(std::move(other.pool)), session(pool),
          poll_thread(std::move(other.poll_thread)), running(other.running.load())
    {
	} // end move constructor

    CoreContext& operator=(CoreContext&& other) noexcept
    {
        if (this != &other)
        {
            core_id = other.core_id;
            epfd = other.epfd;
            exit_fd = other.exit_fd;
            pool = std::move(other.pool);
            session = NeoSession(pool);
            poll_thread = std::move(other.poll_thread);
            running.store(other.running.load());
        }
        return *this;
	} // end move assign
};


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoDriver
{
public:

    NeoDriver(std::string urls, BoltValue auth,
        BoltValue extra = BoltValue::Make_Map(),
        const int pool_size_ = POOL_SIZE);
    ~NeoDriver();

    LBStatus Execute_Async(std::function<void(BoltResult&)> cb, const char* query,
        BoltValue&& params = BoltValue::Make_Map(), BoltValue&& extra = BoltValue::Make_Map());
    LBStatus Execute(const char* query, BoltValue&& params = BoltValue::Make_Map(),
        BoltValue&& extra = BoltValue::Make_Map());
	LBStatus Get_Session(NeoSession& handle);
    int Fetch(BoltResult& result);

    void Close();

    std::string Get_Last_Error() const;

private:

    std::string _urls;       // raw unfiltered url string for database connection
    BoltValue _auth;         // authentication token
    BoltValue _extras;       // any extra connection params (power user mode, not me).

    u64 next_client_id;     // id for the next connection in the pool
	bool ssl_on;           // ssl on or off
	bool clustred;         // single or cluster

    std::vector<CoreContext> _cores;
    std::atomic<size_t> rr_core{ 0 };

    inline size_t Next_Core();
    void Poll_Loop(CoreContext& ctx);
};