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
	: conn_params(params) { } // end NeoQE


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
 * @return 0 on success. -1 on sys error, -2 app speific error s.a version not supported
 */
int NeoCell::Start()
{   
	ConnectionState s = connection.Get_State();
    if (s != ConnectionState::Disconnected)
    {
        err_string = "connection already started";
        return -2;      // app error
	} // end if already started

    if (connection.Init(&conn_params, 0) < 0)
        return -1;      // sys error in all cases

    const int total_tries = 100;
    int try_count = 0;
    int pool_offset = conn_params.pool->Get_Last_Offset();

try_again:

    connection.Set_State(ConnectionState::Connecting);
    if ( (sup_version = connection.Negotiate_Version()) >= 0)
    {
        if (sup_version >= 6.0 || sup_version >= 5.1)   // use version 6/5 hello
        {
            if (connection.Send_Hellov5() < 0)
                return -1;
        } // end if v5.1+
        else if (sup_version >= 1.0)          // version 4 and below 
        {
            if (connection.Send_Hellov4() < 0)
                return -1;
        } // end else if legacy
        else
        {
            err_string = "unsupported negotiated version";
            connection.Disconnect();
            return -2;
        } // end else
    } // end if Negotiate

    // wait on response 
    if (int ret; (ret = connection.Poll_Readable()) < 0)
    {
        if (ret == -3)
        {
            // connection closed by peer;
            // attempt reconnection a few more
            if ( (ret = connection.Reconnect()) < 0)
            {
                if (try_count++ < total_tries)
                    goto try_again;
                else return ret;
            } // end else
        } // end if

        return ret;
    } // end if

    // cleanup any extra params
	Release_Pool<BoltValue>(pool_offset);
    return 0;
} // end Start


/**
 * @brief runs a cypher query against the connected neo4j database. The driver
 *  must be in ready state to accept queries. The function appends a PULL/ALL
 *  message after the RUN message to begin fetching results.
 *
 * @param cypher the cypher query string
 * @param params optional parameters for the cypher query
 * @param extras optional extra parameters for the cypher query (see bolt specs)
 * @param n optional the numbe r of chunks to request, i.e. 1000 records
 *
 * @return 0 on success and -2 on application error
 */
int NeoCell::Run(const char* cypher, BoltValue params, BoltValue extras,
    const int n)
{
    // its ok to pipline calls too
	ConnectionState state = connection.Get_State();
    if (state != ConnectionState::Ready && state != ConnectionState::Run)
    {
        err_string = "invalid state: " + connection.State_ToString();
        return -2;  // app error
    } // end if

    // encode and flush cypher query, append a pull too
    BoltMessage run(
        BoltValue(BOLT_RUN, {
            cypher,
            params,
            extras
            })
    );

    connection.encoder.Encode(run);
    Encode_Pull(n);
    connection.Flush();

    // update the state and number of queries piped, also set the poll flag
    //  to signal recv to wait on block for incoming data
    connection.Set_State(ConnectionState::Run);
    num_queries++;
    connection.has_more = true;

    return 0;
} // end Run_Query


/**
 * @brief fetches the next record from the server after a RUN/PULL command.
 *  The function decodes into a BoltMessage object that can be inspected
 *  for record data or summary meta info.
 *
 * @param out the output message object to decode into
 *
 * @return 0 on completion, 1 if more records to come, -1 on sys error, 
 *  -2 on app error
 */
int NeoCell::Fetch(BoltMessage& out)
{
	ConnectionState state = connection.Get_State();
    if (state == ConnectionState::Run || state == ConnectionState::Pull)
    {
        // expecting success/fail for run message
        connection.has_more = true;
        if (int ret; (ret = connection.Poll_Readable()) < 0)
        {
            // check error satate
            state = connection.Get_State();
            if (state == ConnectionState::Error)
            {
                // its either a run or pull error, attempt to reset
                //  to clean this connection
                connection.has_more = true;     // poll for reset
				return Reset();     // return reset status, terminate fetch on success
            } // end if error state

            return ret;
		} // end if poll error

		state = connection.Get_State();
    } // end if ready

    if (state == ConnectionState::Streaming)
    {
        // we're streaming bytes
        int bytes = connection.decoder.Decode(connection.view.cursor, out);

        // advance the cursor by the bytes
        connection.view.cursor += bytes;
        connection.view.offset += (size_t)bytes;

        // test if we completed?
        if (out.msg.struct_val.tag == BOLT_SUCCESS)
        {
            if (!connection.Is_Record_Done(out))
				return 1;   // more records to come

            connection.view.summary_meta = out.msg;
			connection.read_buf.Reset();    // reset the read buffer for next rounds
            return 0;
        } // end if

		// we're not done yet
        if (connection.view.offset >= connection.view.size)
            connection.Set_State(ConnectionState::Pull);
    } // end else if streaming

    return 1;
} // end Fetch


/**
 * @brief Begins a transaction with the database, this is a manual transaction
 *  that requires commit or rollback to finish.
 *
 * @param options optional parameters for the transaction
 *
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoCell::Begin(const BoltValue& options)
{
    if (transaction_count++ > 0)
        return 0;       // already has some

	// keep track of pool, from here on we allocate from it
	size_t offset = conn_params.map_val.size << 1;

    BoltMessage begin(
        BoltValue(BOLT_BEGIN, {
            options
            })
    );

    connection.encoder.Encode(begin);
    if (!connection.Flush())
    {
		Release_Pool<BoltValue>(offset);
        return -1;
    } // end if no flush


    // wait for success
    int ret = connection.Poll_Readable();
    Release_Pool<BoltValue>(offset);
    
    return ret;
} // end Begin_Transaction


/**
 * @brief Commits the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 *
 * @param options optional parameters for the commit
 *
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoCell::Commit(const BoltValue& options)
{
    if (transaction_count-- > 0)
        return 0;       // got a few more

    // keep track of pool, from here on we allocate from it
    size_t offset = conn_params.map_val.size << 1;

    BoltMessage commit(
        BoltValue(BOLT_COMMIT, {
            options
            })
    );
    
    connection.encoder.Encode(commit);
    if (!connection.Flush())
		return -1;

    // wait for success
    int ret = connection.Poll_Readable();
	Release_Pool<BoltValue>(offset);

    return ret;
} // end Commit_Transaction


/**
 * @brief Rolls back the current transaction, this is a manual transaction that
 *  requires Begin_Transaction to start.
 *
 * @param options optional parameters for the rollback
 *
 * @return 0 on success -1 on system error -2 on application error
 */
int NeoCell::Rollback(const BoltValue& options)
{
    if (transaction_count-- > 0)
        return 0;       // not yet

    // keep track of pool, from here on we allocate from it
    size_t offset = conn_params.map_val.size << 1;

    BoltMessage rollback(
        BoltValue(BOLT_ROLLBACK, {
            options
            })
    );
    
    connection.encoder.Encode(rollback);
    if (!connection.Flush())
        return -1;

    // wait for success
    int ret = connection.Poll_Readable();
    Release_Pool<BoltValue>(offset);

    return ret;
} // end Rollback_Transaction


/*@brief encodes a PULL message and sends it. Useful during reactive style fetch
*
* @param n the number of chunks to to fetch at once, defaulted to - 1 to fetch
* everything.
*
* @return 0 on success -1 on sys error, left for caller to decide its fate
*/
int NeoCell::Pull(const int n)
{
    Encode_Pull(n);
    if (!connection.Flush())
        return -1;

    return 0;
} // end Pull

 
/**
 * @brief encodes a DISCARD message and sends it. Useful during reactive style fetch
 * 
 * @param n the number of chunks to to discard at once, defaulted to -1 to discard
 * 
 * @return 0 on success -1 on sys error, left for caller to decide its fate
 */
int NeoCell::Discard(const int n)
{
	ConnectionState state = connection.Get_State();
    if (state != ConnectionState::Streaming)
        return 0;       // ignore

	size_t offset = conn_params.map_val.size << 1;  

    BoltValue bv(BOLT_DISCARD, {
        BoltValue({
            mp("n", n),
            mp("qid", connection.client_id)
            })
        });

    if (!connection.Flush_And_Poll(bv))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
	} // end if

	Release_Pool<BoltValue>(offset);
    return 0;
} // end Discard


/**
 * @brief sends a TELEMETRY message to server with the api level used by client.
 *  This is a v5.1+ feature.
 * 
 * @param api the api level used by client
 * 
 * @return 0 on success, -1 on sys error
 */
int NeoCell::Telemetry(const int api)
{
	ConnectionState state = connection.Get_State();
    if (state != ConnectionState::Ready)
        return 0;       // ignore

	size_t offset = conn_params.map_val.size << 1;

    BoltValue bv(BOLT_TELEMETRY, { api });
    if (!connection.Flush_And_Poll(bv))
    {
        Release_Pool<BoltValue>(offset);
        return -1;
	} // end if

	Release_Pool<BoltValue>(offset);
    return 0;
} // end Telemetry


/**
 * @brief resets the current active connection should it be in FAILED state.
 *  The client connection then must reset its state to READY before accepting
 *  any more new requests. The state of the connection must be in ERROR state
 *  prior to calling this function.
 * 
 * @return 0 on success always
 */
int NeoCell::Reset()
{
	// memorize the last pool offset to cleanup later
	size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

	ConnectionState state = connection.Get_State();
    if (state != ConnectionState::Error)
        return 0;

    BoltValue bv(BOLT_RESET, {});
    if (!connection.Flush_And_Poll(bv))
    {
        Release_Pool<BoltValue>(offset);
        return -2;  // connection is closed with details in last error
	} // end if

	Release_Pool<BoltValue>(offset);
    return 0;
} // end Reset


/**
 * @brief sends a LOGOFF message to server to gracefully logoff from the
 *  current active connection. This is a v5.1+ feature.
 * 
 * @return 0 on success always
 */
int NeoCell::Logoff()
{
	// get the last offset in the pool to cleanup later
	size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

    ConnectionState s = connection.Get_State();
    if (s == ConnectionState::Disconnected)
        return 0;       // ignore

    connection.Set_State(ConnectionState::Connecting);
    BoltMessage off(BoltValue(BOLT_LOGOFF, {}));
    connection.encoder.Encode(off);

	// ignore the response, closing anyways
    connection.Flush_And_Poll(off.msg);

	Release_Pool<BoltValue>(offset);
    return 0;
} // end Logoff


/**
 * @brief sends a GOODBYE message to server to gracefully close the current
 *  active connection.
 * 
 * @return 0 on success, -1 on sys error
 */
int NeoCell::Goodbye()
{
	// memorize last pool offset to cleanup later
	size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

	ConnectionState state = connection.Get_State();
    if (state == ConnectionState::Disconnected)
        return 0;       // already done

    BoltValue gb(BoltValue(BOLT_GOODBYE, {}));
    connection.encoder.Encode(gb);

    if (!connection.Flush())
        return -1;

	Release_Pool<BoltValue>(offset);
    return 0;
} // end Goodbye


/**
 * @brief this is an old skool way of error handling in neo4j bolt protocol. It
 *  basically acknowledges the last failure message sent by server and resets
 * I wrote it for completeness.
 * 
 * @eturn 0 on success, -1 on sys error
 */
int NeoCell::Ack_Failure()
{
    // memorize last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

	ConnectionState s = connection.Get_State();
    if (s != ConnectionState::Error)
        return 0;       // already done

    BoltValue ack(BoltValue(BOLT_ACK_FAILURE, {}));
    if (!connection.Flush_And_Poll(ack))
        return -1;

    Release_Pool<BoltValue>(offset);
    return 0;
} // end Failure


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
    ConnectionState s = connection.Get_State();
    if (s != ConnectionState::Disconnected)
    {
        // would be ignored if not supported anyways
        if (sup_version >= 5.1)
            Logoff();

        Goodbye();
    } // end if not connecting

	connection.Terminate();
} // end Stop


//===============================================================================|
/**
 * @brief encodes a PULL message after a RUN command to fetch all results.
 *
 * @param n the number of chunks to to fetch at once, defaulted to -1 to fetch
 *  everything.
 */
void NeoCell::Encode_Pull(const int n)
{
    // memorize last pool offset to cleanup later
    size_t offset = GetBoltPool<BoltValue>()->Get_Last_Offset();

    BoltMessage pull(BoltValue(
        BOLT_PULL, {
            {mp("n", n), mp("qid",-1)}
        }));

    connection.encoder.Encode(pull);

	Release_Pool<BoltValue>(offset);
} // end Send_Pull