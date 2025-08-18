#include <chrono>
#include <vector>
#include <iostream>
#include "bolt/bolt_encoder.h"
#include "bolt/bolt_decoder.h"
#include "bolt/bolt_buf.h"
#include "bolt/bolt_message.h"
#include "bolt/boltvalue_pool.h"



#include "utils/utils.h"
#include "utils/errors.h"

using namespace std::chrono;

// Benchmark helper
template <typename Func>
uint64_t benchmark(const std::string& label, Func&& f, size_t iterations) {
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) f();
    auto end = high_resolution_clock::now();
    uint64_t ns = duration_cast<nanoseconds>(end - start).count();
    std::cout << label << ": " << (ns / iterations) << " ns/iter\n";
    return ns;
}

int main() {
    constexpr size_t iterations = 1'000'000;


    // === 1. Streaming 100 Cypher packets ===
    std::vector<BoltValue> streamOps;
    streamOps.reserve(100);
    for (int i = 0; i < 100; ++i) {
        streamOps.emplace_back(BoltValue({
            mp("statement", "RETURN $x + $y"),
            mp("parameters", BoltValue({
                mp("x", i),
                mp("y", i * 2)
            }))
        }));
    }

    
    benchmark("Streaming 100 Cypher packets encoding", [&] {
        for (const auto& op : streamOps) {
            BoltBuf buf(512);
            BoltEncoder encoder(buf);
            encoder.Encode(op);
        }
    }, iterations);

    // === 2. Batched Bolt list of 10,000 items ===
  
    // Pre-allocate reusable buffer for batch encoding
    BoltBuf batchBuf(8 * 1024 * 1024);  // 8MB
    iZero(batchBuf.Data(), 8*1024*1024);
    BoltEncoder encoder(batchBuf);
    BoltValue tags = BoltValue({"a", "b", "c"});  // reused for all records

    benchmark("Batch encoding of 10,000 Bolt records", [&] {
        batchBuf.Reset();
        for (u16 i = 0; i < 10'000; i++)
        {
            BoltValue lst = {
                mp("id", i+1),
                mp("score", (i+1) * 0.1),
                mp("tags", tags)
            };
            encoder.Encode(lst);
        } // end for
    }, iterations);

    // Optional: Benchmark decoding that batch
    BoltDecoder decoder(batchBuf);
    BoltValue out;
    benchmark("Batch decoding of 10,000 Bolt records", [&] {
        decoder.Decode(out);
        batchBuf.Reset_Read();
    }, iterations);

    // std::cout << out.Dump_Txt() << std::endl;
    return 0;
}
