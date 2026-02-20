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
	  esleep(false), dsleep(false), try_count(0), max_tries(5), connect_duration(0)
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
	auto start_time = std::chrono::high_resolution_clock::now();

	// clear previous errors if any and our histogram
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
	auto result = connection.results.Dequeue();
	if (result.has_value())
	{
		if (result->Is_Error())
		{
			last_error = result->err.ToString();
			return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_NEO4J);
		} // end if failed
	} // end if
	
	connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - start_time).count();

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
	// wait for at least one full message
	connection.Wait_Task();

	auto qs = connection.results.Dequeue();
	if (!qs.has_value())
		return -1;

	results = std::move(qs.value());

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
 * @brief returns the duration in milliseconds to complete a connection, including
 *	tcp connection and or ssl connection and ver negotitation and hello and logon
 *	authentication full round trip.
 */
u64 NeoCell::Get_Connection_Time() const
{
	return connect_duration;
} // end Get_Connecton_Time


u64 NeoCell::Percentile(double p) const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		connection.latencies.Percentile(p)).count();
} // end Percentile


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
		connection.tasks.Dequeue();
	} while (!connection.tasks.Is_Empty());

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
					write_ret = connection.Run(req->get().cypher.c_str(), req->get().params,
						req->get().extras, -1, req->get().cb);
					break;

				case CellCmdType::Begin:
					write_ret = connection.Begin(req->get().params);
					break;

				case CellCmdType::Commit:
					write_ret = connection.Commit(req->get().params);
					break;

				case CellCmdType::Rollback:
					write_ret = connection.Rollback(req->get().params);
					break;

				case CellCmdType::Logoff:
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
	bool has_more = false;		// used to indicate if we need to poll more
	LBStatus rc = LB_Make();	// 0k

	while (Is_Running())
	{
		if (!connection.tasks.Is_Empty() || has_more)
		{
			rc = connection.Poll_Readable();
			LBAction action = LBAction(LB_Action(rc));
			LBDomain domain = LBDomain(LB_Domain(rc));

			if ((action == LBAction::LB_FAIL && (domain == LBDomain::LB_DOM_SYS || 
				domain == LBDomain::LB_DOM_SSL)) ||
				(action == LBAction::LB_RETRY && domain == LBDomain::LB_DOM_SYS) )
			{
				Set_Running(false);
				EWake();
				break;
			} // end if readable
			else if (action == LBAction::LB_HASMORE)
			{
				has_more = true;
				continue;
			} // end else waiting for more

			has_more = false;

			if (connection.tasks.Is_Empty() && connection.results.Is_Empty())
			{
				connection.read_buf.Reset();    // reset buffer if no more tasks
			} // end if no more tasks
		} // end if
		else
		{
			//if (!dsleep.load(std::memory_order_acquire))
			{
				Sleep(dsleep);
			} // end if sleeping
		} // end else no more tasks, sleep it out
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
	esleep.store(false, std::memory_order_release);
	if (equeue.Size() <= 1)
	{
		esleep.notify_one();		// wake the encoder thread if sleeping
	} // end equeue
} // end  Toggle_ESleep


/**
 * @brief wakes a sleeping decoder thead if the connection internal memeber
 *	'tasks' for decoding has size of at least one item or the first item
 *	or 0 during exiting.
 */
void NeoCell::DWake()
{
	dsleep.store(false, std::memory_order_release);
	if (connection.tasks.Size() <= 1) dsleep.notify_one();
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
void NeoCell::Sleep(std::atomic<bool>& ws)
{
	while (Is_Running())
	{
		bool bsleep = ws.load(std::memory_order_acquire);
		if (!bsleep) break;		// not sleeping anymore

		ws.wait(bsleep);
	} // end while

	// force sleep on next entry
	ws.store(true, std::memory_order_release);
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