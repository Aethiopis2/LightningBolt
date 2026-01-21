/**
 * @author Rediet Worku aka Aethiops II ben Zahab (aethiopis2rises@gmail.com)
 * 
 * @version 1.2
 * @date created 1st of December 2023, Friday
 * @date updated 18th of January 2026, Sunday
 */
#ifndef REDBASICS_H
#define REDBASICS_H



#if defined(WIN32) || defined(_WIN64)
#define WINDOWS
#endif




//=====================================================================================|
//          INCLUDES
//=====================================================================================|
#include <iostream>             // C++ headers
#include <iomanip>
#include <cstdint>
#include <cctype>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <list>
#include <queue>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <ctime>
#include <chrono>
#include <mutex>
#include <array>
#include <functional>
#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <optional>
#include <bit>
#include <span>


#include <sys/types.h>          // C headers
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>


#if defined(WINDOWS)
#include <WinSock2.h>
#include <ws2tcpip.h>

#define CLOSE(s)                closesocket(s)
#define POLL(ps, len, wait_sec) WSAPoll(ps, len, wait_sec)

#else
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>


#define CLOSE(s)                close(s)
#define POLL(ps, len, wait_sec) poll(ps, len, wait_sec)
#endif 



#if defined(_MSC_VER)
    #include <stdlib.h>
    #define BSWAP64(x) _byteswap_uint64(x)
#elif defined(__GNUC__) || defined(__clang__)
    #define BSWAP64(x) __builtin_bswap64(x)
#else
    // Generic portable fallback
    inline uint64_t BSWAP64(uint64_t x) {
        return ((x & 0xFF00000000000000ull) >> 56) |
               ((x & 0x00FF000000000000ull) >> 40) |
               ((x & 0x0000FF0000000000ull) >> 24) |
               ((x & 0x000000FF00000000ull) >> 8)  |
               ((x & 0x00000000FF000000ull) << 8)  |
               ((x & 0x0000000000FF0000ull) << 24) |
               ((x & 0x000000000000FF00ull) << 40) |
               ((x & 0x00000000000000FFull) << 56);
    }
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || defined(_WIN32)

    inline uint64_t htonll(uint64_t x) {
        return BSWAP64(x);
    }

    inline uint64_t ntohll(uint64_t x) {
        return BSWAP64(x);
    }

#else  // Big endian

    inline uint64_t htonll(uint64_t x) {
        return x;
    }

    inline uint64_t ntohll(uint64_t x) {
        return x;
    }

#endif


//=====================================================================================|
//          TYPES
//=====================================================================================|
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef __int128_t s128;




//=====================================================================================|
//          DEFINES
//=====================================================================================|
#define APP_NAME        "⚡LightningBolt"      // Application name



#define iZero(buf, len) memset(buf, 0, len)
#define iCpy(d, s, l)   memcpy(d, s, l)  //fast_memcpy64(d, s, l) //memblast::Memblast(d, s, l)



#define MAXLINE         4096                /* default size of buffer used during intranetwork buf. */



enum Boolean {FALSE, TRUE};


#ifndef MAXPATH
#define MAXPATH     260     /* dir max length in Windows systems */
#endif


#if defined(WINDOWS)
#define PATH_SEP        '\\'
#else
#define PATH_SEP        '/'
#endif


#define DUMP_TIME char buf[MAXPATH]; time_t curr_time; std::time(&curr_time); \
    char b[MAXPATH]; snprintf(b, MAXPATH, "\033[33m%s\033[37m", APP_NAME); \
    std::strftime(buf, MAXPATH, strcat(b," \033[34m%d-%b-%y, %T\033[37m"), localtime(&curr_time)); \
    std::cout << buf << ": "; 


#define LOCK_GUARD(m)   std::lock_guard<std::mutex> lock(m)


constexpr bool is_big_endian =
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    true;
#else
    false;
#endif



#define SWAP_ENDIAN_DOUBLE(val) {uint8_t *p = (uint8_t*)&val; std::reverse(p, p+sizeof(double)); val;}

// generic byte swap helpers
template<typename T> inline T byte_swap(T val) 
{ 
    if constexpr (sizeof(T) == 1) return val; // no swap needed for 1 byte
    else if constexpr (sizeof(T) == 2) return ntohs(val);
    else if constexpr (sizeof(T) == 4) return ntohl(val);
    else if constexpr (sizeof(T) == 8) return ntohll(val);
    else static_assert(sizeof(T) == 0, "Unsupported type size for byte swap");
} // end byte_swap
template<> inline double byte_swap(double v) { return ntohll(v); }

inline double swap_endian_double(double value) {
    uint8_t* p = reinterpret_cast<uint8_t*>(&value);
    std::reverse(p, p + sizeof(double));
    return value;
}


#endif