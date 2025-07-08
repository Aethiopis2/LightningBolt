/**
 * @file connection.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief An abstract class for connection objects using TCP/IP stack
 * @version 1.0
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once





//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"





//===============================================================================|
//          MACROS
//===============================================================================|
// this macro get's defined for the platform at hand
#if defined(__unix__) || defined(__linux__)
#define CLOSE(s)    close(s)
#else 
#if defined(WIN32) || defined(__WIN64)
#define CLOSE(s)    closesocket(s);
#endif
#endif


#define SERV_PORT       7777                /* default pre-kooked server port */
#define LISTENQ         32                  /* default max number of listening descriptors */
#define SA              struct sockaddr     /* short hand notation for socket address structures */




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief a declartion of basic tcp client object. As an abstract class this class
 *  implements only the bare bones of a tcp client object so as to avoid code 
 *  duplication and have a cleaner interface.
 */
class TcpClient
{
public:

    TcpClient() {}      // default
    TcpClient(const std::string &host, const std::string &port);
    virtual ~TcpClient();

    // virtual int Handle_Event(s16 revents) = 0;
    virtual bool Is_Closed() const = 0;

    // commons 
    int Connect();
    void Disconnect();


    /**
     * @brief Wraps around send system call
     * 
     * @param buf space containing the bytes to send to peer
     * @param len length of the buffer above
     * 
     * @return int number of bytes actually read
     */
    ssize_t Send(const void *buf, size_t len) 
    { 
        return send(fd, buf, len, 0); 
    } // end Send



    /**
     * @brief Wraps around recv or whichever is better system call
     * 
     * @param buf space to store the received bytes
     * @param len length of the buffer above
     * 
     * @return int number of bytes actually read
     */
    ssize_t Recv(void *buf, size_t len) 
    { 
        return recv(fd, buf, len, 0); 
    } // end Recv



    /**
     * @brief return's the socket descriptor as pollfd struct with required
     *  event as POLLIN, I don't care about POLLOUT or others.
     */
    constexpr struct pollfd Get_Pollfd() const 
    {
        return { fd, POLLIN, 0};
    } // end Get_Pollfd



    /**
     * @brief returns the socket descriptor
     */
    constexpr int Get_Socket() const 
    {
        return fd;
    } // end get_socket


    /**
     * @brief toggles socket to either blocking or non blocking mode on every call
     */
    int Toggle_NonBlock()
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (fcntl(fd, F_SETFL, flags ^ O_NONBLOCK) < 0)
            return -1;
        
        return 0;
    } // end Toggle_NonBlock


protected:

    int fd;                         // a socket descriptor
    std::string hostname, port;     // the host and port num
    struct addrinfo *paddr;         // holds the remote address info

    int Fill_Addr();
};