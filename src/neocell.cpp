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
    : conn_params(params)
{
    // first make sure everything in the key section is lower case
    //for (size_t i = 0; i < conn_params.map_val.size; i++) 
    //{
    //    BoltValue* v = conn_params.pool->Get(conn_params.map_val.key_offset);
    //    String_ToLower((char*)v->str_val.str, v->str_val.length);
    //} // end for
} // end NeoQE


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
 * @return 0 on success. -1 on sys error, -2 app speific error s.a version not supported
 */
int NeoCell::Start()
{   
    if (writer.Init(&conn_params, 0) < 0)
        return -1;      // sys error in all cases

    const int total_tries = 100;
    int try_count = 0;

try_again:

    writer.Set_State(ConnectionState::Connecting);
    if ( (sup_version = writer.Negotiate_Version()) >= 0)
    {
        if (sup_version >= 6.0 || sup_version >= 5.1)   // use version 6/5 hello
        {
            if (writer.Send_Hellov5() < 0)
                return -1;
        } // end if v5.1+
        else if (sup_version >= 1.0)          // version 4 and below 
        {
            if (writer.Send_Hellov4() < 0)
                return -1;
        } // end else if legacy
        else
        {
            err_string = "unsupported negotiated version";
            writer.Disconnect();
            return -2;
        } // end else
    } // end if Negotiate

    // wait on response 
    if (int ret; (ret = writer.Poll_Readable()) < 0)
    {
        if (ret == -3)
        {
            // connection closed by peer;
            // attempt reconnection a few more
            if (writer.Reconnect() < 0)
                return -1;
            else
            {
                if (try_count++ < total_tries)
                    goto try_again;
                else return ret;
            } // end else
        } // end if

        return ret;
    } // end if

    return 0;
} // end Start


/**
 * @brief terminates the active connection does house cleaning.
 */
void NeoCell::Stop()
{
    ConnectionState s = writer.Get_State();
    if (s != ConnectionState::Disconnected)
    {
        // would be ignored if not supported anyways
        if (sup_version >= 5.1)
            Logoff();
        Goodbye();
    } // end if not connecting

    writer.Disconnect();
} // end Stop


////===============================================================================|
///**
// * @brief runs a cypher query against the connected neo4j database. The driver
// *  must be in ready state to accept queries. The function appends a PULL/ALL
// *  message after the RUN message to begin fetching results.
// *
// * @param cypher the cypher query string
// * @param params optional parameters for the cypher query
// * @param extras optional extra parameters for the cypher query (see bolt specs)
// * @param n optional the numbe r of chunks to request, i.e. 1000 records
// *
// * @return 0 on success and -2 on application error
// */
//int NeoConnection::Run_Query(const char* cypher, BoltValue params, BoltValue extras,
//    const int n)
//{
//    // its ok to pipline calls too
//    if (state != BoltState::Ready && state != BoltState::Run)
//    {
//        err_string = "invalid state: " + State_ToString();
//        return -2;  // app error
//    } // end if
//
//    // update the state and number of queries piped, also set the poll flag
//    //  to signal recv to wait on block for incoming data
//    Set_State(BoltState::Run);
//    num_queries++;
//    has_more = true;
//
//    // encode and flush cypher query, append a pull too
//    BoltMessage run(
//        BoltValue(BOLT_RUN, {
//            cypher,
//            params,
//            extras
//            })
//    );
//
//    encoder.Encode(run);
//    Encode_Pull(n);
//    Flush();
//
//    return 0;
//} // end Run_Query
//
////===============================================================================|
//int NeoConnection::Fetch(BoltMessage& out)
//{
//    if (state == BoltState::Run || state == BoltState::Pull)
//    {
//        // expecting success/fail for run message
//        has_more = true;
//        if (int ret; (ret = Poll_Readable()) < 0)
//            return ret;
//    } // end if ready
//
//    if (state == BoltState::Streaming)
//    {
//        // we're streaming bytes
//        int bytes = decoder.Decode(view.cursor, out);
//
//        // advance the cursor by the bytes
//        view.cursor += bytes;
//        view.offset += (size_t)bytes;
//
//        // test if we completed?
//        if (out.msg.struct_val.tag == BOLT_SUCCESS)
//        {
//            view.summary_meta = out.msg;
//            Set_State(BoltState::Ready);
//            read_buf.Reset();
//            return 0;
//        } // end if
//
//        if (view.offset >= view.size)
//            Set_State(BoltState::Pull);
//    } // end else if streaming
//
//    return 1;
//} // end Fetch
//
////===============================================================================|
///**
// * @brief Begins a transaction with the database, this is a manual transaction
// *  that requires commit or rollback to finish.
// *
// * @param options optional parameters for the transaction
// *
// * @return 0 on success -1 on system error -2 on application error
// */
//int NeoConnection::Begin_Transaction(const BoltValue& options)
//{
//    if (transaction_count++ > 0)
//        return 0;       // already has some
//
//    BoltMessage begin(
//        BoltValue(BOLT_BEGIN, {
//            options
//            })
//    );
//    encoder.Encode(begin);
//    Flush();
//
//    // wait for success
//    Poll_Readable();
//    return 0;
//} // end Begin_Transaction
//
//
////===============================================================================|
///**
// * @brief Commits the current transaction, this is a manual transaction that
// *  requires Begin_Transaction to start.
// *
// * @param options optional parameters for the commit
// *
// * @return 0 on success -1 on system error -2 on application error
// */
//int NeoConnection::Commit_Transaction(const BoltValue& options)
//{
//    if (transaction_count-- > 0)
//        return 0;       // got a few more
//
//    BoltMessage commit(
//        BoltValue(BOLT_COMMIT, {
//            options
//            })
//    );
//    encoder.Encode(commit);
//    Flush();
//
//    // wait for success
//    Poll_Readable();
//    return 0;
//} // end Commit_Transaction
//
//
////===============================================================================|
///**
// * @brief Rolls back the current transaction, this is a manual transaction that
// *  requires Begin_Transaction to start.
// *
// * @param options optional parameters for the rollback
// *
// * @return 0 on success -1 on system error -2 on application error
// */
//int NeoConnection::Rollback_Transaction(const BoltValue& options)
//{
//    if (transaction_count-- > 0)
//        return 0;       // not yet
//
//    BoltMessage rollback(
//        BoltValue(BOLT_ROLLBACK, {
//            options
//            })
//    );
//    encoder.Encode(rollback);
//    Flush();
//
//    // wait for success
//    Poll_Readable();
//    return 0;
//} // end Rollback_Transaction
//
////===============================================================================|
///*@brief encodes a PULL message and sends it. Useful during reactive style fetch
//*
//* @param n the number of chunks to to fetch at once, defaulted to - 1 to fetch
//* everything.
//*
//* @return 0 on success -1 on sys error, left for caller to decide its fate
//*/
//int NeoConnection::Pull(const int n)
//{
//    Encode_Pull(n);
//    if (!Flush())
//        return -1;
//
//    return 0;
//} // end Pull
//
////===============================================================================|
//int NeoConnection::Reset()
//{
//    if (state != BoltState::Error || state != BoltState::Ready)
//        return 0;
//
//    BoltValue bv(BOLT_RESET, {});
//    if (!Flush_And_Poll(bv))
//        return -1;
//
//    return 0;
//} // end Reset
//
////===============================================================================|
//int NeoConnection::Discard(const int n)
//{
//    if (state != BoltState::Streaming)
//        return 0;       // ignore
//
//    BoltValue bv(BOLT_DISCARD, {
//        BoltValue({
//            mp("n", n),
//            mp("qid", (int)client_id)
//            })
//        });
//
//    if (!Flush_And_Poll(bv))
//        return -1;
//
//    return 0;
//} // end Discard
//
////===============================================================================|
//int NeoConnection::Telemetry(const int api)
//{
//    if (state != BoltState::Ready)
//        return 0;       // ignore
//
//    BoltValue bv(BOLT_TELEMETRY, { api });
//    if (!Flush_And_Poll(bv))
//        return -1;
//
//    return 0;
//} // end Telemetry
//
//===============================================================================|
int NeoCell::Logoff()
{
    //if (state == DriverState::Disconnected)
    //    return 0;       // ignore

    //Set_State(DriverState::Connecting);
    //BoltMessage off(BoltValue(BOLT_LOGOFF, {}));
    //writer.encoder.Encode(off);

    //if (!writer.Flush())
    //    return -1;

    return 0;
} // end Logoff

//===============================================================================|
int NeoCell::Goodbye()
{
    //if (state == DriverState::Disconnected)
    //    return 0;       // already done

    BoltValue gb(BoltValue(BOLT_GOODBYE, {}));
    /*encoder.Encode(gb);

    if (!Flush())
        return -1;

    Close_Driver();
    Set_State(BoltState::Disconnected);*/
    return 0;
} // end Goodbye
//
////===============================================================================|
//int NeoConnection::Ack_Failure()
//{
//    if (state != BoltState::Error)
//        return 0;       // already done
//
//    BoltValue ack(BoltValue(BOLT_ACK_FAILURE, {}));
//    if (!Flush_And_Poll(ack))
//        return -1;
//
//    return 0;
//} // end Failure
//
////===============================================================================|
///**
// * @brief kills the active connection if not already closed; reset's buffers and
// *  turns driver state to disconnected.
// */
//void NeoConnection::Close_Driver()
//{
//    Set_State(BoltState::Disconnected);
//    if (!Is_Closed())
//        CLOSE(fd);
//
//    write_buf.Reset();
//    read_buf.Reset();
//} // end Close_Driver
//
////===============================================================================|
///**
// * @brief sends the contents of write buffer to the connected peer.
// *
// * @return a true on success alas false
// */
//bool NeoConnection::Poll_Writable()
//{
//    while (!write_buf.Empty())
//    {
//        ssize_t n = Send(write_buf.Read_Ptr(), write_buf.Size());
//        if (n <= 0)
//        {
//            if (errno == EAGAIN || errno == EWOULDBLOCK)
//                return false;
//
//            Stop();
//            return false;
//        } // end if
//
//        write_buf.Consume(n);
//    } // end while
//
//    return true;
//} // end Poll_Writable
//
////===============================================================================|
///**
// * @brief waits on recieve /recv system call on blocking mode. It stops recieving
// *  when a compelete bolt packet is recieved or the consuming routine has deemed
// *  it necessary to stop fetching by setting has_more to false. However, once done
// *  it must be reset back for the loop to continue. Should the buffer lack space
// *  to recv chunks it grows to accomodiate more.
// * Optionally it can be controlled to shrink back inside of a pool based on some
// *  statistically collected traffic data.
// */
//int NeoConnection::Poll_Readable()
//{
//    // do we need to grow for space
//    if (read_buf.Writable_Size() < 256)
//        read_buf.Grow(read_buf.Capacity() * 2);
//
//    while (has_more)
//    {
//        ssize_t n = Recv(read_buf.Write_Ptr(), read_buf.Writable_Size());
//        if (n <= 0)
//        {
//            if (n == 0 || errno == EINTR)
//                continue;
//            else
//            {
//                Close_Driver();
//                return -1;
//            } // end else
//        } // end if
//
//        // if data is completely received push to response handler
//        bytes_to_decode += n;
//        if (Recv_Completed(bytes_to_decode))
//        {
//            if (!Decode_Response(read_buf.Read_Ptr(), bytes_to_decode))
//                return -2;
//
//            read_buf.Consume(bytes_to_decode);
//            bytes_to_decode = 0;
//            //has_more = false;   // persume done
//        } // end if complete
//
//        read_buf.Advance(n);
//    } // end while
//
//    return 0;
//} // end Poll_Readable
//
////===============================================================================|
///**
// * @brief makes sure all the contents of the write buffer has been written to the
// *  sending buffer and kernel is probably sending it.
// *
// * @return a true on success
// */
//bool NeoConnection::Flush()
//{
//    while (!write_buf.Empty())
//    {
//        if (Poll_Writable())
//            return false;   // has to be a syscall error always
//
//        // Optional: safety guard
//        if (write_buf.Size() > 0)
//        {
//            std::this_thread::yield();  // Allow other I/O threads time
//        } // end if
//    } // end while
//
//    write_buf.Reset();
//    return true;
//} // end Flush
//
//////===============================================================================|
/////**
//// * @brief encodes a PULL message after a RUN command to fetch all results.
//// *
//// * @param n the number of chunks to to fetch at once, defaulted to -1 to fetch
//// *  everything.
//// */
////void NeoConnection::Encode_Pull(const int n)
////{
////    BoltMessage pull(BoltValue(
////        BOLT_PULL, {
////            {mp("n", n), mp("qid",-1)}
////        }));
////
////    encoder.Encode(pull);
////} // end Send_Pull

std::string NeoCell::Get_Last_Error() const
{
    return writer.Get_Last_Error();
} // end Get_Last_Error