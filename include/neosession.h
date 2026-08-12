/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 20th of April 2026, Sunday.
 * @date updated 9th of August 2026, Sunday.
 */
#pragma once


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <emmintrin.h>
#include <immintrin.h>
#include <sys/eventfd.h>
#include "neopool.h"
#include "neoerr.h"



#ifdef LB_DEBUG
#define LB_THREAD_ID std::this_thread::get_id()
#define LB_ASSERT_THREAD(expected) \
        if (std::this_thread::get_id() != expected) \
            std::abort()
#else
#define LB_THREAD_ID 0
#define LB_ASSERT_THREAD(x) ((void)0)
#endif



// SIMD shorthands
#if defined(__AVX2__)
#define SIMD_TYPE __m256i
#define LOADU(x) _mm256_loadu_si256(reinterpret_cast<const __m256i*>(x))
#define CMPEQ(a,b) _mm256_cmpeq_epi8(a,b)
#define MOVEMASK(a) _mm256_movemask_epi8(a)
#define SIMD_WIDTH 32
#elif defined(__SSE2__)
#define SIMD_TYPE __m128i
#define LOADU(x) _mm_loadu_si128(reinterpret_cast<const __m128i*>(x))
#define CMPEQ(a,b) _mm_cmpeq_epi8(a,b)
#define MOVEMASK(a) _mm_movemask_epi8(a)
#define SIMD_WIDTH 16
#else
#define SIMD_TYPE char*  // fallback scalar
#endif



//===============================================================================|
//          KONSTANTS
//===============================================================================|
// Write keywords
constexpr std::array<std::string_view, 6> write_keywords = {
	"CREATE", "MERGE", "SET", "DELETE", "DETACH DELETE", "REMOVE"
};



//===============================================================================|
//          FUNCTIONS
//===============================================================================|
/**
 * @brief checks if the query is a write query by looking for the presence of
 *  write keywords in the query string. This is used to determine the routing
 *  strategy for the query when connecting to a cluster.
 *
 * @param query the query string to check
 *
 * @return true if the query is a write query, false otherwise.
 */
QueryMode Detect_Query_Mode_Scalar(const std::string_view query)
{
	bool in_string = false, in_line_comment = false, in_block_comment = false;
	size_t n = query.size();

	for (int i = 0; i < n; i++)
	{
		char c = query[i];

		if (!in_line_comment && !in_block_comment && (c == '\'' || c == '"'))
		{
			in_string = !in_string;
			continue;
		} // end if string toggle

		if (!in_string && !in_block_comment && i + 1 < n && c == '/' && query[i + 1] == '/')
		{
			in_line_comment = true;
			++i;
			continue;
		} // end if line comment

		if (in_line_comment && c == '\n')
		{
			in_line_comment = false;
			continue;
		} // end if newline

		if (!in_string && !in_line_comment && i + 1 < n && c == '/' && query[i + 1] == '*')
		{
			in_block_comment = true;
			++i;
			continue;
		} // end if block comment start

		if (in_block_comment && c == '*' && i + 1 < n && query[i + 1] == '/')
		{
			in_block_comment = false;
			++i;
			continue;
		} // end if block comment end

		if (!in_string && !in_line_comment && !in_block_comment)
		{
			for (auto kw : write_keywords)
			{
				size_t kw_len = kw.size();
				if (i + kw_len > n) continue;
				bool match = true;
				for (size_t j = 0; j < kw_len; ++j)
				{
					if (std::tolower(query[i + j]) != kw[j])
					{
						match = false;
						break;
					} // end if char mismatch
				} // end for kw chars

				if (match) return QueryMode::Write;
			} // end nested if
		} // end if not in string or comment
	} // end for

	return QueryMode::Read;
} // end Is_Write_Query


#if defined(__SSE2__) || defined(__AVX2__)
QueryMode Detect_Query_Mode_SIMD(std::string_view query)
{
	bool in_string = false, in_line_comment = false, in_block_comment = false;
	size_t n = query.size();

	for (size_t i = 0; i < n; ++i)
	{
		char c = query[i];

		if (!in_line_comment && !in_block_comment && (c == '\'' || c == '"'))
		{
			in_string = !in_string;
			continue;
		} // end if comment

		if (!in_string && !in_block_comment && i + 1 < n && c == '/' && query[i + 1] == '/')
		{
			in_line_comment = true; ++i; continue;
		} // end if string

		if (in_line_comment && c == '\n')
		{
			in_line_comment = false;
			continue;
		} // end if line comment

		if (!in_string && !in_line_comment && i + 1 < n && c == '/' && query[i + 1] == '*')
		{
			in_block_comment = true; ++i; continue;
		} // end if

		if (in_block_comment && c == '*' && i + 1 < n && query[i + 1] == '/')
		{
			in_block_comment = false; ++i; continue;
		} // end if

		if (!in_string && !in_line_comment && !in_block_comment)
		{
			for (auto kw : write_keywords)
			{
				size_t kw_len = kw.size();
				if (kw_len > SIMD_WIDTH || i + kw_len > n) continue;

				alignas(SIMD_WIDTH) char buf[SIMD_WIDTH] = {};
				for (size_t j = 0; j < kw_len; ++j) buf[j] = std::tolower(query[i + j]);

				SIMD_TYPE q_vec = LOADU(buf);

				alignas(SIMD_WIDTH) char kw_buf[SIMD_WIDTH] = {};
				for (size_t j = 0; j < kw_len; ++j) kw_buf[j] = kw[j];

				SIMD_TYPE kw_vec = LOADU(kw_buf);
				SIMD_TYPE cmp = CMPEQ(q_vec, kw_vec);
				int mask = MOVEMASK(cmp);

				if ((mask & ((1 << kw_len) - 1)) == ((1 << kw_len) - 1))
					return QueryMode::Write;
			} // end for nested
		} // end if
	} // end for

	return QueryMode::Read;
} // end Detect_Query_Mode_SIMD
#endif


// --------------------
// Unified wrapper
// --------------------
QueryMode Detect_Query_Mode(std::string_view query)
{
#if defined(__AVX2__) || defined(__SSE2__)
	return Detect_Query_Mode_SIMD(query);
#else
	return Detect_Query_Mode_Scalar(query);
#endif
} // end Detect_Query_Mode



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
class NeoDriver;

struct RoutingTable
{
	bool ssl_enabled = false;
	BoltValue* pauth;  
	BoltValue* pextras;
	std::vector<NeoConnection*> leaders;
	std::vector<NeoConnection*> followers;

	RoutingTable() = default;
	RoutingTable(BoltValue& writers, BoltValue& readers)
	{
		for (size_t i = 0; i < writers.list_val.size; ++i)
		{
			std::string url = writers(i).ToString();
			leaders.push_back(new NeoConnection(ssl_enabled, url, pauth, pextras));
		} // end for writers
		for (size_t i = 0; i < readers.size; ++i)
		{
			std::string url = readers(i).ToString();
			followers.push_back(new NeoConnection(ssl_enabled, url, pauth, pextras));
		} // end for readers
	} // end cntr


	void Refresh_Route(BoltValue& writers, BoltValue& readers)
	{
		size_t total = writers.list_val.size + readers.list_val.size;
		size_t current = leaders.size() + followers.size();

		if (total > current)
		{
			for (size_t i = 0; i < writers.list_val.size; ++i)
			{
				std::string url = writers(i).ToString();
				auto it = std::find_if(leaders.begin(), leaders.end(), [&](NeoConnection* pc) {
					return pc->Get_Host_Address() == url;
					});

				if (it != leaders.end())
				{
					leaders.push_back(new NeoConnection(ssl_enabled, url, pauth, pextras));
				}

				// which writers are now followers? move them into the leaders list
				leaders.push_back(std::move(followers.begin(), followers.end(), [&](NeoConnection* pc) {
					return pc->Get_Host_Address() == writers(i).ToString();
					}));
				// delete all leaders that are not in the new list
				leaders.erase(std::remove_if(leaders.begin(), leaders.end(), [&](NeoConnection* pc) {
					return writers(i).ToString() != pc->Get_Host_Address();
					}), leaders.end());
			} // end for writers
			for (size_t i = 0; i < readers.list_val.size; ++i)
			{
				std::string url = readers(i).ToString();
				followers.push_back(new NeoConnection(ssl_enabled, url, pauth, pextras));
			} // end for readers
		} // end if total > current

	} // end Refresh_Route
};


std::atomic<RoutingTable*> g_routing{ nullptr };


//===============================================================================|
//          CLASS
//===============================================================================|
class NeoSession
{
	friend class NeoDriver;
    

public:

	/**
	 * @brief default constructor; initializes the session with no connection and no pool.
	 */
	NeoSession() : pconn(nullptr), pool(*(NeoPool*)nullptr)  { } // end default cntr


	/**
	 * @brief constructor that initializes the session with a connection pool.
	 */
	NeoSession(NeoPool& _pool) : pconn(nullptr), pool(_pool) { } // end pool cntr


	/**
	 * @brief move constructor; transfers ownership of the connection and pool 
	 *	from another session.
	 */
	NeoSession(NeoSession&& other) : pconn(other.pconn), pool(other.pool) 
	{
		other.pconn = nullptr;
	} // end move cntr


	/**
	 * @brief destructor; closes the session and releases the connection back to the pool.
	 */
	~NeoSession() { Close(); }


	/**
	 * @brief move assignment operator; transfers ownership of the connection and 
	 *	pool from another session.
	 */
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
	} // end operator=


	/**
	 * @brief starts a session with neo4j server; i.e. it basically performs handshake
	 *	and/or negotitates version and sends HELLO and LOGON (v5.x+) commands to server.
	 * 
	 * @param epfd descriptor to epoll event listener
	 * @param client_fd descriptor for client socket
	 *
	 * @returns LB_OK on success or LB_FAIL on terminal fail
	 */
	LBStatus Start_Session(int epfd, int client_id, bool is_routed = false)
	{
		if (!pconn)
		{
			pconn = pool.Acquire();
		} // end if no connection

		Add_RCount();
		LBStatus rc = pconn->Connect_Neo4j(epfd, client_id, LB_Handle_Status);
		Wait_For_Response();

		// result ready, check if we got a success or failure
		auto& r = results.Front().value().get();
		if (r.error)
		{
			rc = LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_NEO4J);
			pconn->Terminate();
		} // end if error

		session_epfd = epfd;
		cli_id = client_id;
		return rc;
	} // end Start_Session


    LBStatus Run_Async(
		std::function<void(BoltResult&)> cb,
		const char* query,
        BoltValue&& params = BoltValue::Make_Map(),
        BoltValue&& extras = BoltValue::Make_Map())
    {
        if (!pconn) return FAIL();

		RequestCommand cmd;
		cmd.cypher = query;
		cmd.param = std::move(params);
		cmd.extra = std::move(extras);
		cmd.type = RequestCmdType::Run;
		cmd.mode = cmd.extra.size != 0 ?
			(cmd.extra["mode"].type != BoltType::Unk ?
				(cmd.extra["mode"].ToString() == "r" ? QueryMode::Read : QueryMode::Write) : 
				Detect_Query_Mode(query)) : Detect_Query_Mode(query);
		cmd.cb = std::move(cb);

		if (!requests.Enqueue(std::move(cmd)))
			return FAIL();

		return Execute_Command(requests.Size() - 1);
	} // end Run


	LBStatus Run_Async_Routed(
		std::function<void(BoltResult&)> cb,
		const char* query,
		BoltValue&& params = BoltValue::Make_Map(),
		BoltValue&& extras = BoltValue::Make_Map())
	{
		RequestCommand cmd;
		cmd.cypher = query;
		cmd.param = std::move(params);
		cmd.extra = std::move(extras);
		cmd.type = RequestCmdType::Run;
		cmd.mode = cmd.extra.size != 0 ?
			(cmd.extra["mode"].type != BoltType::Unk ?
				(cmd.extra["mode"].ToString() == "r" ? QueryMode::Read : QueryMode::Write) :
				Detect_Query_Mode(query)) : Detect_Query_Mode(query);
		cmd.cb = std::move(cb);

		LBStatus rc;
		if (cmd.mode != prev_mode)
		{
			if (pconn) pconn = nullptr;		// just blow it away

			pconn = Acquire(cmd.mode);
			if (!pconn) return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);

			rc = pconn->Connect_Neo4j(session_epfd, cli_id, LB_Handle_Status);
			if (!LB_OK(rc)) return rc;

			prev_mode = cmd.mode;
		} // end if mode changed

		rc = pconn->Run(cmd.cypher.c_str(), cmd.param, cmd.extra, cmd.n,
			LB_Handle_Status);

		if (!LB_OK(rc))
			return rc;

		if (!requests.Enqueue(std::move(cmd)))
			return FAIL();

		return LB_Make();
	} // end Run_Async_Routed


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

		auto& r = results.Front()->get();

		result = std::move(r);
		pconn->latencies.Record_Latency
		(
			std::chrono::high_resolution_clock::now() -
			result.start_clock
		);

		if (!results.Is_Empty()) //(result.done)
		{
			results.Dequeue();
			//return LB_Make();
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
		RequestCommand cmd;
		cmd.extra = std::move(options);
		cmd.mode = QueryMode::Read;
		cmd.type = RequestCmdType::Begin;

		if (!requests.Enqueue(std::move(cmd)))
		{
			return FAIL();
		} // end if enqueued

		return pconn ? pconn->Begin(cmd.extra, LB_Handle_Status) : FAIL();
    } // end Begin


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
        RequestCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = RequestCmdType::Commit;

		if (!requests.Enqueue(std::move(cmd)))
		{
			return FAIL();
		} // end if enqueued

        return pconn ? pconn->Commit(cmd.extra, LB_Handle_Status) : FAIL();
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
        RequestCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = RequestCmdType::Run;
		if (!requests.Enqueue(std::move(cmd)))
		{
			return FAIL();
		} // end if enqueued

        return pconn ? pconn->Rollback(cmd.extra, LB_Handle_Status) : FAIL();
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
			pconn->Logoff(LB_Handle_Status);
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


	std::string Get_Last_Error() const
	{
		return pconn ? pconn->Get_Last_Error() : 
			LB_Error_String(LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER, LB_Code(7)));
	} // end Get_Last_Error


private:

	int session_epfd{ -1 };	// epoll descriptor for this session
	int cli_id{ -1 };		// client id for this session
	s64 prev_rcount{ 0 };	// previous request count for this session

	QueryMode prev_mode{ QueryMode::Auto };	// previous query mode for this session
	NeoConnection* pconn;	// used during non-routing processing, i.e. when we have a single connection to the server
	NeoPool& pool;

	std::atomic<s64> rcount{ 0 };			// number of requests sent
	std::atomic<size_t> rr_leaders{ 0 };	// round robin pointer for leader connections
	std::atomic<size_t> rr_followers{ 0 };	// round robin pointer for follower connections

	LockFreeQueue<RequestCommand> requests;		// query requests pending
	LockFreeQueue<BoltResult> results;			// results pending 


	LBStatus Execute_Command(const int index)
	{
		LBStatus rc;
		if (requests.Is_Empty())
		{
			return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER, 
				LBCode::LB_CODE_STATE_QUEUE_SIZE);
		} // end if not cool

		auto& cmd = requests[index].value().get();
		switch (cmd.type)
		{
		case RequestCmdType::Run:
			rc = pconn->Run(cmd.cypher.c_str(), cmd.param, cmd.extra, cmd.n, LB_Handle_Status);
			if (!LB_OK(rc)) return rc;
			break;

		case RequestCmdType::Begin:
			rc = pconn->Begin(cmd.extra, LB_Handle_Status);
			if (!LB_OK(rc)) return rc;
			break;

		case RequestCmdType::Commit:
			rc = pconn->Commit(cmd.extra, LB_Handle_Status);
			if (!LB_OK(rc)) return rc;
			break;

		case RequestCmdType::Rollback:
			rc = pconn->Rollback(cmd.extra, LB_Handle_Status);
			if (!LB_OK(rc)) return rc;
			break;

		case RequestCmdType::Logoff:
			rc = pconn->Logoff(LB_Handle_Status);
			if (!LB_OK(rc)) return rc;
			break;
		} // end cmd.type

		return LB_Make();
	} // end Execute_Command


	// For debug builds, we can track the thread that owns this session to ensure thread safety.
    inline LBStatus FAIL() const
    {
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);
	} // end FAIL


	/**
	 * @brief adds to the rcount, this is used to track the number of pending messages
	 *  and wake waiting threads inorder to simulate a sync fetch style of processing results.
	 */
	inline void NeoSession::Add_RCount()
	{
		rcount.fetch_add(1, std::memory_order_acq_rel);
	} // end Add_Ref


	/**
	 * @brief subtracts from the rcount, this is used to track the number of pending messages
	 *  and wake waiting threads inorder to simulate a sync fetch style of processing results.
	 */
	inline void NeoSession::Sub_RCount()
	{
		prev_rcount = rcount.fetch_sub(1, std::memory_order_acq_rel);
		rcount.notify_one();
	} // end Sub_Ref


	/**
	 * @brief waits for the response of the last sent requestby waiting on the
	 *  rcount atomic variable, which is short for "request count". 
	 *	This function blocks until the response is received.
	 */
	void NeoSession::Wait_For_Response()
	{
		while (1)
		{
			s64 current = rcount.load(std::memory_order_acquire);
			if (current <= prev_rcount)
			{
				prev_rcount = current;
				break;
			} // end if done

			rcount.wait(current);
		} // end while
	} // end Wait_Response


	std::function<void(LBStatus, BoltResult&)> LB_Handle_Status = [&](LBStatus ret, BoltResult& res)
		{
			LBAction action = LB_Action(ret);
			LBDomain domain = LB_Domain(ret);
			LBStatus rc;

			// LB_OK = done
			// LB_HASMORE = not done partial streaming possible
			// LB_RETRY = failed request, retry
			// LB_ROUTE = route error, refresh route table
			// LB_FAIL = terminal oops.

			switch (action)
			{
			case LBAction::LB_OK:
			case LBAction::LB_FAIL:
			{
				// have we requests?
				if (!requests.Is_Empty())
				{
					auto cmd = requests.Dequeue().value();
					if (cmd.cb)
						cmd.cb(res);
					else
					{
						if (!results.Enqueue(std::move(res)))
						{
							res = BoltResult();
						} // end if no enqueue

						pconn->Sub_Sync_Count();
					} // end else sync
				} // end if neo4j domain
				else
				{
					if (!results.Enqueue(std::move(res)))
					{
						res = BoltResult();
					} // end if no enqueue

					pconn->Sub_Sync_Count();
				} // end else sync
			} break;

			case LBAction::LB_RESET:
				rc = pconn->Reset(LB_Handle_Status);
				if (!LB_OK(rc))
					LB_Handle_Status(rc, res);
				break;

			case LBAction::LB_RETRY:
			{
				auto* pres = &requests.Front().value().get();

				if (pres->retry_count++ < MAX_RETRY)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(
						(pres->retry_count - 1) * 500));
#ifdef _DEBUG
					Utils::Print("Retry attempt %d of %d times.",
						pres->retry_count, MAX_RETRY);
#endif
					if (domain == LBDomain::LB_DOM_SYS || domain == LBDomain::LB_DOM_SSL)
					{
						// driver error, maybe connection issue, try to reconnect
						rc = pconn->Connect_Neo4j(pconn->epfd, pconn->client_id, LB_Handle_Status);
						if (!LB_OK(rc))
							LB_Handle_Status(rc, res);
					} // end if domain

					// retry the command
					for (size_t i = 0; i < requests.Size(); ++i)
					{
						rc = Execute_Command(i);
						if (!LB_OK(rc))
						{
							LB_Handle_Status(rc, res);
							break;
						} // end if not ok
					} // end for
				} // end if retry count
				else
				{
					// too many retries, fail the command;
					auto cmd = requests.Dequeue().value();
					if (cmd.cb)
						cmd.cb(res);
					else
					{
						results.Enqueue(std::move(res));
						pconn->Sub_Sync_Count();
					} // end else

					// continue with the next requests
					for (size_t i = 0; i < requests.Size(); ++i)
					{
						rc = Execute_Command(i);
						if (!LB_OK(rc))
						{
							LB_Handle_Status(rc, res);
							break;
						} // end if not ok
					} // end for
				} // end else
			} break;

			case LBAction::LB_SETROUTE:
			{
				BoltValue readers, writers, route;
				size_t count = res.summary.msg(0)["rt"]["servers"].list_val.size;
				for (size_t i = 0; i < count; i++)
				{
					BoltValue server = res.summary.msg(0)["rt"]["servers"];
					std::string role = server(i)["role"].ToString();
					if (!role.compare("ROUTE"))
						route = server(i)["addresses"];
					else if (!role.compare("WRITE"))
						writers = server(i)["addresses"];
					else if (!role.compare("READ"))
						readers = server(i)["addresses"];
				} // end for

				if (!g_routing.load(std::memory_order_acquire))
				{
					g_routing.store(new RoutingTable{ writers, readers }, std::memory_order_release);
				} // end if old table
				else 
				
			} break;

			} // end switch
		}; // end LB_Handle_Status

};