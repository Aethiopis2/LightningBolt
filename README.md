# ⚡LightningBolt
A high-speed neo4j C++ driver over bolt protocol


# Internals
## ⚡ BoltBuf: Adaptive High-Performance I/O Buffer

`BoltBuf` is a zero-copy, hardware-aligned buffer used per connection in LightningBolt to manage reads and writes. It is tuned for:

- 🚀 High-throughput streaming
- 🎯 Cache-line alignment
- 🧠 Partial message safety
- 📈 Adaptive resizing
- 🧵 Lock-free usage in single-connection contexts

---

### ✅ Key Features

- **Cache-Aligned Memory**  
  Uses `posix_memalign()` to align memory to `CACHE_LINE_SIZE` (e.g., 64 bytes), improving cache performance.

- **Zero-Copy Streaming**  
  Exposes raw pointers via `Read_Ptr()` and `Write_Ptr()`, avoiding unnecessary memory copies.

- **Safe Read/Write Pointers**  
  Maintains `read_offset` and `write_offset` to track unprocessed vs. writable regions.

- **EMA-Based Traffic Monitoring**  
  Learns traffic patterns via Exponential Moving Average (EMA) and resizes accordingly.

- **Idle-Time Shrinking**  
  The buffer shrinks only when the driver is idle, ensuring no impact on active performance.

- **Tail Safety**  
  Reserves buffer space near the end (`TAIL_SIZE`) to prevent partial message overwrites.

---

### 📊 EMA-Based Resizing Logic

Internally, `BoltBuf` uses an exponential moving average to track recent I/O volumes:

```cpp
ema = alpha * bytes_this_cycle + (1 - alpha) * ema;




## 🔩 BoltPool - High-Performance Temporary Memory Pool for Bolt Protocol Decoding (still work in progress)

`BoltPool` is a high-speed, zero-allocation-on-fast-path memory pool designed for temporary allocation of Bolt protocol data structures (e.g., `BoltValue`, `List`, `Struct`, `Map`). It provides an efficient two-tiered memory allocation system optimized for minimal memory overhead, maximum locality, and rapid reuse across decoding cycles or queries.

## 🧱 Architecture Overview

BoltPool internally combines:

- **`ScratchBuffer<T, N>`** – a fixed-size, cache-aligned static buffer for ultra-fast allocation without heap usage.
- **`ArenaAllocator<T>`** – a dynamically growing heap-backed arena for larger or overflow allocations.
- **`BoltPool<T>`** – a hybrid allocator that transparently uses both layers and tracks allocations for rollback.

All allocations are recorded, enabling deterministic `Release()` behavior in reverse order, similar to a stack allocator.

## 📦 Components

### `ScratchBuffer<T, N>`
- Allocates from a fixed-size buffer aligned to 64 bytes (cache-line).
- Provides fast allocations with zero malloc overhead.
- Useful for small or frequent allocations.

### `ArenaAllocator<T>`
- Manages a heap-based dynamic memory pool.
- Grows automatically using `realloc()` when capacity is exceeded.
- Allocations are contiguous, reducing fragmentation.

### `BoltPool<T>`
- Tries to allocate from `ScratchBuffer` first, then overflows to `ArenaAllocator`.
- Maintains a log (`alloc_log`) of allocation sources for safe `Release()`.
- Allows memory reuse via `Reset_All()` between decoding sessions.

## 🛠️ Usage

```cpp
// Get a thread-local instance of the pool
auto& pool = GetBoltPool<BoltValue>();

// Allocate memory for 10 BoltValues
size_t offset = pool.Alloc(10);

// Resolve raw pointer from offset
BoltValue* values = pool.Resolve(offset);

// Use values...

// Release last 10 allocations (LIFO)
pool.Release(10);

// Or reset the entire pool
pool.Reset_All();