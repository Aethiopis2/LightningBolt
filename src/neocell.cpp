/**
 * @brief implementation detials for NeoQE, the Query Engine
 * 
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @date created 10th of December 2025, Wednesday
 * @date updated 12th of Decemeber 2025, Friday
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
NeoCell::NeoCell(BoltValue params)
	: connection(params), running(false), twait(true), esleep(false), dsleep(false)
{ } // end NeoCell


/**
 * @brief house cleanup via Stop()
 */
NeoCell::~NeoCell()
{
	Stop();
	GetBoltPool<BoltValue>()->Reset_All();   // reset the pool
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
int NeoCell::Start(const int id)
{   
	if (int ret; (ret = connection.Init(id)) < 0)
		return ret;

	EWake();
	Set_Running(true);
	encoder_thread = std::thread(&NeoCell::Encoder_Loop, this);
	decoder_thread = std::thread(&NeoCell::Decoder_Loop, this);

	// wait for result
	Wait_Task();
    return read_ret;	// should be ready
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
	Wait_Task();

	if (read_ret < 0)
		return read_ret;

	auto qs = connection.results.Dequeue();
	if (!qs.has_value())
		return -1;

	results = qs.value();

	if (connection.results.Is_Empty())
		connection.read_buf.Reset();
	return 0;
} // end Fetch

/**
 * @brief returns the last error string from the active connection
 */
std::string NeoCell::Get_Last_Error() const
{
    return connection.Get_Last_Error();
} // end Get_Last_Error


/**
 * @brief terminates the active connection does house cleaning.
 */
void NeoCell::Stop()
{
	if (connection.Is_Closed())
		return;

	if (connection.supported_version.major >= 5)
		Enqueue_Request({ CellCmdType::Logoff });
	
	// drain all requrests before terminating
	do {
		Wait_Task();
	} while (!connection.tasks.Is_Empty());

	connection.Terminate();
	Set_Running(false);

	EWake();
	if (encoder_thread.joinable()) 
		encoder_thread.join();

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
			if (req.has_value())
			{
				switch (req->get().type)
				{
				case CellCmdType::Run:
					write_ret = connection.Run(req->get().cypher.c_str(), req->get().params, 
						req->get().extras, -1, req->get().ecb, req->get().cb);

					if (req->get().ecb) req->get().ecb(write_ret, nullptr);
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
					connection.err_string = "Encoder loop, command violation. Aborted thread.";
					write_ret = -2;		// critical violation
				} // end switch

				// test the return value
				if (write_ret < 0) break;
			} // end if has value

			equeue.Dequeue();		// now remove it
		} // end if not empty
		else
		{
			esleep.store(true, std::memory_order_release);
			Sleep(esleep);	// wait it out till notified.
		} // end else
	} // end while running
} // end Write_Loop


/**
 * @brief decoder thread sits and polls incomming requests, decodes them as they appear
 *	on the connection decoder tasks. The decoder is also responsible for decrementing
 *	the added qcounts after they are persumed to be processed. The thread uses, attribute
 *	read_ret int value to convey error messages beyond the thread. Shouls no tasks exist
 *	or should it not fetch more, has_more = false, then it sleeps until woken by
 *	Add_QCount().
 */
void NeoCell::Decoder_Loop()
{
	bool has_more = false;	// used to indicate if we need to poll more
	while (Is_Running())
	{
		int prev_tasks = static_cast<int>(connection.tasks.Size());
		if (!connection.tasks.Is_Empty() || !equeue.Is_Empty() || has_more)
		{
			if ((read_ret = connection.Poll_Readable()) < 0)
			{
				if (read_ret >= -2)
				{
					Set_Running(false);
					twait.store(false, std::memory_order_release);
					twait.notify_one();
					EWake();
					break;
				} // end if fatal
			} // end if readable
			else if (read_ret > 0)
			{
				has_more = true;
				continue;
			} // end else waiting for more

			has_more = false;
			twait.store(false, std::memory_order_release);
			twait.notify_one();
		} // end if
		else
		{
			dsleep.store(true, std::memory_order_release);
			Sleep(dsleep);
		} // end else sleeping
	} // end while running
} // end Read_Loop


/**
 * @brief wakes a sleeping thread if 'equeue' encoder queue size is exactly 1
 *	or the first element, i.e. simulates waking on first message arrival after
 *	idle periods. Less than equal to 1 helps here because I want to wake a
 *	sleeping encoder thread during exits.
 */
void NeoCell::EWake()
{
	esleep.store(false, std::memory_order_release);
	if (equeue.Size() <= 1)
	{
		esleep.notify_one();		// wake the encoder thread if sleeping
		DWake();		// since tied to it, wake decoder too
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
} // end Sleep_Encoder


/**
 * @brief causes a thread to wait for a decoder task to be ready, signaled when
 *	deocder thread returns 0 from 'Connection::Poll_Readable' to imply complete
 *	message recieved. It resets the value upon exit from the loop to make sure
 *	the next process waits for the next task decoding completion as expected.
 */
void NeoCell::Wait_Task()
{
	while (Is_Running())
	{
		bool bwait = twait.load(std::memory_order_acquire);
		if (!bwait)
			break;	// wait no more

		twait.wait(bwait);	 // task is not ready, we wait
	} // end while
	
	// reset it
	twait.store(true, std::memory_order_release);
} // end Block_Until


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