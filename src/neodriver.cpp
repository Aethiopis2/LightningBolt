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
#include <emmintrin.h>
#include <immintrin.h>
#include <sys/eventfd.h>
#include "neodriver.h"




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


/**
 * @brief associates a given thread to a specific core
 */
void PinThreadToCore(int core_id)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);

	pthread_t current_thread = pthread_self();
	pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
} // end PinThreadToCore


//===============================================================================|
//          DEFINITON
//===============================================================================|
/**
 * @brief constructor
 */
NeoDriver::NeoDriver(std::string urls, BoltValue auth, BoltValue extras, int cores)
	: _auth(std::move(auth))
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

	// check url info and set ssl if needed
	size_t pos = urls.find_first_of(URL_SEPARATOR);
	if (pos != std::string::npos)
	{
		if (!urls.substr(0, pos).compare(BOLT_NORMAL))
		{
			ssl_on = false;
			clustred = false;
		} // end else if no ssl on single
		else if (!urls.substr(0, pos).compare(BOLT_SSL))
		{
			ssl_on = true;
			clustred = false;
		} // end if ssl on single
		else if (!urls.substr(0, pos).compare(NEO4J_NORMAL))
		{
			ssl_on = false;
			clustred = true;
		} // end if ssl on single
		else if (!urls.substr(0, pos).compare(NEO4J_SSL))
		{
			ssl_on = false;
			clustred = true;
		} // end else if ssl on cluster

		_urls = urls.substr(pos + 3, urls.length() - (pos + 3));
	} // end if pos

	// create contexts and their corresponding pools and routers
	_cores.reserve(cores);
	for (int i = 0; i < cores; i++)
	{
		int efd = epoll_create1(0);
		NeoPool pool(efd, ssl_on, clustred, i, POOL_SIZE, _urls, &_auth, &_extras);

		CoreContext ctx
		(
			i, 
			efd, 
			eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC),
			std::move(pool)
		);

		_cores.push_back(std::move(ctx));
		auto& c = _cores[i];
		c.poll_thread = std::thread([this, &c]() {
			PinThreadToCore(c.core_id);
			Poll_Loop(c);
		});
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
	BoltValue&& params, 
	BoltValue&& extra)
{
	/*ConnectionCommand cmd;
	cmd.type = ConnectionCmdType::Run;
	cmd.cypher = query;
	cmd.param = std::move(params);
	cmd.extra = std::move(extra);
	cmd.cb = cb;
	cmd.mode = Detect_Query_Mode(query);*/

	// core-local dispatch (no contention)
	CoreContext& ctx = _cores[Next_Core()];
	LBStatus rc = ctx.session.Start_Session(ctx.epfd, ++next_client_id);
	if (!LB_OK(rc))
	{
		//last_err = pcell->Get_Last_Error();
		return rc;
	} // end if failed to start session

	return ctx.session.Run_Async(cb, query, std::move(params), std::move(extra));
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


/**
 * @brief acquires a session from the pool and returns it to the caller. The session
 *	handle is used for subsequent operations like Run, Fetch, Begin, Commit, Rollback etc.
 * 
 * @param handle the session handle to populate and return to the caller.
 * 
 * @return LB_OK on success, LB_FAIL on failure.
 */
LBStatus NeoDriver::Get_Session(NeoSession& handle)
{
	CoreContext& ctx = _cores[Next_Core()];
	NeoConnection* pcon = ctx.pool.Acquire();
	if (!pcon)
	{
		return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);
	} // end if no cell

	handle.pconn = pcon;
	//handle.pdriver = this;

	LBStatus rc = handle.Start_Session(ctx.epfd, ++next_client_id);
	if (!LB_OK(rc))
	{
		//last_err = pcell->Get_Last_Error();
		return rc;
	} // end if failed to start session

	return LB_Make();
} // end Get_Session



void NeoDriver::Close()
{
	u64 my_exit = 1;

	for (auto& ctx : _cores)
	{
		ctx.pool.Close();
		write(ctx.exit_fd, &my_exit, sizeof(my_exit));
		if (ctx.poll_thread.joinable()) ctx.poll_thread.join();

		CLOSE(ctx.epfd);
		CLOSE(ctx.exit_fd);
	} // end for
} // end Close


inline size_t NeoDriver::Next_Core()
{
	return rr_core.fetch_add(1, std::memory_order_relaxed) % _cores.size();
} // end Next_Core


void NeoDriver::Poll_Loop(CoreContext& ctx)
{
	epoll_event events[MAX_EVENTS];

	while (ctx.running.load(std::memory_order_acquire))
	{
		int nfds = epoll_wait(ctx.epfd, events, MAX_EVENTS, 1000); // 1 second timeout
		for (int n = 0; n < nfds; ++n)
		{
			NeoConnection* pconn = static_cast<NeoConnection*>(events[n].data.ptr);
			if (events[n].events & EPOLLIN)
			{
				if (events[n].data.fd == ctx.exit_fd)
				{
					uint64_t val;
					read(ctx.exit_fd, &val, sizeof(val)); // clear it
					ctx.running.store(false, std::memory_order_relaxed);
					break;
				}

				// ready to read, read it in and push task to decoder
				LBStatus rc = LB_Make(LBAction::LB_HASMORE);
				while (LB_Action(rc) == LBAction::LB_HASMORE)
				{
					rc = pconn->Poll_Readable();
					if (!LB_OK(rc))
					{
						if (LBAction(LB_Action(rc)) == LBAction::LB_FAIL)
						{
							pconn->Terminate();	// kill it
							break;
						} // end if fail
						else break; // should be wait only
					} // end if error

					// now begin decoding, if we have a full message
					rc = pconn->Decode_Response(pconn->read_buf.Read_Ptr(), LB_Aux(rc));
					if (pconn->results.Is_Empty() && pconn->tasks.Is_Empty())
						pconn->read_buf.Reset();
				} // end while
			} // end if readable
		} // end for nfds
	} // end while looping
} // end Poll_Read