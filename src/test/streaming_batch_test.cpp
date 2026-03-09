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
    streamOps.reserve(10);
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
    const u16 size = 10'000;
    BoltValue bgList = BoltValue::Make_List();
    for (u16 i = 0; i < size; i++)
    {
        bgList.Insert_List({
            mp("id", i + 1),
            mp("score", (i + 1) * 0.1),
            mp("tags", BoltValue({i, i + 1, i + 2}))
            });
    } // end for

    // Pre-allocate reusable buffer for batch encoding
    BoltBuf batchBuf(8 * 1024 * 1024);  // 8MB
    iZero(batchBuf.Data(), 8*1024*1024);
    BoltEncoder encoder(batchBuf);

    benchmark("Batch encoding of 10,000 Bolt records", [&] {
        batchBuf.Reset();
        encoder.Encode(bgList);
    }, iterations);

    // Optional: Benchmark decoding that batch
    BoltDecoder decoder(batchBuf);
    BoltValue out;
    benchmark("Batch decoding of 10,000 Bolt records", [&] {
        decoder.Decode(out);
        batchBuf.Reset_Read();
    }, iterations);


    // === 4. Fun: Batch Maps of 1,000 items ===
    const u16 size2 = 1'000;
    BoltValue bgMap = BoltValue::Make_Map();
    for (u16 i = 0; i < size2; i++)
    {
        bgMap.Insert_Map("TheKey", BoltValue({
            mp("id", BoltValue({i + 1, i + 2, i + 3}))
            }));
    } // end for

	// clear previous buffer
    iZero(batchBuf.Data(), 8 * 1024 * 1024);
    benchmark("Batch encoding of 1,000 Bolt map/dictionary records", [&] {
        batchBuf.Reset();
        encoder.Encode(bgMap);
        }, iterations);
	
    return 0;
}
