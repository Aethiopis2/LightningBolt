/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date @date updated 27th of Feburary 2026, Friday
 */


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neocell.h"




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief constructor
 *
 * @param con_string the connection string to connect to neo4j server
 */
NeoCell::NeoCell(int epfd_, const std::string& urls, BoltValue* pauth, BoltValue* pextras)
	: connection(urls, pauth, pextras), epfd(epfd_), max_retries(12)
{
	retry_count = 0;
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
	LBStatus rc = Handshake(id);
	if (!LB_OK(rc)) return LB_Handle_Status(rc, this);

	// push a state into the queue
	TaskState state = TaskState::Hello;
	if (!connection.tasks.Enqueue(state))
		return LB_Make(
			LBAction::LB_FAIL, 
			LBDomain::LB_DOM_STATE,
			LBStage::LB_STAGE_SESSION, 
			LBCode::LB_CODE_STATE_QUEUE_MEM
		);

	if (connection.supported_version.Get_Version() >= 5.1)       // use version 6/5 hello
		rc = connection.Send_Hellov5(state);
	else  // version 4 and below 
		rc = connection.Send_Hellov4(state);

	if (!LB_OK(rc)) return LB_Handle_Status(rc, this);
	connection.Wait_Task();

	// check decoded value
	auto t = connection.results.Dequeue();
	if (t.has_value())
	{
		if (t->error)
		{
			err_desc = t->begin().bv.ToString();
			rc = LB_Make(
				LBAction::LB_FAIL,
				LBDomain::LB_DOM_NEO4J,
				LBStage::LB_STAGE_SESSION
			);
		} // end if failed
	} // end if

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
		connection.Wait_Task();

		requests.Dequeue();		// remove the request on response to user, its done!
		auto result = connection.results.Dequeue();
		if (!result.has_value())
			return LB_Make();

		results = std::move(result.value());
	} while (!connection.results.Is_Empty());

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
int NeoCell::Get_Retry_Count() const
{
	return retry_count;
} // end Get_Try_Count


/**
 * @brief returns the maximum retry count allowed
 */
int NeoCell::Get_Max_Retry_Count() const
{
	return max_retries;
} // end Get_Max_Try_Count


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
		connection.latencies.Percentile(p)).count();
} // end Percentile


/**
 * @brief returns the average latency in milliseconds, aproximately. The latency 
 *	is calculated based on the time it takes for the connection to encode + 
 *	send + receive + decode a full response for a request.
 *
 * @return the average latency in milliseconds
 */
u64 NeoCell::Wall_Latency() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		connection.latencies.Avg_Latency()).count();
}

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
 * @brief increments the try count for connection attempts
 */
bool NeoCell::Can_Retry()
{
	if (++retry_count > max_retries)
	{
		retry_count = 0;		// reset it
		return false;
	} // end if not anymore

	return true;	// yes you can
} // end Increment_Try_Count


/**
 * @brief sets the maximum retry count to the number, n greater than 0
 */
void NeoCell::Set_Max_Retry_Count(const int n)
{
	if (n > 0 && max_retries != n)
		max_retries = n;
} // end Set_Retry_Count


/**
 * @brief reset's the retry count to 0 to begin afresh.
 */
void NeoCell::Reset_Retry()
{
	retry_count = 0;
} // end Reset_Retry


/**
 * @brief terminates the active connection does house cleaning.
 */
void NeoCell::Stop()
{
	DecoderTask task(TaskState::Logoff);
	if (!connection.Is_Open())
		return;

	if (connection.supported_version.Get_Version() >= 5.1)
	{
		CellCommand cmd({ CellCmdType::Logoff });
		Execute_Command(cmd);
	} // end if ver 5.1

	// drain all requrests before terminating
	do {
		connection.Wait_Task();
		connection.tasks.Dequeue();
	} while (!connection.tasks.Is_Empty());

	epoll_ctl(epfd, EPOLL_CTL_DEL, Get_Socket(), nullptr);
	connection.Terminate();
} // end Stop



/**
 * @brief clear's the histogram of latencies
 */
void NeoCell::Clear_Histo()
{
	connection.latencies.Clear();
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
 * @brief starts a TCP connection with Neo4j server either TLS enabled or not. On
 *	successful connection, it negotiates the bolt protocol version with the peer.
 *	It then registers it to epoll for it to actively poll read events. It finally
 *	sets the client id from the parameter passed via Set_ClientID() memeber.
 *
 * @param id application defined connection identifer
 *
 * @return LB_OK on success. LB_RETRY on ssl/sys error. LB_FAIL on epoll register
 *	error.
 */
LBStatus NeoCell::Handshake(const int id)
{
	connection.Set_ClientID(id);
	LBStatus rc = connection.Connect();
	if (!LB_OK(rc)) return LB_Add_Stage(rc, LBStage::LB_STAGE_HANDSHAKE);

	rc = connection.Negotiate_Version();
	if (!LB_OK(rc)) return LB_Add_Stage(rc, LBStage::LB_STAGE_HANDSHAKE);

	// set non-blocking and enable keepalive
	connection.Enable_NonBlock();
	connection.Enable_Keepalive();

	// register to epoll
	epoll_event ev{};
	ev.events = EPOLLIN | EPOLLET;
	ev.data.ptr = this;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, Get_Socket(), &ev) < 0)
	{
		return LB_Make(
			LBAction::LB_FAIL, 
			LBDomain::LB_DOM_SYS,
			LBStage::LB_STAGE_HANDSHAKE, 
			LBCode::LB_CODE_NONE, 
			errno
		);
	} // end if error adding to epoll

	connection.Set_ClientID(id);
	return rc;	// should be 0k
} // end Connect_Tcp



/**
 * @brief invokes connection's Poll_Readable() and returns the result
 *	as is.
 *
 * @return LB_OK on success, LB_RETRY/LB_FAIL on failure.
 */
LBStatus NeoCell::Poll_Read()
{
	return connection.Poll_Readable();
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

	default:
		//connection.err_string = "Encoder loop, command violation. Aborted thread.";
		return LB_Make(LBAction::LB_FAIL);
	} // end switch

	if (LB_OK(rc)) requests.Enqueue(std::move(cmd));
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
#endif

	int total_decode = bytes + leftover_bytes;
	leftover_bytes = 0;		// reset for next round

	while (decoded < total_decode)
	{
		auto task = connection.tasks.Front();
		if (!task.has_value())
			return LBOK_INFO(total_decode);		// treat as done.

		task->get().view.cursor = ptr;			// set the cursor to the start of the buffer
		task->get().view.size = total_decode;	// set the size to the number of bytes received

		rc = connection.Can_Decode(ptr, total_decode - decoded);
		if (!LB_OK(rc))
		{
			if (LBAction(LB_Action(rc)) == LBAction::LB_HASMORE)
			{
				leftover_bytes = total_decode - decoded;	// set the left over bytes for the next batch
				return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
					LBStage::LB_STAGE_DECODEING_TASK, LBCode::LB_CODE_NONE, decoded);
			} // end if appended info

			return rc;
		} // end if cannot decode

		rc = connection.Decode_One(task->get());
		u32 aux = LB_Aux(rc);
		decoded += aux;			// get the number of bytes decoded and add it to the total
		ptr += aux;				// move the cursor forward by the number of bytes decoded
	} // end while

	// update the buffer stats with what's actually decoded
	connection.read_buf.Adaptive_Tick(decoded);  // EMA based growth/shrink
	return LBOK_INFO(decoded);
} // end Decode_Response