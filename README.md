# ⚡LightningBolt
A high-speed neo4j C++ driver over bolt protocol

Build (Linux):

mkdir bulid && cd build
cmake ..
make

Example direct query via async callbacks:
```cpp
#include "neodriver.h"
#include "bolt_result.h"

int main()
{
	NeoDriver driver(
		"bolt://localhost:7687",
		Auth::Basic("username", "password")
	);

	driver.Execute_Async(
    		"MATCH (n) AS n RETURN n LIMIT 25", 
    		[](BoltResult res) {
			for (auto r : res) std::cout << r.ToString() << "\n";
		}
	);
}
```

Running test samples (build directory):

./bin/encoder_decoder_test	# runs speed benchmark tests for cooked samples
./bin/streaming_batch_test	# mild tests on batched encoding/decoding speed benchmarks
./bin/connection_test		# aggressively tests the connection/disconnection test (stability test)
./bin/basic_query_test		# runs a speed benchmark test on cooked query samples in simulated synchronized mode
./bin/async_qbenchmark_test	# runs a speed benchmark test on cooked query over async threaded (pooled) mode
./bin/percentile_test		# runs a QPS and percentile tests as latency measures on a cooked query


Project Structure:
```
|- include
   |- bolt
      |- boltvalue.h		# definition of bolt pack stream wrappers
      |- boltvalue_pool.h	# pool for bolt values during encoding/decoding
      |- bolt_buf.h		# definition for adaptive buffer used for storage during sending and receiving
      |- bolt_decoder.h		# definition for bolt pack stream deserializer
      |- bolt_encoder.h		# definition for bolt pack stream serializer
      |- bolt_jump_table.h	# definition of bolt byte indexed jump table for deserializers
      |- bolt_message.h		# a light weight bolt message type definition, basically BoltValue with chunk size info
      |- bolt_result.h		# allows for iteration of bolt serialized messages from the recv buffer
      |- decoder_task.h		# definition of decoder tasks carring info on state and view into buffer
   |- connection
      |- tcp_client.h		# wrapper for posix socket functions + openssl 
      |- neoconnection.h	# defines a connection object with interfaces to Neo4j server
   |- utils
      |- errors.h		# prototypes of C style error handlers
      |- lock_free_queue.h	# definition for lock free queue template class
      |- red_stats.h		# extra utility functions used in measurements and all
      |- utils.h		# other utility functions developed over various times
   |- basics.h			# basic headers and few constants
   |- neocell.h			# wrapper to neoconnection
   |- neopool.h			# pool of neocells
   |- neodriver.h		# main driver module implementation
   |- neoerr.h			# contains definition of LBStatus 64-bit uint field used as return value by functions
|- src
   |- bolt
      |- bolt_decoder.cpp	# dummy file
      |- bolt_encoder.cpp	# dummy too
      |- bolt_jump_table.cpp	# implementation for tag based jump tables used for decoding/deseriliazation
   |- connection
      |- neoconnection.cpp	# implementation of connection class that implements all bolt functions
      |- tcp_client.cpp		# implementation of raw network functions such as send/recv and also ssl.
   |- test
      |- async_qbenchmark_test.cpp	# various tests used during development
      |- basic_query_test.cpp	#
      |- connection_test.cpp	#
      |- encoder_decoder_test.cpp	#
      |- percentile_test.cpp	#
      |- streaming_batch_test.cpp	#
   |- utils
      |- errors.cpp		# implementation for C style error handlers, that terminate app or dump error messages
      |- utils.cpp		# implementation for various utility functions used
   |- neocell.cpp		# implementation for NeoCell session object
   |- neodriver.cpp		# main driver module implementation
   |- neopool.cpp		# pool of NeoCell connections with load balancing implementation
   |- neoerr.cpp		# implementation of error handlers and display functions
```

Bolt Protocol:

Bolt is a binary protocol used by neo4j graph database. Its based on PackStream v1.0 specs, in which
a value is preceeded by a byte value (tag) which defines the type and size of data it encodes or serializes.
Example:
	the string, "a string" for instance is encoded/serialized in bolt as follows in network byte order
	or big-endian format (in hex):
		```88 61 20 73 74 72 69 6E 67```

	In which the first byte encodes both the type as string, 0x80 code, and its length, which is 8, followed
	by the character codes for each string.

Refer to bolt protocol for details, here.

Philosophy:

In a client server model, consider the moment at which the server sends its responses for a given query request over TCP/IP enabled network.
The response is copied over application supplied buffers as bolt serialized binary data, this is done during recv()
system call and by the kernel. What this implies is that as long as the recv buffer is guaranteed to remain unaltered during
the lifetime of the request, then responses can be deserialized on spot without the need for further allocations and copies.
It means by keeping pointers and markers to buffers we can stream and copy the required result on external buffers on demand.
Thus LightningBolt was born, everything else and all the other mechanics involved are meant to largely address this issue, 
henceforth its design. 
Its datatypes and abstractions are meant to ease encoding/serialization and/or decoding/deserialization of bolt protocol.
And because bolt uses compound types such as lists, and structs which are containers to more fundamental types, it uses pre-allocated
memory pool to represent these values naturally during encoding compound types. Decoding happens fully on the recv buffer.
The purpose of memory pool is to, 
	1. speed up parsing encoding times of bolt streams.
	2. avoid or minimize memory fragmentation and leaks
	3. represent bolt values in their natural format, i.e. Dictionaries and mutli-typed lists.
	4. minimize the number of allocations needed as a result of 3 above.
It also takes advantage of full-duplex sending and receiving bolt streams over TCP/IP, by keeping sending and receiving
buffers separate, i.e. sending (request) buffers are different from receiving (response) buffers. This generally avoids
locks and resource contentions that can arise during these stages at the expense of memory.
As the other or rather opportunistic goal was to come up with lock free architecture so as to maximize speed. 
It was also necessary to make the API's async using callback functions as the primary means of fetching responses 
as they arrive. This was made so in order to make the API more natural for web based applications. As this is going to power up
something I am cooking. Anyhow, a client can make requests and stream the results whenever they are ready without blocking
the client app. Although, its primary mechanics is async based, it also posses the capacity to make synchronous execute()...fetch()
calls. Console based applications mostly benefit from such features and the synchronous feature was result of need to test the 
driver functions in the console.
Since LightningBolt uses zero-copy strategy, it defines a minimum buffer requirement during receive and clear boundary of ownership
of decoded bolt values. Zero copy because it decodes values in buffer, however it does copy ints, floats, nulls and bools because 
in 64-bit machines a pointer is 64-bits anyways and these values have sizes <= 64 in bolt format. Strings, maps/dictionaries, byte
arrays, and structs are never copied and are marked with pointers/offsets (see BoltValue for details) to the receive buffer. And
since according to bolt protocol specs, a message boundary during responses can never exceed 64k (the 2 byte header at the beginning)
the minimum receiving buffer size is 64K + 4 bytes (2 byte header + 2 byte trailing zeros if they exist) = 65,540 bytes. For encoding I have
rarely cared as its simply same as decoder size at initialization and because feedback loops can adjust it to an optimal size (see 
BoltBuf for details).

Architecture:

To meet the above philosophical and real life constraints, I have approached the whole thing in a cellular manner. I.e. much
like biological cells. A cell can function in isolation, thus a NeoConnection object wrapped around NeoCell object should be
capable of sustaining itself. Thus a cell, or NeoCell keeps its own connection, which in turn keeps its own separate sending and
receiving buffers. This allows for lock free architecture internally as the number of shared states is minimal. Thus sending and
receiving can happen on their own threads without getting in each other's hair, and NeoCell makes that possible. Keeping a bunch
of NeoCell objects in a pool, NeoPool gives me the ability to maintain concurrency and again lock free approach as each NeoCell
object is essentially on its own. Thus two NeoCell objects could concurrently be performing decoding and encoding without the 
need to wait on one another. Internally each NeoCell keeps a queue of requests and responses, this allows each cell to process
more requests within one connection. Ofcourse, the downside is memory sacrificed but is somewhat mildly compensated with by 
minimizing allocations and leaks and keeping the cache hot and more predictable. All that means is that on multicore systems,
which are pretty much the norm these days, LightningBolt will execute requests/results in parallel on most days, and thus
should naturally be fast, hence the name LightningBolt.

Internals:

I'm going to do this in the way the process evolved beginning with the first objects I've tackled that aid in bolt packet
serialization/deserialization which I simply dubbed `BoltEncoder` and `BoltDecoder`, then followed by a connection object
`NeoConnection` and thus its cellular wrapper `NeoCell` which bakes it async model and acts like a session contained inside
a pool object, `NeoPool` for fast access and concurrency managed by the main driver object, `NeoDriver` which allows users
to run queries and do other stuff such as obtain pointers to `NeoCell`s for explicit control.

1. BoltValue

Bolt is optimized for memory use but its packed in such ways that makes access to its contents from the perspective of a 
C/C++ program a little strange. Nonetheless built on strict rules thus manageable. As the bolt spec states, every bolt content
or value begins with a byte tag that explains what the next byte is. Sometimes the byte itself is the value as in tiny integers
>= -16 and <= 16. Hence, one could easily copy the intended values by simply looking at/decoding the tags. And this is what
the LightningBolt does, but it only copies certain types of values such as integers and floats and it offsets strings, maps,
byte arrays, structures, and lists into the buffer which received the bolt packets when decoding/deserializing values. 
On the encoding end, bolt serialization or encoding is a matter of copying standard C/C++ objects into a buffer with certain
types of tags or bytes appended in a network byte order. It means based on type of object sent, bolt inserts specific kinds
of tags at the beginning of the binary data that define the type and length of the data encoded. 
The BoltValue was an abstraction designed to meet these goals. It does copy simple types such as `bools` and `ints` and it offsets
others into the receiving buffer. And In LightningBolt `byte arrays`, `lists`, `maps`, `strings`, and `structs` are but container objects
that contain other basic types such as `nulls`, `ints`, `floats`, and `bools`. During decoding, complex types may be chunked
to form a single result set, to iterate over those we only need to keep the starting offset into the buffer containing 
the serialized bolt object.

As shown bolt values are divided into two broad groups based on how they use the pool
	1. Baic types: require almost no pool support when encoding since only pointer arithmetic is involved and/or
	 are easily copied these groups include:
		-> Null
		-> Boolean
		-> Integer
                -> Float
		-> String
		-> Byte array

	2. Compound types: require external pool support to hold values as these are containers of basic types
	these are:
		-> List
		-> Maps/Dictionary
		-> Struct

* Note: the message is a special struct class with only chunk size prepended and optionally padding zeros appended.
	special types such as nodes, relationships, etc are treated as structs with different tag values in LB.

Types are then implemented as a union objects that take on the role of the various types supported in bolt along side
the type field definition. The following excerpt from `include/bolt/bolt_value.h` shows the definition of BoltValue:
```cpp
	union 
	{
    		s64 int_val;
    		double float_val;
    		bool bool_val;
  
    		struct 
    		{
        		union
        		{
				size_t offset;      // during decoding
				const char* str;    // during encoding
        		};
        		size_t length;
    		} str_val;

	...
	}
```

the chocie to store offsets instead of direct pointers was because I needed to avoid "dangling pointer" errors that occur during buffer changes 
such as when growing and shrinking (see adaptive buffer). If a buffer grows mid during point decoding, an OS may relocate
the buffer to a newer address causing the old pointer to dangle, thus no direct pointers.
Encoding too has its own tricks, since LB wanted to support what I like to call "bolt natural syntax", as the following example:
```cpp
	BoltMessage msg( BoltValue(
    		0x00, {"Hit", "the", "road", "jack",
    		BoltValue({
        		{1, "Hi", 3, true, 512 }, 
        		{"So I've changed this string from what it was, care to know what?", "nested?", 678984, false},
        		{"five", 35}, 
        		{"true", {1, 2, true, false, "another nested text", 3.14567889} }  
    		}), {
            	mp("statement", "MATCH (n:Person)-[:KNOWS]->(m:Person) "
                            "WHERE n.name = $name AND m.age > $minAge "
                            "RETURN m.name, m.age, m.location ORDER BY m.age DESC"),
            	mp("parameters", BoltValue({
                	mp("name", "Alice"),
                	mp("minAge", 30),
                	mp("includeDetails", true),
                	mp("filters", BoltValue({
                    		mp("location", "Europe"),
                    		mp("interests", BoltValue({
                        		"hiking", "reading", "travel"
                    		}))
                	}))
            	}))
        	}
	}
	) );
```
You get the idea! And such syntax requires memory. And the memory is supplied from the pool. Normally in C++ such inline
initializations disappear the moment the line ends (well except for certain static elements such as c-strings and all). 
Hence, I'll need to keep these values via the constructor. And I didn't want to use heap allocated memory as that 
contaminates things at this point, thus a pre-allocated pool for reasons stated above and boost performance thru cache localization.
So now a decoder/encoder class can take these BoltValues as parameters as a means of generic formats/specs for intercoms.
Besides that, the structure or type BoltValue defines type field which is an enum constant defined within the same file. It allows
for distinguishing different bolt types. 
Provides access to underlying elements through direct invocation of union elements or through the use of ToString() method,
which returns a string formatted version of bolt. This is useful during web transmission as string data.

2. BoltEncoder

This object is defined in include/bolt/bolt_encoder.h, and its purpose is to serialize data according to bolt specs. The class
provides only a single interface, Enocde(), which is a template method taking on various types. The class also takes a reference
to a `BoltBuf` during initialization. Therefore all encodings/serializations happen into the reference buffer according to bolt
specs. Since the types serialized are but ordinary C/C++ types with added info, we can take advantage of `constexpr` to define 
a highly predictable routine for each, thus much of the hustle is resolved during compile time. As stated above, the encoder supports
encoding of nulls, bools, ints, floats, strings, byte arrays, lists, maps, structs, and messages. A bolt message is a container
to a BoltValue of type struct with preceeding 16-bit chunk size and optionally appended with 16-bit padding. I have added bolt type
of Unknown for clarity and testing of null values. So how well does this encoder perform, below is a sample benchmark for encoding
all the supported types. The example is found at `src/test/encoder_decoder_test.cpp`.
```
Encoding Benchmark (1000000 iterations)
-----------------------------------------------
Operation             Avg Time (ns)     Throughput (ops/sec)
-----------------------------------------------
Null                         0.9348              1.06975e+09
Bool                         0.9294              1.07596e+09
Integer                      2.0676              4.83653e+08
Float                        1.7268              5.79106e+08
String                       5.9917              1.66898e+08
Bytes                        7.0118              1.42617e+08
List                        129.694              7.71048e+06
Map                         138.448               7.2229e+06
Struct                      241.898              4.13398e+06
Message                     318.162              3.14305e+06
```
*Note: Compound types such as maps and structs were tested on deeply nested structures as the above
	BoltValue snap shows in section BoltValue.

3. BoltDecoder

The object defined in `include/bolt/bolt_decoder.h`, and its helper definition `include/bolt/bolt_jump_table.h` and implementation at
`src/bolt/bolt_jump_table.cpp`. Bolt is tag based, and because each tag mean something, its possible to prepare a jump table of functions
that handles the correct decoding. Since a tag is a byte value, we only need 255 handlers for all bolt types. The decoder uses `jump_table`
array defined in `include/bolt/bolt_jump_table.h` to decode each type according to the tags received. Thus decoder object provides a single
overloaded function, `Decode`, which returns decoded values as parameter. The most useful and practical of the `Decode` functions is 
`BoltDecoder::Decode(u8*, BoltMessage&);` which returns a message type from the provided buffer's relative start and returns the number of
bytes consumed by the decoder. The reason for splitting the decoder with its jump table definitions is having to do circular references in headers
viz BoltValue, which happens also to require decoding functionality when converting BoltValue's to strings. In a sense LightningBolt decodes
twice, once during recv and the intial decode, and second when fetching compound types such as lists, maps, and structs.
Below is decoder benchmarks for all types supported, from `src/test/encoder_decoder_test.cpp`.
```
Decoding Benchmark (1000000 iterations)
-----------------------------------------------
Operation             Avg Time (ns)     Throughput (ops/sec)
-----------------------------------------------
Null                         3.5656              2.80458e+08
Bool                         3.2877              3.04164e+08
Integer                      2.9936              3.34046e+08
Float                        3.9054              2.56056e+08
String                        3.451              2.89771e+08
Bytes                        3.9335              2.54227e+08
List                        72.9345              1.37109e+07
Map                         56.1464              1.78106e+07
Struct                      134.255              7.44851e+06
Message                     135.823               7.3625e+06
```

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

6. BoltBuf:

y[n]=αy[n−1]+(1−α)⋅CurrentD
discrete-time leaky integrator / exponential moving average (EMA).

Features:
=> Cache-aligned storage
=> Prefetch hints
=> Tail protection
=> Emergency correctness grow
=> EMA-based adaptive trend grow
=> Hysteresis-based shrink
=> Oscillation resistance

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

4. Error Handling
LB_Status format 64-bit:

63                                                               0
+--------+--------+--------+--------+----------------------------+
| Action | Domain | Stage  | Code   | Aux / Payload               |
+--------+--------+--------+--------+----------------------------+
 8 bits    8 bits   8 bits   8 bits          32 bits