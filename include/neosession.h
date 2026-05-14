/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 20th of April 2026, Sunday.
 * @date updated 20th of April 2026, Sunday.
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neopool.h"



#ifdef LB_DEBUG
#define LB_THREAD_ID std::this_thread::get_id()
#define LB_ASSERT_THREAD(expected) \
        if (std::this_thread::get_id() != expected) \
            std::abort()
#else
#define LB_THREAD_ID 0
#define LB_ASSERT_THREAD(x) ((void)0)
#endif



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
class NeoDriver;



//===============================================================================|
//          CLASS
//===============================================================================|
class NeoSession
{
	friend class NeoDriver;

public:

	NeoSession() : pconn(nullptr), pool(*(NeoPool*)nullptr) {}
	NeoSession(NeoPool& _pool) : pconn(nullptr), pool(_pool) {}
	NeoSession(NeoSession&& other) : pconn(other.pconn), pool(other.pool) { }
	~NeoSession() { Close(); }

	NeoSession& operator=(NeoSession&& other)
	{
		if (this != &other)
		{
			Close();
			pool = std::move(other.pool);
			pconn = other.pconn;
			other.pconn = nullptr;
		}
		return *this;
	}

    LBStatus Run_Async(
		std::function<void(BoltResult&)> cb,
		const char* query,
        BoltValue&& params = BoltValue::Make_Map(),
        BoltValue&& extra = BoltValue::Make_Map())
    {
        CHECK_THREAD();
        if (!pconn) return FAIL();

		ConnectionCommand cmd;
		cmd.cypher = query;
		cmd.param = std::move(params);
		cmd.extra = std::move(extra);
		cmd.mode = QueryMode::Read;
		cmd.type = ConnectionCmdType::Run;
		cmd.cb = std::move(cb);
		return pconn->Run(cmd);
	} // end Run


	/**
	 * @brief runs a query synchronously and waits for the result to be ready before returning. This is a blocking call.
	 *
	 * @param query the Cypher query string to execute.
	 * @param params a map of parameters to pass with the query. This is optional and defaults to an empty map.
	 * @param extra a map of extra options to pass with the query. This is optional and defaults to an empty map.
	 * 
	 * @return LB_OK on success, LB_FAIL on failure.
	 */
	LBStatus Run(
		const char* query,
		BoltValue&& params = BoltValue::Make_Map(),
		BoltValue&& extra = BoltValue::Make_Map())
	{
		return Run_Async(nullptr, query, std::move(params), std::move(extra));
	} // end Run


    /**
	 * @brief runs a query asynchronously and invokes the provided callback with the result when ready.
     * 
	 * @param result the result of the query execution, which can be iterated over for records and summary.
	 *
	 * @return LB_OK on success, LB_FAIL on failure, LB_HASMORE if there are more records to fetch.
     */
    LBStatus Fetch(BoltResult& result)
    {
        CHECK_THREAD();
        if (!pconn) return FAIL();

		// wait for at least one full message
		pconn->Wait_For_Response();

		auto& results = pconn->results.Front()->get();

		result = std::move(results);
		pconn->latencies.Record_Latency
		(
			std::chrono::high_resolution_clock::now() -
			result.start_clock
		);
		if (result.done)
		{
			pconn->results.Dequeue();
			return LB_Make();
		} // end if done

		return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_NEO4J);
	} // end Fetch


    /**
	 * @brief starts a transaction with the given options. Options can include things like
	 *  bookmarks, database name, tx timeout, tx metadata etc. 
     * 
	 * @param options a map of options for the transaction.
     *
	 * @return LB_OK on success, LB_FAIL on failure.
     */
    LBStatus Begin(BoltValue&& options = BoltValue::Make_Map())
    {
        CHECK_THREAD();
		ConnectionCommand cmd;
		cmd.extra = std::move(options);
		cmd.mode = QueryMode::Read;
		cmd.type = ConnectionCmdType::Begin;
		return pconn ? pconn->Begin(cmd) : FAIL();
    } // end Begin


	LBStatus Start_Session(int epfd, int client_id)
	{
		if (!pconn)
		{
			pconn = pool.Acquire();
		} // end if no connection

		return pconn->Start_Session(epfd, client_id);
	} // end Start_Session


    /**
	 * @brief commits the current transaction with the given options. Options can include things like
	 *  bookmarks, database name, tx timeout, tx metadata etc.
     * 
	 * @param options a map of options for the transaction.
     * 
     * @return LB_OK on success, LB_FAIL on failure.
	 */
    LBStatus Commit(BoltValue&& options = BoltValue::Make_Map())
    {
        CHECK_THREAD();
        ConnectionCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = ConnectionCmdType::Commit;

        return pconn ? pconn->Commit(cmd) : FAIL();
	} // end Commit


    /**
     * @brief rolls back the current transaction with the given option if provided.
     * 
     * @param options a map of options for the transaction.
     * 
     * @return LB_OK on success, LB_FAIL on failure.
	 */
    LBStatus Rollback(BoltValue&& options = BoltValue::Make_Map())
    {
        CHECK_THREAD();
        ConnectionCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = ConnectionCmdType::Run;

        return pconn ? pconn->Rollback(cmd) : FAIL();
	} // end Rollback


    // -------------------
    // Lifecycle
    // -------------------

    /**
     * @brief terminates the session and ends the connection.
     */
    void Close()
    {
        if (!pconn) return;
        CHECK_THREAD();

		if (!pconn->Is_Open())
			return;

		if (pconn->supported_version.Get_Version() >= 5.1)
		{
			ConnectionCommand cmd({ ConnectionCmdType::Logoff });
			pconn->Logoff(cmd);
		} // end if ver 5.1

		// drain all requests before terminating
		while (!pconn->tasks.Is_Empty())
		{
			pconn->Wait_For_Response();
			pconn->tasks.Dequeue();
		} // end while`

		//epoll_ctl(epfd, EPOLL_CTL_DEL, pconn->Get_Socket(), nullptr);

		pconn->Terminate();
		pconn = nullptr;
	} // end Close


    /**
	 * @brief checks if the session is valid and can be used for operations.
     */
    bool Valid() const { return pconn != nullptr; }

private:

	NeoConnection* pconn;
	NeoPool& pool;
	std::string err_desc;


	// For debug builds, we can track the thread that owns this session to ensure thread safety.
    inline LBStatus FAIL() const
    {
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);
	} // end FAIL


#ifdef LB_DEBUG
    inline void CHECK_THREAD() const
    {
        LB_ASSERT_THREAD(owner_thread);
    }
#else
    inline void CHECK_THREAD() const {}
#endif

};