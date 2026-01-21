# ⚡LightningBolt
A high-speed neo4j C++ driver over bolt protocol


# Internals
## TcpClient

### Minimal High-Performance TCP / TLS Client

A minimal, low-level **TCP / TLS client abstraction** designed for performance-critical systems.

This class provides the *bare bones* required to establish TCP or TLS connections, send and receive data, and integrate cleanly with `poll()` / event-driven architectures — **without virtual dispatch, hidden allocations, or framework overhead**.

It is intended as an infrastructure building block (e.g. database drivers, protocol engines, async runtimes), not a batteries-included networking framework.

---

### Design Goals

* **Zero abstraction tax** in hot paths
* **Explicit lifecycle control** (connect, handshake, shutdown)
* **No virtual send/recv overhead**
* **Works with `poll()` / epoll-style event loops**
* **Optional TLS (OpenSSL)** with thread-safe global initialization
* **Blocking and non-blocking I/O** with identical interface

This class intentionally avoids:

* RAII wrappers around sockets or SSL
* Background threads
* Automatic retries or buffering
* Implicit state machines

Higher layers are expected to orchestrate behavior.

---

### Key Features

#### 1. Function-Pointer I/O Dispatch

Instead of virtual methods, `TcpClient` uses **member function pointers**:

* `Send()` and `Recv()` dispatch directly to:

  * TCP blocking
  * TCP non-blocking
  * TLS (`SSL_write` / `SSL_read`)

This avoids vtable lookups and keeps the hot path predictable.

---

#### 2. Optional TLS (OpenSSL)

TLS support is optional and explicit:

* TLS is enabled via constructor or `Enable_SSL()` (pre-connect)
* Handshake is performed after TCP connect
* All OpenSSL global state is initialized **once per process** using `std::call_once`

```cpp
static std::once_flag ssl_init_once;
std::call_once(ssl_init_once, []() noexcept {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
});
```

This guarantees:

* Thread-safe initialization
* Zero overhead after first call

---

#### 3. `poll()`-Friendly API

The client is designed to integrate cleanly with event loops:

```cpp
pollfd pfd = client.Get_Pollfd();
```

* Only `POLLIN` is requested
* Write readiness is controlled by the caller
* No internal blocking loops

This matches architectures where reads are kernel-driven and writes are demand-driven.

---

#### 4. Blocking and Non-Blocking Modes

The socket can be switched to non-blocking mode at runtime:

```cpp
client.Set_NonBlock();
```

Behavioral differences are handled internally by switching function pointers.

Return values distinguish:

* Success
* Try-again (`EAGAIN`, `EWOULDBLOCK`, `SSL_ERROR_WANT_*`)
* Peer closed
* Fatal error

No exceptions are used.

---

### Public Interface Overview

```cpp
class TcpClient
{
public:
    TcpClient();
    TcpClient(const std::string& host,
              const std::string& port,
              bool ssl = false);

    virtual ~TcpClient();

    virtual bool Is_Closed() const = 0;

    int Connect();
    int Send(const void* buf, size_t len);
    int Recv(void* buf, size_t len);

    pollfd Get_Pollfd() const;
    int Get_Socket() const;

    int Set_NonBlock();
    void Disconnect();

    void Enable_SSL();
};
```

---

### Typical Usage

#### TCP (Blocking)

```cpp
TcpClient client("localhost", "7687");
if (client.Connect() == TCP_SUCCESS)
{
    client.Send(data, size);
    client.Recv(buf, sizeof(buf));
}
```

---

#### TCP (Non-Blocking + poll)

```cpp
client.Set_NonBlock();

pollfd pfd = client.Get_Pollfd();
poll(&pfd, 1, timeout);

if (pfd.revents & POLLIN)
    client.Recv(buf, sizeof(buf));
```

---

#### TLS

```cpp
TcpClient client("db.example.com", "7687", true);
client.Connect();
```

TLS handshake occurs immediately after TCP connect.

---

### Error Model

All I/O functions return `int`:

| Return value       | Meaning                           |
| ------------------ | --------------------------------- |
| > 0                | Bytes transferred                 |
| `TCP_TRYAGAIN`     | Retry later (`EAGAIN` / SSL WANT) |
| `TCP_ERROR_CLOSED` | Peer closed connection            |
| `TCP_ERROR`        | Fatal TCP error                   |
| `TCP_ERROR_SSL`    | Fatal TLS error                   |

The class does **not** retry automatically.

---

### Threading Model

* The class itself is **not synchronized**
* One `TcpClient` instance should be owned by one thread at a time
* OpenSSL global initialization is thread-safe

This design avoids hidden locks in the hot path.

---

### Non-Goals

This class intentionally does **not** provide:

* Async futures / promises
* Internal buffers
* Automatic reconnects
* Protocol framing
* Connection pooling

Those concerns belong in higher layers.

---

### Intended Use Cases

* High-performance database drivers (e.g. Bolt / Neo4j)
* Binary protocol engines
* Event-driven network clients
* Systems where latency and predictability matter more than convenience

---

### Philosophy

> *Make the fast path obvious, explicit, and cheap.*

`TcpClient` exposes exactly what the OS and TLS stack provide — no more, no less — and lets higher layers build meaning on top.

If you need a framework, use one.
If you need control, this is the right layer.

---

### License

Use freely. Attribution appreciated.

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