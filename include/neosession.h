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
	LBStatus Start_Session(int epfd, int client_id)
	{
		if (!pconn)
		{
			pconn = pool.Acquire();
		} // end if no connection

		LBStatus rc = pconn->Connect_Neo4j(epfd, client_id,
			[&](LBStatus ret, BoltResult& res) {
				if (res.error)
				{
					if (!results.Enqueue(std::move(res)))
					{
						return LB_Make(
							LBAction::LB_FAIL,
							LBDomain::LB_DOM_DRIVER,
							LBCode::LB_CODE_STATE_QUEUE_MEM
						);
					} // end if no enqueue

					rc = LB_Make
					(
						LBAction::LB_FAIL,
						LBDomain::LB_DOM_NEO4J
					);
				} // end if error
				else rc = LB_Make();
			});

		pconn->Wait_For_Response();
		if (!LB_OK(rc))
		{
			LBAction action = LB_Action(rc);
			LBDomain domain = LB_Domain(rc);
			LBCode code = LB_Code(rc);

			while (action == LBAction::LB_RETRY && ++retry_count < MAX_RETRY)
			{
#ifdef _DEBUG
				Utils::Print("connection #%d failed. Retry attempt %d of %d times.",
					client_id, retry_count, MAX_RETRY);
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(
					(retry_count - 1) * 500));

				rc = pconn->Connect_Neo4j(epfd, client_id,
					[&](LBStatus ret, BoltResult& res) {
						if (res.error)
						{
							if (!results.Enqueue(std::move(res)))
							{
								return LB_Make(
									LBAction::LB_FAIL,
									LBDomain::LB_DOM_DRIVER,
									LBCode::LB_CODE_STATE_QUEUE_MEM
								);
							} // end if no enqueue

							rc = LB_Make
							(
								LBAction::LB_FAIL,
								LBDomain::LB_DOM_NEO4J
							);
						} // end if error
						else rc = LB_Make();
					});

				action = LB_Action(rc);
			} // end while

			// at the end of the day..
			retry_count = 0;
		} // end if failed to connect

		return rc;
	} // end Start_Session


    LBStatus Run_Async(
		std::function<void(BoltResult&)> cb,
		const char* query,
        BoltValue&& params = BoltValue::Make_Map(),
        BoltValue&& extras = BoltValue::Make_Map())
    {
        CHECK_THREAD();
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
		{
			return FAIL();
		} // end if enqueued

		return pconn->Run(query, cmd.param, cmd.extra, cmd.n,
			LB_Handle_Status);
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

	NeoConnection* pconn;
	NeoPool& pool;

	int retry_count{ 0 };		// how many times we've retried a failed attempt

	LockFreeQueue<RequestCommand> requests;		// query requests pending
	LockFreeQueue<BoltResult> results;			// results pending 

	LBStatus Execute_Command()
	{
		LBStatus rc;
		for (size_t i = 0; i < requests.Size(); i++)
		{
			auto& cmd = requests[i].value().get();
			switch (cmd.type)
			{
			case RequestCmdType::Run:
				rc = pconn->Run(cmd.cypher, cmd.param, cmd.extra, cmd.n, LB_Handle_Status);
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
		} // end for 

		return LB_Make();
	} // end Execute_Command


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


	std::function<void(LBStatus, BoltResult&)> LB_Handle_Status = [&](LBStatus ret, BoltResult& res)
		{
			LBAction action = LB_Action(ret);

			// LB_OK = done
			// LB_HASMORE = not done partial streaming possible
			// LB_RETRY = failed request, retry
			// LB_ROUTE = route error, refresh route table
			// LB_FAIL = terminal oops.
			// All domains are from Neo4j except for enqueue errors which are from driver domain.

			switch (action)
			{
			case LBAction::LB_OK:
			case LBAction::LB_FAIL:
			{
				auto cmd = requests.Dequeue().value();
				if (cmd.cb)
					cmd.cb(res);
				else
				{
					results.Enqueue(std::move(res));
					pconn->Sub_Sync_Count();
				}
			} break;

			} // end switch
		}; // end LB_Handle_Status

};