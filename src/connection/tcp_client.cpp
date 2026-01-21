/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (Aethiopis2rises@gmail.com)
 * 
 * @version 2.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 21st of January 2026, Wednesday
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "connection/tcp_client.h"
#include "utils/errors.h"





//===============================================================================|
//          GLOBALS
//===============================================================================|





//===============================================================================|
//          CLASS IMP
//===============================================================================|
/**
 * @brief Construct a new Tcp Client:: Tcp Client object Initalizes the object
 *  in a disconnected state.
 */
TcpClient::TcpClient()
	: fd{ -1 }, hostname{ "" }, port{ "" }, paddr{ nullptr }, ssl_enabled{ false }
{
    pSend = &TcpClient::Send_Tcp;
	pRecv = &TcpClient::Recv_Tcp;
} // end default constructor


/**
 * @brief Construct a new Tcp Client:: Tcp Client object Initalizes the object
 *  in a connected state.
 * 
 * @param hostname the host name or ip address to connect to
 * @param port the corresponding port number
 */
TcpClient::TcpClient(const std::string &host, const std::string &nu_port, bool ssl)
	: fd{ -1 }, hostname{ host }, port{ nu_port }, paddr{ nullptr }, ssl_enabled{ ssl }
{
    if (ssl_enabled)
    {
        pSend = &TcpClient::SSL_Send;
        pRecv = &TcpClient::SSL_Recv;
    } // end if ssl
    else
    {
        pSend = &TcpClient::Send_Tcp;
        pRecv = &TcpClient::Recv_Tcp;
	} // end else non-ssl
} // end constructor II


/**
 * @brief Destroy the Tcp Client:: Tcp Client object Closes the socket if already
 *  active.
 */
TcpClient::~TcpClient()
{
    Disconnect();
} // end Destructor


/**
 * @brief Connects to server/host at the provided address using the best protocol 
 *  the kernel can determine.
 * 
 * @return int a 0 on success, -1 on fail.
 */
int TcpClient::Connect()
{
    int rc;
    if (ssl_enabled)
    {
        if (Init_SSL() < 0)
            return TCP_ERROR_SSL;
	} // end if ssl

    // initalize the base addr info first
    if (Fill_Addr() == -1)
        return TCP_ERROR;  

    struct addrinfo *p_alias = paddr;
    do {

        fd = socket(p_alias->ai_family, p_alias->ai_socktype, p_alias->ai_protocol);
        if (fd < 0)
            continue;

        if (::connect(fd, p_alias->ai_addr, p_alias->ai_addrlen) == 0)
        {
            if (ssl_enabled)
            {
                if (SSL_Connect() == -1)
                {
                    CLOSE(fd);
                    fd = -1;
                    continue;   // try next addr
                } // end if ssl error
            } // end if ssl

            return TCP_SUCCESS;
        } // end if connect ok

    } while ((p_alias = p_alias->ai_next) != NULL);

    // at the end of the day if socket is null
    return TCP_ERROR;
} // end Connect


/**
 * @brief Initalizes SSL library and creates context and ssl object.
 *
 * @return int 0 for success -1 for error
 */
int TcpClient::Init_SSL()
{
#include <mutex>

    static std::once_flag ssl_init_once;
    std::call_once(ssl_init_once, []() noexcept {
        SSL_library_init();
        SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
    });

    const SSL_METHOD* method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    if (!ctx)
    {
		str_err = "SSL_CTX_new() failed";
        return TCP_ERROR_SSL;
    } // end if ctx

    ssl = SSL_new(ctx);
    return TCP_SUCCESS;
} // end Init_SSL


/**
 * @brief performs ssl handshake with the peer after connecting tcp socket
 *
 * @return int 0 for success -1 for error
 */
int TcpClient::SSL_Connect()
{
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) <= 0)
    {
		str_err = "SSL_connect() failed";
        return TCP_ERROR_SSL;
    } // end if error

    return TCP_SUCCESS;
} // end SSL_Connect


/**
 * @brief Wraps around send system call
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read
 */
int TcpClient::Send(const void* buf, size_t len)
{
	return (this->*pSend)(buf, len);
} // end Send


/**
 * @brief Wraps around recv or whichever is better system call
 *
 * @param buf space to store the received bytes
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read
 */
int TcpClient::Recv(void* buf, size_t len)
{
	return (this->*pRecv)(buf, len);
} // end Recv


/**
 * @brief return's the socket descriptor as pollfd struct with required
 *  event as POLLIN, I don't care about POLLOUT or others.
 */
struct pollfd TcpClient::Get_Pollfd() const
{
    return { fd, POLLIN, 0 };
} // end Get_Pollfd


/**
 * @brief returns the socket descriptor
 */
int TcpClient::Get_Socket() const
{
    return fd;
} // end get_socket


/**
 * @brief Sets the socket to a non-blocking mode
 */
int TcpClient::Set_NonBlock()
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return TCP_ERROR;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;

    if (!ssl_enabled)
    {
        pRecv = &TcpClient::Recv_Tcp_NonBlock;
        pSend = &TcpClient::Send_Tcp_NonBlock;
	} // end if non-ssl
    else
    {
		pRecv = &TcpClient::SSL_Recv;
		pSend = &TcpClient::SSL_Send;
	} // end else ssl

    return TCP_SUCCESS;
} // end Toggle_NonBlock


/**
 * @brief Releases resources and explicitly closes an open socket.
 *
 * @return int 0 on success alas -1 for error
 */
void TcpClient::Disconnect()
{
	Shutdown_SSL();
    if (fd > 0)
    {
        if (paddr)
        {
            freeaddrinfo(paddr);
            paddr = nullptr;    // Andre style but with c++11 taste
        } // end if addr

        CLOSE(fd);
		fd = -1;
    } // end closing socket
} // end Disconnect


/**
 * @brief shuts down ssl connection and frees resources
 */
void TcpClient::Shutdown_SSL()
{
    if (ssl_enabled)
    {
        if (ssl)
        {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        } // end if ssl
        if (ctx)
        {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        } // end if ctx

        ssl_enabled = false;
	} // end if ssl enabled
} // end Shutdown_SSL


/**
 * @brief enables ssl for this tcp client object
 */
void TcpClient::Enable_SSL()
{
    if (fd > 0)
    {
        ssl_enabled = true;
        pSend = &TcpClient::SSL_Send;
        pRecv = &TcpClient::SSL_Recv;
	} // end if connected
} // end Enable_SSL



//===============================================================================|
/**
 * @brief fills in the protocol independant structure addrinfo; from there the 
 *  kernel determines what protocol to use.
 * 
 * @return int 0 for success -1 for error
 */
int TcpClient::Fill_Addr()
{
    struct addrinfo hints; 

    // is this Windows?
#if defined(WINDOWS)
    WSADATA wsa;        // required by the startup function
    if (WSAStartup(MAKEWORD(2,2), &wsa))
        return TCP_ERROR;      // WSAGetLastError() for details

#endif

    iZero(&hints, sizeof(hints));
    hints.ai_flags = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    // get an address info
    if (getaddrinfo(hostname.c_str(), port.c_str(), &hints, &paddr) != 0)
        return TCP_ERROR;

    return TCP_SUCCESS;       // success
} // end Fill_Addr

/**
 * @brief wraps around blocking send system call with error handling
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read. -4 for try again later, -1 for other errors
 */
int TcpClient::Send_Tcp(const void * buf, size_t len)
{
    ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
    if (n <= 0)
    {
        if (n == 0) return TCP_ERROR_CLOSED;      // peer closed connection
        else
        {
            if (errno == EINTR)
                return TCP_TRYAGAIN;      // try again a little later
			else return TCP_ERROR;        // other system call errors
        } // end else
    } // end if error

    return static_cast<int>(n);
} // end Send_Tcp


/**
 * @brief wraps around blocking recv system call with error handling
 *
 * @param buf space to store the received bytes
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read. -4 for try again later, -1 for other errors
 */
int TcpClient::Recv_Tcp(void * buf, size_t len)
{
    ssize_t n = recv(fd, buf, len, 0);
    if (n <= 0)
    {
        if (n == 0) return TCP_ERROR_CLOSED;       // peer closed connection
        else
        {
            if (errno == EINTR)
				return TCP_TRYAGAIN;   // try again a little later
            else return TCP_ERROR;     // other system call errors
        } // end else
	} // end if error

    return static_cast<int>(n);
} // end Recv_Tcp


/**
 * @brief wraps around non-blocking send system call with error handling
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read. -4 for try again later, -1 for other errors
 */
int TcpClient::Send_Tcp_NonBlock(const void * buf, size_t len)
{
    ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
    if (n <= 0)
    {
        if (n == 0) return TCP_ERROR_CLOSED;      // peer closed connection
        else
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return TCP_TRYAGAIN;   // try again a little later
            else return TCP_ERROR;     // other system call errors
        } // end else
	} // end if error condition

    return static_cast<int>(n);
} // end Send_Tcp_NonBlock


/**
 * @brief wraps around non-blocking recv system call with error handling
 * 
 * @param buf space to store the received bytes
 * @param len length of the buffer above
 * 
 * @return int number of bytes actually read. -4 for try again later, -1 for other errors
 */
int TcpClient::Recv_Tcp_NonBlock(void * buf, size_t len)
{
    ssize_t n = recv(fd, buf, len, 0);
    if (n <= 0)
    {
        if (n == 0) return TCP_ERROR_CLOSED;      // peer closed connection
        else
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return TCP_TRYAGAIN;   // try again a little later
            else return TCP_ERROR;     // other system call errors
        } // end else 
    } // end if error condition

    return static_cast<int>(n);
} // end Recv_Tcp_NonBlock


/**
 * @brief wraps around ssl send call with error handling
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read. -4 for try again later, -1 for other errors
 */
int TcpClient::SSL_Send(const void * buf, size_t len)
{
    ssize_t n = SSL_write(ssl, buf, static_cast<int>(len));
    if (n <= 0)
    {
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE)
            return TCP_TRYAGAIN;       // try again a little later
        else return TCP_ERROR_SSL;     // other ssl errors
    } // end if error condition

    return static_cast<int>(n);
} // end SSL_Send


/**
 * @brief wraps around ssl recv call with error handling
 *
 * @param buf space to store the received bytes
 * @param len length of the buffer above
 *
 * @return int number of bytes actually read. -4 for try again later, -1 for other errors
 */
int TcpClient::SSL_Recv(void * buf, size_t len)
{
    ssize_t n = SSL_read(ssl, buf, static_cast<int>(len));
    if (n <= 0)
    {
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE)
            return TCP_TRYAGAIN;       // try again a little later
        else return TCP_ERROR_SSL;     // other ssl errors
    } // end if error condition

    return static_cast<int>(n);
} // end SSL_Recv