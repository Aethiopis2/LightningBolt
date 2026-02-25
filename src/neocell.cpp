/**
 * @brief implementation detials for NeoQE, the Query Engine
 *
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @date created 10th of December 2025, Wednesday
 * @date updated 15th of Feburary 2026, Sunday
 *
 * @copyright Copyright (c) 2025
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
NeoCell::NeoCell(const std::string& urls, BoltValue* pauth, BoltValue* pextras)
	: connection(urls, pauth, pextras), running(false),
	  esleep(0), dsleep(0), try_count(0), max_tries(5)
{
} // end NeoCell


/**
 * @brief house cleanup via Stop()
 */
NeoCell::~NeoCell()
{
	Stop();
} // end NeoConnection


/**
 * @brief starts a TCP connection with Neo4j server either TLS enabled or not. It then begins
 *  version negotiation using v5.7+ manifest if supported or old skool 20 byte payload data
 *  if not. On Successful negotiation the function sends a HELLO message for < v5.0 and
 *  followed by LOGON message for v5.0+. During handshake, should peer closes the connection
 *  the function attempts reconnect a couple of more times before giving up.
 *
 * @param id application defined connection identifer
 *
 * @return 0 on success. -1 on sys error, -2 app speific error s.a version not supported
 */
LBStatus NeoCell::Start(const int id)
{
	// clear previous errors & add new ones
	tasks.Clear();
	tasks.Enqueue({ QueryState::Connection });

	LBStatus rc = connection.Init(id);
	if (!LB_OK(rc))
	{
		//LB_Handle_Status(rc, this);
		return rc;
	} // end if

	EWake();
	Set_Running(true);
	encoder_thread = std::thread(&NeoCell::Encoder_Loop, this);
	decoder_thread = std::thread(&NeoCell::Decoder_Loop, this);

	// wait for result
	connection.Wait_Task();

	// check decoded value
	auto t = rqueue.Dequeue();
	if (t.has_value())
	{
		if (t->Is_Error())
		{
			last_error = t->err.ToString();
			return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_NEO4J);
		} // end if failed
	} // end if

	return rc;	// should be ready
} // end Start


int NeoCell::Enqueue_Request(CellCommand&& cmd)
{
	if (!equeue.Enqueue(std::move(cmd)))
		return -3;

	EWake();	// also wakes decoder too
	return 0;
} // end Enqueue_Request


//int NeoCell::Run(const char* cypher, BoltValue params, BoltValue extras, ResultCallback fn)
//{
//	results_function = fn;
//	if (!request_queue.Enqueue({ cypher, std::move(params), std::move(extras) }))
//	{
//		results_function(-1, nullptr);
//		return -3;
//	} // end if not enqueued
//
//	Add_QCount();
//	return 0;
//} // Run


int NeoCell::Fetch(BoltResult& results)
{
	do
	{
		// wait for at least one full message
		connection.Wait_Task();

		auto result = rqueue.Dequeue();
		if (!result.has_value())
			return -1;

		results = std::move(result.value());
	} while (!rqueue.Is_Empty());

	return 0;
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
int NeoCell::Get_Try_Count() const
{
	return try_count;
} // end Get_Try_Count


/**
 * @brief returns the maximum retry count allowed
 */
int NeoCell::Get_Max_Try_Count() const
{
	return max_tries;
} // end Get_Max_Try_Count


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
	return last_error;
} // end Get_Last_Error


/**
 * @brief increments the try count for connection attempts
 */
bool NeoCell::Can_Retry()
{
	if (++try_count > max_tries)
	{
		try_count = 0;		// reset it
		return false;
	} // end if not anymore

	return true;	// yes you can
} // end Increment_Try_Count


/**
 * @brief sets the maximum retry count to the number, n greater than 0
 */
void NeoCell::Set_Retry_Count(const int n)
{
	if (n > 0)
		max_tries = n;
} // end Set_Retry_Count


/**
 * @brief terminates the active connection does house cleaning.
 */
void NeoCell::Stop()
{
	if (!connection.Is_Open())
		return;

	if (connection.supported_version.major >= 5)
		Enqueue_Request({ CellCmdType::Logoff });

	// drain all requrests before terminating
	do {
		connection.Wait_Task();
		tasks.Dequeue();
	} while (!tasks.Is_Empty());

	connection.Terminate();
	Set_Running(false);

	EWake();
	if (encoder_thread.joinable())
		encoder_thread.join();

	DWake();
	if (decoder_thread.joinable())
		decoder_thread.join();
} // end Stop


/**
 * @brief the encoder loop encodes or calls the connection functions based on
 *	the command type info given by CellCommand structure. Once all the rquest_queue
 *	items are done, or flushed to peer, the thread is put to sleep until woken by
 *	Add_QCount() method or similar.
 */
void NeoCell::Encoder_Loop()
{
	while (Is_Running())
	{
		if (!equeue.Is_Empty())
		{
			auto req = equeue.Front();
			int write_ret = 0;
			if (req.has_value())
			{
				switch (req->get().type)
				{
				case CellCmdType::Run:
					tasks.Enqueue({ QueryState::Run, req->get().cb });
					write_ret = connection.Run(req->get().cypher.c_str(), req->get().params,
						req->get().extras, -1);
					break;

				case CellCmdType::Begin:
					tasks.Enqueue({ QueryState::Begin });
					write_ret = connection.Begin(req->get().params);
					break;

				case CellCmdType::Commit:
					tasks.Enqueue({ QueryState::Commit });
					write_ret = connection.Commit(req->get().params);
					break;

				case CellCmdType::Rollback:
					tasks.Enqueue({ QueryState::Rollback });
					write_ret = connection.Rollback(req->get().params);
					break;

				case CellCmdType::Logoff:
					tasks.Enqueue({ QueryState::Logoff });
					write_ret = connection.Logoff();
					break;

				default:
					//connection.err_string = "Encoder loop, command violation. Aborted thread.";
					write_ret = -2;		// critical violation
				} // end switch

				// test the return value
				if (write_ret < 0) break;
			} // end if has value

			equeue.Dequeue();		// now remove it
		} // end if not empty
		else Sleep(esleep);	// wait it out till notified.

	} // end while running
} // end Write_Loop


/**
 * @brief decoder thread sits and polls incomming requests, decodes them as they appear
 *	on the connection decoder tasks. When decoder tasks are empty are has no more to do,
 *	the thread sleeps/waits for state changes on the atomic boolean dsleep is true.
 */
void NeoCell::Decoder_Loop()
{
	bool begin_decode = true;		// used to indicate if we need to poll more
	LBStatus rc = LB_Make();	// 0k

	while (Is_Running())
	{
		if (!tasks.Is_Empty())
		{
			rc = connection.Poll_Readable();
			LBAction action = LBAction(LB_Action(rc));
			LBDomain domain = LBDomain(LB_Domain(rc));

			if (action == LBAction::LB_OK)
			{
				rc = Decode_Response(connection.read_buf.Read_Ptr(), LB_Aux(rc));
				if (!LB_OK(rc))
				{
					if (LBAction(LB_Action(rc)) == LBAction::LB_FAIL)
					{
						Set_Running(false);
						EWake();
						break;
					} // end if fail
					else if (LBAction(LB_Action(rc)) == LBAction::LB_HASMORE)
					{
						connection.read_buf.Consume(LB_Aux(rc));
						continue;	// keep polling if we have more to decode
					} // end else
				} // end if decode error

				connection.read_buf.Consume(LB_Aux(rc));
				if (tasks.Is_Empty())
					connection.read_buf.Reset();		// reset buffer if no more tasks
			} // end if begin decode
			else if ((action == LBAction::LB_FAIL && (domain == LBDomain::LB_DOM_SYS ||
				domain == LBDomain::LB_DOM_SSL)) ||
				(action == LBAction::LB_RETRY && domain == LBDomain::LB_DOM_SYS))
			{
				Set_Running(false);
				EWake();
				break;
			} // end if readable

		} // end if
		else Sleep(dsleep);	// wait it out till notified.
	} // end while running

	// on abnormal exit, handle it
	//if (!LB_OK(rc)) LB_Handle_Status(rc, this);
} // end Decoder_Loop


/**
 * @brief wakes a sleeping thread if 'equeue' encoder queue size is exactly 1
 *	or the first element, i.e. simulates waking on first message arrival after
 *	idle periods.
 */
void NeoCell::EWake()
{
	int c = esleep.fetch_add(1, std::memory_order_acq_rel);
	if (c >= 1) esleep.notify_one();
} // end EWake


/**
 * @brief wakes a sleeping decoder thead if the connection internal memeber
 *	'tasks' for decoding has size of at least one item or the first item
 *	or 0 during exiting.
 */
void NeoCell::DWake()
{
	int c = dsleep.fetch_add(1, std::memory_order_acq_rel);
	if (c >= 1) dsleep.notify_one();
} // end DWake


/**
 * @brief clear's the histogram of latencies
 */
void NeoCell::Clear_Histo()
{
	connection.latencies.Clear();
} // end Clear_Histo


/**
 * @brief causes the encoder thread to wait or simulate sleep as long as boolean atomic
 *	member/attribute 'esleep' is set to true. On change of state the sleeping thread
 *	needs to be notified through Toggle_ESleep() or Add_QCount(), which internally calls
 *	esleep.notify_one() during the first changes to the equeue or encoder queue.
 */
void NeoCell::Sleep(std::atomic<int>& ws)
{
	while (Is_Running())
	{
		int c = ws.fetch_sub(1, std::memory_order_acq_rel);
		if (c <= 0) break;		// not sleeping anymore

		ws.wait(c);
	} // end while
} // end Sleep_Encoder


/**
 * @brief sets the running state of the cell
 */
void NeoCell::Set_Running(const bool state)
{
	running.store(state, std::memory_order_release);
} // end Set_Running


/**
 * @brief returns true if the cell is running
 */
bool NeoCell::Is_Running() const
{
	return running.load(std::memory_order_acquire);
} // end Is_Running


LBStatus NeoCell::Decode_Response(u8* ptr, const size_t bytes)
{
	size_t decoded = 0; // tacks decoded bytes thus far
	LBStatus rc = 0;    // holds return values

	//Utils::Dump_Hex((const char*)ptr, bytes);

	auto task = tasks.Front();
	int total_decode = bytes + task->get().prev_bytes;
	task->get().prev_bytes = 0;				// reset the left over bytes for the next batch
	task->get().view.cursor = ptr;			// set the cursor to the start of the buffer
	task->get().view.size = total_decode;	// set the size to the number of bytes received

	while (decoded < total_decode)
	{
		rc = connection.Can_Decode(task->get().view.cursor, total_decode - decoded);
		if (!LB_OK(rc))
		{
			if (LBAction(LB_Action(rc)) == LBAction::LB_HASMORE)
			{
				task->get().prev_bytes = total_decode - decoded;	// set the left over bytes for the next batch
				return LB_Make(LBAction::LB_HASMORE, LBDomain::LB_DOM_BOLT,
					LBCode::LB_CODE_NONE, decoded);
			} // end if appended info

			return rc;
		} // end if cannot decode

		rc = connection.Decode_One(task.value());
		LBAction action = LBAction(LB_Action(rc));
		u32 aux = LB_Aux(rc);
		if (action != LBAction::LB_OK && action != LBAction::LB_HASMORE)
			return rc;

		decoded += aux;					// get the number of bytes decoded and add it to the total
		task->get().view.cursor += aux;	// move the cursor forward by the number of bytes decoded

		// are done with this task?
		if (task->get().is_done)
		{
			if (task->get().cb)
			{
				task->get().cb(task->get().result);
			} // end if callback
			else
			{
				// move the result to the result queue for user to fetch
				rqueue.Enqueue(std::move(task->get().result));
				connection.Wake();
			} // end else
			
			// clock latency
			connection.latencies.Record_Latency(
				std::chrono::high_resolution_clock::now() -
				task->get().start_clock
			);

			tasks.Dequeue();		// remove the task from the queue
			task = tasks.Front();	// get the next task to process
			if (!task.has_value()) break;	// no more tasks to process

			// set it up for the next task
			task->get().view.cursor = ptr + decoded;	// move the cursor to the next position
			task->get().view.size = bytes - decoded;	// set the remaining size to decode for the next task
		} // end if done
		
	} // end while

	return LB_OK_INFO(decoded);
} // end Decode_Response