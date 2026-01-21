/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 2.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 21st of January 2026, Wednesday
 */
#pragma once





//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"
#include <openssl/ssl.h>   // Correct header for OpenSSL SSL functionality
#include <openssl/err.h>




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


// return codes for TcpClient methods
#define TCP_SUCCESS        0               /* success */
#define TCP_ERROR         -1               /* generic system call error */
#define TCP_TRYAGAIN      -4               /* try again later */
#define TCP_ERROR_SSL     -5               /* ssl related error */
#define TCP_ERROR_CLOSED  -6               /* connection closed by peer */



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

    TcpClient();      // default
    TcpClient(const std::string &host, const std::string &port, bool ssl = false);
    virtual ~TcpClient();

    virtual bool Is_Closed() const = 0;

    // commons 
    int Connect();
    int Init_SSL();
    int SSL_Connect();

    int Send(const void* buf, size_t len);
    int Recv(void* buf, size_t len);

    struct pollfd Get_Pollfd() const;
    int Get_Socket() const;
    int Set_NonBlock();

    void Disconnect();
    void Shutdown_SSL();
	void Enable_SSL();

protected:

    int fd;                         // a socket descriptor
	bool ssl_enabled;               // ssl flag
    std::string hostname, port;     // the host and port num
	std::string str_err;            // holds the last error string
    struct addrinfo *paddr;         // holds the remote address info

    // ssl stuff
	SSL_CTX* ctx;   // ssl context
    SSL* ssl;       // ssl object

private:

	// function pointers for polymorphic behavior
    using Send_Fn_Ptr = int (TcpClient::*)(const void*, size_t);
	using Recv_Fn_Ptr = int (TcpClient::*)(void*, size_t);
    
	Send_Fn_Ptr pSend;
	Recv_Fn_Ptr pRecv;

    // utils
    int Fill_Addr();

    int Send_Tcp(const void* buf, size_t len);
	int Recv_Tcp(void* buf, size_t len);
    int Send_Tcp_NonBlock(const void* buf, size_t len);
	int Recv_Tcp_NonBlock(void* buf, size_t len);
	int SSL_Send(const void* buf, size_t len);
	int SSL_Recv(void* buf, size_t len);
};