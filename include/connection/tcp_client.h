/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 2.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 10th of Feburary 2026, Tuesday
 */
#pragma once





//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "neoerr.h"
#include <openssl/ssl.h>  
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
    TcpClient(const std::string &host, const std::string &port, 
        bool ssl = false);
    virtual ~TcpClient();

    int Get_Socket() const;

    bool Is_Open() const;
    bool Enable_Keepalive(const int idle_sec = 5,
        const int interval_sec = 2,
        const int count = 5);

    void Enable_SSL();
    void Enable_NonBlock();
    void Disconnect();

    LBStatus Connect();
    LBStatus Send(const void* buf, const int len);
    LBStatus Recv(void* buf, const int len);

protected:

    int fd;                         // a socket descriptor
    bool is_open;                   // connection flag
	bool ssl_enabled;               // ssl flag
    std::string hostname, port;     // the host and port num
    struct addrinfo *paddr;         // holds the remote address info

    // ssl stuff
	SSL_CTX* ctx;   // ssl context
    SSL* ssl;       // ssl object

private:

	// function pointers for polymorphic behavior
    using Send_Fn_Ptr = LBStatus (TcpClient::*)(const void*, const int);
	using Recv_Fn_Ptr = LBStatus (TcpClient::*)(void*, const int);
    
	Send_Fn_Ptr pSend;
	Recv_Fn_Ptr pRecv;

    // utils
    void Shutdown_SSL();

    LBStatus Fill_Addr();
    LBStatus Init_SSL();
    LBStatus SSL_Connect();

    LBStatus Send_Tcp(const void* buf, const int len);
	LBStatus Recv_Tcp(void* buf, const int len);
    LBStatus Send_Tcp_NonBlock(const void* buf, const int len);
	LBStatus Recv_Tcp_NonBlock(void* buf, const int len);
	LBStatus SSL_Send(const void* buf, const int len);
	LBStatus SSL_Recv(void* buf, const int len);
};