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
	: connection(params), running(false), qcount(0), is_shutting_down(false),
	results_function(nullptr) { } // end NeoQE


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

	Add_QCount();
	Set_Running(true);
	write_thread = std::thread(&NeoCell::Write_Loop, this);
	read_thread = std::thread(&NeoCell::Read_Loop, this);
    return 0;
} // end Start


int NeoCell::Enqueue_Request(RequestParams&& req)
{
	results_function = req.results_func;
	if (!request_queue.Enqueue(std::move(req)))
	{
		results_function(-1, nullptr);
		return -2;
	} // end if not enqueued

	Add_QCount();
	return 0;
} // end Enqueue_Request



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

	// last minute burner...
	is_shutting_down.store(true, std::memory_order_release);
	qcount.notify_all();
	Wait_Thread();

	if (connection.supported_version.major >= 5)
	{
		if (connection.Logoff() < 0) Sub_QCount();
		else Add_QCount();

		Wait_Thread();
	} // end if ver >= 5+
	connection.Terminate();

	Set_Running(false);
	if (write_thread.joinable()) write_thread.join();
	if (read_thread.joinable()) read_thread.join();
} // end Stop


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


/**
 * @brief the main writer loop that services queued requests, if any. Should
 *	the queue be empty the thread goes to sleep until woken up by Add_Writer_Ref().
 */
void NeoCell::Write_Loop()
{
	while (Is_Running())
	{
		if (!request_queue.Is_Empty())
		{
			auto req = request_queue.Dequeue();
			if (req.has_value())
			{
				if (connection.Run(req->cypher.c_str(), req->params, req->extras) < 0)
					break;

			} // end if has value
		} // end if not empty
		else
		{
			Sleep_Thread();
		} // end else
	} // end while running
} // end Write_Loop


void NeoCell::Read_Loop()
{
	while (Is_Running())
	{
		if (Get_QCount() > 0)
		{
			if (int ret; (ret = connection.Poll_Readable()) < 0)
			{
				if (ret >= -2)
					break;
			} // end if readable

			//if (results_function)
			//{
			//	BoltMessage msg;
			//	int fetch_ret = connection.Fetch(msg);
			//	results_function(fetch_ret, nullptr);
			//} // end if results func callback

			Sub_QCount();		// this one is processed
		} // end if
		else
		{
			Sleep_Thread();
		} // end else sleeping
	} // end while running
} // end Read_Loop


/**
 * @brief adds 1 to track the active queries being processed by the 
 *	writer thread. If thread was sleeping it wakes it up using notify_one(),
 *	it does so only once and when the first query is being processed for the
 *	batch.
 */
void NeoCell::Add_QCount()
{
	int prev = qcount.fetch_add(1, std::memory_order_relaxed);
	if (prev == 0)
		qcount.notify_one();
} // end Toggle_Writer_State


/**
 * @brief subtracts 1 to track active queries left to process. Is called
 *	after the processing of a query is done, and when the count reaches
 *	down to 0, it calls notify_one() to activate any waiting processes.
 *	Useful if waiting on a task to compelete.
 */
void NeoCell::Sub_QCount()
{
    int prev = qcount.fetch_sub(1, std::memory_order_acq_rel);
	if (prev == 1)
		qcount.notify_one();
} // end Scout_Loop


/**
 * @brief puts the thead to sleep until the qcount atomic has something
 *	in it, i.e. has at least one task/query to process. On the fist task 
 *	or when qcount == 1, the function breaks the loop
 */
void NeoCell::Sleep_Thread()
{
	int prev = Get_QCount();
	while (prev <= 0 && !is_shutting_down.load(std::memory_order_acquire))
	{
		qcount.wait(prev);
		prev = Get_QCount();
	} // end while
} // end Sleep_Writer_Ref


/**
 * @brief puts the thread in waiting mode until all active tasks/queries
 *	have been processesed, i.e. qcount == 0.
 */
void NeoCell::Wait_Thread()
{
	int prev = Get_QCount();
	while (prev > 0 && Is_Running())
	{
	    qcount.wait(prev);
	    prev = Get_QCount();
	} // end while
} // end Wait_Thread


/**
 * @brief returns the current qcount
 */
int NeoCell::Get_QCount() const
{
	return qcount.load(std::memory_order_acquire);
} // end Get_Writer_State