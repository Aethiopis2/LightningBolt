/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday.
 * @date updated 22nd of March 2026, Sunday.
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
//          INCLUDES
//===============================================================================|
#include <emmintrin.h>
#include <immintrin.h>
#include "neocell.h"




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
constexpr static int MAX_CONNECTION_RETRIES = 12;	// default retry count for connection
constexpr static int MAX_REQUEST_RETRIES = 3;		// default retry count for request

enum class QueryMode : u8 {
	Auto,
	Read,
	Write
};


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
//          CLASS
//===============================================================================|
/**
 * @brief constructor
 *
 * @param con_string the connection string to connect to neo4j server
 */
NeoCell::NeoCell(int epfd_, const std::string& urls, BoltValue* pauth, BoltValue* pextras)
	: connection(urls, pauth, pextras), epfd(epfd_), max_connection_retries(MAX_CONNECTION_RETRIES),
	  max_req_retries(MAX_REQUEST_RETRIES), resp_ref(0)
{
	connection_retry_count = 0;
	req_retry_count = 0;
	leftover_bytes = 0;
} // end NeoCell


/**
 * @brief house cleanup via Stop()
 */
NeoCell::~NeoCell()
{
	Stop();
} // end NeoConnection


/**
 * @brief starts a session with the peer by sending a HELLO message based on the version
 *  negotiated. For v5.x it sends a HELLO followed by LOGON message, while for v4.x
 *  it sends a single HELLO message. On successful authentication the peer responds with
 *  SUCCESS message and we are good to go. On LB_Retry the function attempts reconnection
 *	predefined number of times before giving up.
 *
 * @return LB_OK on success. LB_FAIL on terminal fail.
 */
LBStatus NeoCell::Start_Session(const int id)
{
	// trivially reject false calls
	if (connection.is_open) return LB_Make();

	// push result queue
	if (!requests.Enqueue({}))
	{
		return LB_Handle_Status(LB_Make
		(
			LBAction::LB_FAIL,
			LBDomain::LB_DOM_DRIVER,
			LBCode::LB_CODE_TASKSTATE
		), this);
	} // end if queue not avail

	LBStatus rc = connection.Handshake(epfd, this, id);
	if (!LB_OK(rc))
	{
		requests.Dequeue();
		return LB_Handle_Status(rc, this);
	} // end if no handshake

	Add_Ref();
	TaskState state = connection.tasks.Front()->get().state;
	if (connection.supported_version.Get_Version() >= 5.1)       // use version 6/5 hello
		rc = connection.Send_Hellov5();
	else  // version 4 and below 
		rc = connection.Send_Hellov4();

	if (!LB_OK(rc))
	{
		requests.Dequeue();
		return LB_Handle_Status(rc, this);
	} // end if no hello

	if (Should_Wait())
	{
		Wait_Response();

		// check decoded value
		auto res = requests.Dequeue();
		if (res.has_value())
		{
			if (res->result.error)
			{
				err_desc = res->result.begin().bv.ToString();
				rc = LB_Make
				(
					LBAction::LB_FAIL,
					LBDomain::LB_DOM_NEO4J
				);
			} // end if failed

			latencies.Record_Latency
			(
				std::chrono::high_resolution_clock::now() -
				res->result.start_clock
			);
		} // end if
	} // end if should wait

	return rc;
} // end Start_Session


LBStatus NeoCell::Run_Async(std::function<void(BoltResult&)> cb, 
	const char* query, BoltValue&& param, BoltValue&& extra)
{
	// queue the request as a command structure before running it;
	//	that allows for retries incase of failures.
	CellCommand cmd;
	cmd.type = CellCmdType::Run;
	cmd.cypher = query;
	cmd.param = std::move(param);
	cmd.extra = std::move(extra);
	cmd.cb = cb;

	LBStatus rc = Execute_Command(cmd);
	if (!LB_OK(rc))
		rc = LB_Handle_Status(rc, this);

	return LB_Make();
} // end run


LBStatus NeoCell::Run(const char* query, BoltValue&& param, BoltValue&& extra)
{
	return Run_Async(nullptr, query, std::move(param), std::move(extra));
} // end run



LBStatus NeoCell::Fetch(BoltResult& results)
{
	do
	{
		// wait for at least one full message
		Wait_Response();

		auto& result = requests.Front()->get().result;

		results = std::move(result);
		if (results.done)
			requests.Dequeue();

		latencies.Record_Latency
		(
			std::chrono::high_resolution_clock::now() -
			result.start_clock
		);
	} while (!requests.Is_Empty());

	return LB_Make();
} // end Fetch


/**
 * @brief returns the underlying socket descriptorconnection.read_buf.Reset();
 */
int NeoCell::Get_Socket() const
{
	return connection.Get_Socket();
} // end Get_Socket


/**
 * @brief returns the number of connection attempts made so far
 */
int NeoCell::Get_Connection_Retry_Count() const
{
	return connection_retry_count;
} // end Get_Try_Count


/**
 * @brief returns the maximum retry count allowed currently
 */
int NeoCell::Get_Max_Connection_Retry_Count() const
{
	return max_connection_retries;
} // end Get_Max_Try_Count


/**
 * @brief returns the number of retries attempted for a request thus far
 */
int NeoCell::Get_Request_Retry_Count() const
{
	return req_retry_count;
} // end Get_Request_Retry_Count


/**
 * @brief returns the maximum retry count for requests set currently
 */
int NeoCell::Get_Max_Request_Retry_Count() const
{
	return max_req_retries;
} // end Get_Max_Request_Retry_Count


/**
 * @brief returns the optional client id set from driver
 */
int NeoCell::Get_ClientID() const
{
	return connection.client_id;
} // end Get_Cli_ID


/**
 * @brief returns the p-th percentile latency in milliseconds, aproximately. 
 *	The latency is calculated based on the time it takes for the connection to 
 *	encode + send + receive + decode a full response for a request. 
 *
 * @param p the percentile to compute in [0.0, 1.0]
 * 
 * @return the p-th percentile latency in milliseconds
 */
u64 NeoCell::Percentile(double p) const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		latencies.Percentile(p)).count();
} // end Percentile


/**
 * @brief returns the average latency in milliseconds, aproximately. The latency 
 *	is calculated based on the time it takes for the connection to encode + 
 *	send + receive + decode a full response for a request.
 *
 * @return the average latency in milliseconds
 */
u64 NeoCell::Avg_Latency() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		latencies.Avg_Latency()).count();
} // end Avg_Latency


/**
 * @brief indicates if the underlying connection is still active
 */
bool NeoCell::Is_Connected() const
{
	return connection.Is_Open();
} // end Is_Connected


/**
 * @brief returns a human readable string version of last error encountered
 *	usually from Neo4j bolt side.
 */
std::string NeoCell::Get_Last_Error() const
{
	return err_desc;
} // end Get_Last_Error


/**
 * @brief should the incremented memeber connection_retry_count exceed
 *	its max set value the function returns false, and reset's the 
 *	member to 0.
 */
bool NeoCell::Can_Retry_Connect()
{
	return Can_Retry(connection_retry_count, max_connection_retries);
} // end Can_Retry_Connect


/**
 * @brief returns false should the request count exceed its maximum.
 *	Function makes use of Can_Retry() memeber
 */
bool NeoCell::Can_Retry_Request()
{
	return Can_Retry(req_retry_count, max_req_retries);
} // end Can_Retry_Connect


/**
 * @brief returns false if startup shouldn't wait for response, it
 *	happens during session starting on auto retries and prevents
 *	the function from waiting on results.
 */
bool NeoCell::Should_Wait() const
{
	return connection.should_wait.load(std::memory_order_acquire);
} // end Should_Wait


/**
 * @brief sets the maximum retry count to the number, n greater than 0
 */
void NeoCell::Set_Max_Connection_Retry_Count(const int n)
{
	if (n > 0 && max_connection_retries != n)
		max_connection_retries = n;
} // end Set_Retry_Count


/**
 * @brief reset's the retry count to 0 to begin afresh.
 */
void NeoCell::Reset_Connection_Retry()
{
	connection_retry_count = 0;
} // end Set_Max_Connection_Retry_Count


/**
 * @brief sets the maximum retry count to the number, n greater than 0
 */
void NeoCell::Set_Max_Request_Retry_Count(const int n)
{
	if (n > 0 && max_req_retries != n)
		max_req_retries = n;
} // end Set_Max_Request_Retry_Count


/**
 * @brief reset's the retry count to 0 to begin afresh.
 */
void NeoCell::Reset_Request_Retry()
{
	req_retry_count = 0;
} // end Reset_Request_Retry


void NeoCell::Wait_Response()
{
	while (1)
	{
		s64 current = resp_ref.load(std::memory_order_acquire);
		if (current <= 0) break;

		resp_ref.wait(current);
	} // end while
} // end Wait_Response


void NeoCell::Add_Ref()
{
	resp_ref.fetch_add(1, std::memory_order_acq_rel);
} // end Add_Ref


void NeoCell::Sub_Ref()
{
	u64 prev = resp_ref.fetch_sub(1, std::memory_order_acq_rel);
	resp_ref.notify_one();
} // end Sub_Ref


/**
 * @brief set's the wait on/off controller flag from the parameter
 *  passed.
 *
 * @param wait_ a boolean value to set should wait explicilty
 */
void NeoCell::Set_Wait(const bool wait_)
{
	connection.should_wait.store(wait_, std::memory_order_release);
} // end Set_Wait


/**
 * @brief terminates the active connection does house cleaning.
 */
void NeoCell::Stop()
{
	if (!connection.Is_Open())
		return;

	if (connection.supported_version.Get_Version() >= 5.1)
	{
		CellCommand cmd({ CellCmdType::Logoff });
		Execute_Command(cmd);
	} // end if ver 5.1

	// drain all requrests before terminating
	while (!requests.Is_Empty())
	{
		Wait_Response();
		requests.Dequeue();
	} // end while 

	epoll_ctl(epfd, EPOLL_CTL_DEL, Get_Socket(), nullptr);
	connection.Terminate();
} // end Stop


/**
 * @brief clear's the histogram of latencies
 */
void NeoCell::Clear_Histo()
{
	latencies.Clear();
} // end Clear_Histo


/**
 * @brief consumes the read buffer by the number of bytes specified. This is
 *  usually called after a complete message is decoded and consumed from the
 *  buffer.
 *
 * @param bytes the number of bytes to consume from the read buffer
 */
void NeoCell::Consume_Read_Buffer(const size_t bytes)
{
	connection.read_buf.Consume(bytes);
} // end Consume_Read_Buffer


/**
 * @brief resets the read buffer to be empty and ready for the next batch of
 *  messages. This is usually called after a complete message is decoded and
 *  consumed from the buffer.
 */
void NeoCell::Reset_Read_Buffer()
{
	connection.read_buf.Reset();
} // end Reset_Read_Buffer


/**
 * @brief returns a pointer to the current read position in the read buffer.
 *  This is usually called by the decoder loop when it is ready to decode
 *  messages from the buffer.
 *
 * @return a pointer to the current read position in the read buffer
 */
u8* NeoCell::Get_Read_Buffer_Read_Ptr()
{
	return connection.read_buf.Read_Ptr();
} // end Get_Read_Buffer_Read_Ptr


/**
 * @brief returns true should the _count remain less than max_count. 
 *	when incremental _count exceeds max, it resets _count to 0 and
 *	returns a false to caller to indicate done.
 * 
 * @param _count reference to incremental count value
 * @param max_count the maximum allowed count before false
 * 
 * @return true if _count remains less than max.
 */
inline bool NeoCell::Can_Retry(int& _count, const int max_count)
{
	if (++_count > max_count)
	{
		_count = 0;		// reset it
		return false;
	} // end if not anymore

	return true;	// yes you can
} // end Can_Retry



/**
 * @brief invokes connection's Poll_Readable() and returns the result
 *	as is.
 *
 * @return LB_OK on success, LB_FAIL on terminal failure, or LB_WAIT
 *	if poll would block on waiting.
 */
LBStatus NeoCell::Poll_Read()
{
	LBStatus rc = connection.Poll_Readable();
	if (!LB_OK(rc))
	{
		Set_Wait();
		rc = LB_Handle_Status(rc, this);
		Set_Wait(true);	// restore
	} // end if

	return rc;
} // end Poll_Read


/**
 * @brief the encoder loop encodes or calls the connection functions based on
 *	the command type info given by CellCommand structure. Once all the rquest_queue
 *	items are done, or flushed to peer, the thread is put to sleep until woken by
 *	Add_QCount() method or similar.
 */
LBStatus NeoCell::Execute_Command(CellCommand& cmd)
{
	LBStatus rc = 0;
	if (!requests.Enqueue(std::move(cmd)))
	{
		return LB_Make
		(
			LBAction::LB_FAIL,
			LBDomain::LB_DOM_DRIVER,
			LBCode::LB_CODE_STATE_QUEUE_MEM
		);
	} // end if fail enqueue

	Add_Ref();
	switch (cmd.type)
	{
	case CellCmdType::Run:
		rc = connection.Run(cmd.cypher, cmd.param, cmd.extra, cmd.n);
		break;

	case CellCmdType::Begin:
		rc = connection.Begin(cmd.param);
		break;

	case CellCmdType::Commit:
		rc = connection.Commit(cmd.param);
		break;

	case CellCmdType::Rollback:
		rc = connection.Rollback(cmd.param);
		break;

	case CellCmdType::Logoff:
		rc = connection.Logoff();
		break;

	case CellCmdType::Reset:
		rc = connection.Reset();
		break;

	default:
		//connection.err_string = "Encoder loop, command violation. Aborted thread.";
		return LB_Make(LBAction::LB_FAIL);
	} // end switch

	if (!LB_OK(rc))
	{
		Sub_Ref();
		requests.Dequeue();
	} // end if 

	return rc;
} // end Write_Loop


/**
 * @breif marks buffer position for decoding starting from ptr. It decodes everything
 *	it can between the start and its size in bytes. If data is trimmed or cut to the 
 *	end it marks the position and recv more. It also notifies callers as soon as
 *	the current recv buffer frame is ready via atomic::notify_all or callbacks to reduce
 *	waiting latency on the user side.
 * 
 * @param ptr the starting position in the current buffer frame
 * @param bytes the number of bytes for the view/frame of buffer
 * 
 * @return LOB_OK on success, alas LB_FAIL/RETRY on failure.
 */
LBStatus NeoCell::Decode_Response(u8* ptr, const size_t bytes)
{
	size_t decoded = 0; // tacks decoded bytes thus far
	LBStatus rc = 0;    // holds return values

#ifdef _DEBUG
	Utils::Print("Decoding response, bytes received: %zu", bytes);
	Utils::Dump_Hex((const char*)ptr, bytes);

	std::cout << "Write_Offset: " << connection.read_buf.Get_Write_Offset()
		<< "\nRead_Offset: " << connection.read_buf.Get_Read_Offset() << std::endl;

#endif

	int total_decode = bytes + leftover_bytes;
	leftover_bytes = 0;		// reset for next round

	while (decoded < total_decode)
	{
		auto task = connection.tasks.Front();
		if (!task.has_value())
		{
			connection.read_buf.Consume(decoded);
			return LBOK_INFO(decoded);		// treat as done.
		} // end no more tasks

		task->get().view.cursor = ptr;			// set the cursor to the start of the buffer
		task->get().view.size = total_decode;	// set the size to the number of bytes received

		rc = connection.Can_Decode(ptr, total_decode - decoded);
		if (!LB_OK(rc))
		{
			if (LBAction(LB_Action(rc)) == LBAction::LB_HASMORE)
			{
				leftover_bytes = total_decode - decoded;	// set the left over bytes for the next batch
				connection.read_buf.Consume(decoded);
				return LB_Make
				(
					LBAction::LB_HASMORE,
					LBDomain::LB_DOM_NEO4J,
					LBCode::LB_CODE_NONE,
					decoded
				);
			} // end if appended info

			return rc;
		} // end if cannot decode

		// because we can decode it
		u32 aux = LB_Aux(rc);
		decoded += aux;			// get the number of bytes decoded and add it to the total
		ptr += aux;				// move the cursor forward by the number of bytes decoded

		// now do decode it
		rc = connection.Decode_One(requests.Front()->get().result);
		// LB_OK = done
		// LB_HASMORE = not done partial streaming possible
		// LB_RETRY = failed request, retry
		// LB_ROUTE = route error, refresh route table
		// LB_FAIL = terminal oops.
		// All domains are from Neo4j


		LBAction action = LB_Action(rc);
		if (action == LBAction::LB_OK || action == LBAction::LB_FAIL)
		{
			auto& req = requests.Front()->get();
			if (req.cb)
			{
				req.result.client_id = Get_ClientID();
				req.cb(req.result);
				if (req.result.done) requests.Dequeue();
			} // end if viz callback
			
			Sub_Ref();
		} // end if ok
		else if (action == LBAction::LB_RETRY)
		{
			connection.read_buf.Consume(decoded);
			connection.read_buf.Adaptive_Tick(decoded);  // EMA based growth/shrink
			return LB_Handle_Status(rc, this);
		} // end else if retry
	} // end while

	// update the buffer pos, stats and all with what's actually decoded
	connection.read_buf.Consume(decoded);
	connection.read_buf.Adaptive_Tick(decoded);  // EMA based growth/shrink
	return rc; // LBOK_INFO(decoded);
} // end Decode_Response
