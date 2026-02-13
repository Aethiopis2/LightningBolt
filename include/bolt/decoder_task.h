/**
 * @file decoder_task.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @brief
 * @version 1.0
 * @date 14th of May 2025, Wednesday
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include <cmath>
#include "connection/neoconnection.h"
#include "bolt/bolt_message.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * LightningBolt states over bolt
 */
enum class QueryState : u8 {
    Connection,     // special 1: used during HELLO 
    Logon,          // special 2: used in v5.x+ after HELLO
    Logoff,         // special 3: expecting logoff success/fail message

    Run,            // driver is expecting run success/fail message
    Pull,           // driver is expecting pull success/fail message
    Streaming,      // driver is in a streaming state; i.e. reading buffer
    Discard,        // driver is expecting discard success/fail message
    Begin,          // driver is expecting begin trx success/fail message
    Commit,         // driver is expecting commit trx success/fail message
    Rollback,       // driver is expecting rollback trx success/fail message
    Route,          // driver is expecting route success/fail message
    Reset,          // driver is expecting reset success/fail message
    Telemetry,      // driver is expecting telemetry success/fail message
    Ack_Failure,    // driver is expecting ack_failure success/fail message
    Error,          // driver has encountered errors  
};
constexpr int QUERY_STATES = 15;



/**
 * @brief the result of a bolt query. It consists of three fields, the fields
 *  names for the query result, a list of records, and a summary detail, as per
 *  driver specs for neo4j
 */
struct BoltResult
{
    BoltMessage fields;     // the field names for the record
    std::vector<BoltValue> records;      // the actual list of records returned
    BoltMessage record;     // a single bolt record
    BoltMessage summary;    // the summary message at end of records
    BoltValue error = BoltValue::Make_Unknown();        // used when error occurs
    int messages{ 0 };      // count of messages contained within records

    int client_id = 0;

    bool Success() const
    {
        return error.type != BoltType::Unk;
    } // end successul
};


/**
 * @brief a view points at the next row/value to decode in a single
 *  bolt request/query. Since Bolt returns pipelined requests in the order
 *  sent, we can queue each response for processing in a ring buffer.
 */
struct BoltView
{
    u8* cursor;                 // the current address
    u64 offset;                 // the cursor offset
    size_t size;                // the size of view into buffer
};


struct ConnectionStat
{
    using clock = std::chrono::steady_clock;
    using duration = std::chrono::nanoseconds;
    static constexpr size_t HIST_BUCKETS = 64;

    u64 samples{ 0 };
    duration best_latency{ duration::max() };
    duration worst_latency{ duration::zero() };
    duration total_latency{ duration::zero() }; // for avg only

    u64 latency_hist[HIST_BUCKETS]{};
    u64 total_bytes_written{ 0 };
    u64 total_bytes_read{ 0 };

    double avg_bytes_written{ 0 };
    double avg_bytes_read{ 0 };


    /**
     * @brief records a latency sample
     *
     * @param d the duration to record
     */
    inline void Record_Latency(duration d)
    {
        ++samples;
        total_latency += d;

        if (d < best_latency)  best_latency = d;
        if (d > worst_latency) worst_latency = d;

        latency_hist[Bucket_For(d)]++;
    } // end Record_Latency


    /**
     * @brief computes the average latency over all samples taken
     *
     * @return duration the average latency
     */
    inline duration Avg_Latency() const
    {
        return samples ? duration(total_latency / samples) : duration::zero();
    } // end Avg_Latency


    /**
     * @brief computes the p-th percentile latency
     *
     * @param p the percentile to compute in [0.0, 1.0]
     */
    duration Percentile(double p) const
    {
        if (!samples) return duration::zero();

        uint64_t target = static_cast<uint64_t>(std::ceil(p * samples));
        uint64_t cumulative = 0;

        for (size_t i = 0; i < HIST_BUCKETS; ++i)
        {
            cumulative += latency_hist[i];
            if (cumulative >= target)
            {
                // representative value for bucket i
                uint64_t ns = 1ULL << (i + 1);
                return duration(ns);
            } // end if
        } // end for

        return worst_latency;
    } // end Percentile


    /**
     * @brief computes the histogram bucket for a given duration
     *
     * @param d the duration to compute bucket for
     * @return size_t the bucket index
     */
    static inline size_t Bucket_For(ConnectionStat::duration d)
    {
        u64 ns = d.count();
        // avoid zero
        ns |= 1;
        return std::min<size_t>(
            ConnectionStat::HIST_BUCKETS - 1,
            63 - __builtin_clzll(ns)
        );
    } // end Bucket_For
};


/**
 * @brief holds the state of a pipelined query along with its view into
 *  the buffer and the decoded results that point right at that buffer
 *  which can be shallow copied to a caller.
 */
struct DecoderTask
{
    QueryState state;       // current state of the query
    BoltView view;          // view into the buffer for this query
    BoltResult result;      // results of bolt values
    ConnectionStat latency; // latency stats

    int total_bytes{ 0 };   // total bytes recvd

    std::function<void(BoltResult&)> cb = nullptr;  // a callback for async procs ideal for web apps.
};