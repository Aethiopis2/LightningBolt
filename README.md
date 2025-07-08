# ⚡LightningBolt
A high-speed neo4j C++ driver over bolt protocol


# Internals
## BoltBuf: Adaptive High-Performance I/O Buffer

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
```
Where:
    alpha = 0.2
    ema is the exponentially weighted average of recent traffic

**Resize Conditions:**
**Condition	                      Action**
ema > capacity * 0.8	            🔼 Grow
ema < capacity * 0.8 (when idle)	🔽 Shrink

**🔧 Typical Usage**
**Receiving Data**
```cpp
// Receive data directly into BoltBuf
ssize_t n = recv(fd, buf.Write_Ptr(), buf.Writable_Size(), 0);
if (n > 0) {
    buf.Advance(n);
    buf.Update_Stat(n);  // Feed into EMA
    buf.Grow();          // Conditionally grow if high traffic
}
```
**Decoding Loop**
```cpp
// Decode messages in sequence
while (Has_Complete_Message(buf)) {
    Decode(buf.Read_Ptr());
    buf.Consume(message_length);
}
```
**Shrinking on Idle**
```cpp
if (connection->Is_Idle())
    buf.Shrink();  // Only shrink when safe
```
**Appending Bolt-Encoded Buffers**
```cpp
BoltBuf output_buf;
BoltBuf partial_encoded = Encoder::Encode(my_value);

output_buf.Append(partial_encoded);  // Efficient copy
```

**📐 Memory Layout Overview**
```
[data ......................] → entire capacity
 ^            ^             ^
 |            |             |
read_offset  write_offset  capacity
```
[data between read_offset and write_offset] → ready for decode  
[write_offset to capacity - TAIL_SIZE]      → safe to write  
[capacity - TAIL_SIZE to capacity]          → tail buffer guard  

**🧱 Design Principles**

    - One buffer per connection
    - No need for locks or atomic ops
    - Write-ahead friendly for pipelined query mode
    - Compatible with SIMD-accelerated decode
    - Ideal for high-frequency recv-decode cycles
    - Defensive bounds logic prevents message overflow or fragmentation

**📎 API Summary**
**Method**	          **Purpose**
Read_Ptr()	      Pointer to data ready for decoding
Write_Ptr()	      Pointer to space ready for writing
Advance(n)	      Advance write head after recv()
Consume(n)	      Advance read head after decoding
Grow()	          Doubles buffer if traffic is high
Shrink()	        Halves buffer if traffic drops and idle
Update_Stat(n)	  Updates EMA using current recv size
Writable_Size()	  Returns space left before end of buffer
Size()	          Returns amount of unread data
Empty()	          Returns true if buffer has no data
Append(buf)	      Appends another BoltBuf's content
Reset()	          Resets both read and write offsets to 0
Reset_Read()	    Resets only the read offset to 0

**🚦 Internal Notes**

    - Grow() respects TAIL_SIZE by ensuring enough slack even after expansion
    - Shrink() happens only when the connection is idle and the buffer has been consumed
    - EMA and buffer behavior controlled by an internal BufferStats structure

**📁 Source**

    Implementation: src/core/bolt_buf.hpp
    
**🔗 Best Used With**

    - Pipelined query dispatch logic
    - Thread-per-connection decoder workers
    - SIMD-accelerated decoding pipelines


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
```