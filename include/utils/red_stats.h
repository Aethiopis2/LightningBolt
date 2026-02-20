/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date created 14th of May 2025, Wednesday
 * @date update 18th of Feburary 2026, Tuesday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "basics.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * @brief a structure that helps in measurments of latencies as a histogram of
 *  distributions. Makes use of bit bucket in which ith bucket is represented by
 *  the range [2^i,2^i+1) nanoseconds.
 */
struct LatencyHistogram
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
     * @brief computes the average or wall latency over all samples taken
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
     * @brief clears stored histogram
     */
    inline void Clear()
    {
        memset(latency_hist, 0, sizeof(s64) * HIST_BUCKETS);
    } // end Clear


    /**
     * @brief computes the histogram bucket for a given duration
     *
     * @param d the duration to compute bucket for
     * @return size_t the bucket index
     */
    static inline size_t Bucket_For(LatencyHistogram::duration d)
    {
        u64 ns = d.count();
        // avoid zero
        ns |= 1;
        return std::min<size_t>(
            LatencyHistogram::HIST_BUCKETS - 1,
            63 - __builtin_clzll(ns)
        );
    } // end Bucket_For
};