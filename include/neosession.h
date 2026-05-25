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
	}

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

		if (!pconn->commands.Enqueue(std::move(cmd)))
		{
			return FAIL();
		} // end if enqueued

		return pconn->Run(pconn->commands.Front()->get());
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
		RequestCommand cmd;
		cmd.extra = std::move(options);
		cmd.mode = QueryMode::Read;
		cmd.type = RequestCmdType::Begin;
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
        RequestCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = RequestCmdType::Commit;

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
        RequestCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = RequestCmdType::Run;

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
			RequestCommand cmd({ RequestCmdType::Logoff });
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