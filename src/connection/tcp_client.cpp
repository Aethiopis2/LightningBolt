/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (Aethiopis2rises@gmail.com)
 * 
 * @version 2.0
 * @date created 9th of April 2025, Wednesday
 * @date updated 13th of Feburary 2026, Friday
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
	: fd{ -1 }, hostname{ "" }, port{ "" }, paddr{ nullptr }, ssl_enabled{ false },
      is_open{false}
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
	: fd{ -1 }, hostname{ host }, port{ nu_port }, paddr{ nullptr }, ssl_enabled{ ssl },
      is_open{ false }
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
 * @brief returns the socket descriptor
 */
int TcpClient::Get_Socket() const
{
    return fd;
} // end get_socket


/**
 * @brief returns a boolean true if connection is still open
 */
bool TcpClient::Is_Open() const
{
    return is_open;
} // end Is_Opne


/**
 * @brief set's the tcp keep alive option and adjusts the idle, interval times and
 *  count before giving up on a connection.
 *
 * @param ilde_sec how long before first proble
 * @param interval_sec time between probes
 * @param count number of failed probes before final death
 *
 * @return a true on success alas false
 */
bool TcpClient::Enable_Keepalive(const int idle_sec, const int interval_sec,
    const int count)
{
#if defined(WINDOWS)

    BOOL opt = TRUE;
    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt)) != 0)
        return false;

    tcp_keepalive ka;
    ka.onoff = 1;
    ka.keepalivetime = idle_sec * 1000;
    ka.keepaliveinterval = interval_sec * 1000;

    DWORD ret;
    return WSAIoctl(s, SIO_KEEPALIVE_VALS,
        &ka, sizeof(ka),
        NULL, 0,
        &ret, NULL, NULL) == 0;

#else

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0)
        return false;

    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_sec, sizeof(idle_sec));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval_sec, sizeof(interval_sec));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));

    return true;

#endif
} // end Enable_Keepalive


/**
 * @brief It simply sets and ssl flag to true and assigns function pointers.
 *  This function must be called prior to statring ssl based connection, if not
 *  intialized via constructor.
 */
void TcpClient::Enable_SSL()
{
    ssl_enabled = true;
    pSend = &TcpClient::SSL_Send;
    pRecv = &TcpClient::SSL_Recv;
} // end Enable_SSL


/**
 * @brief Sets the socket to a non-blocking mode
 */
void TcpClient::Enable_NonBlock()
{
#if defined(__unix__) || defined(__linux__)
    int flags = fcntl(fd, F_GETFL);
    if (flags != -1)
    {
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1)
        {
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
        } // end fcntl set ok
    } // end fcntl get ok
#else
    ulong mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) == NO_ERROR)
    {
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
    } // end if ok set to non blocking
#endif
} // end Enable_NonBlock


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
 * @brief Connects to server/host at the provided address using the best protocol 
 *  the kernel can determine. The function optionally connects via ssl if enabled.
 * 
 * @return LB_Status code, LB_Ok on success. LB_FAIL on SSL error, LB_RETRY on
 *  socket error.
 */
LBStatus TcpClient::Connect()
{
    LBStatus rc;    // store's function results

    if (ssl_enabled)
    {
		rc = Init_SSL();
        if (!LB_OK(rc))
            return rc;
	} // end if ssl

    // initalize the base addr info first0, 0
    rc = Fill_Addr();
    if (!LB_OK(rc))
        return rc;
    
    struct addrinfo *p_alias = paddr;

    do {
        fd = socket(p_alias->ai_family, p_alias->ai_socktype, p_alias->ai_protocol);
        if (fd < 0)
            continue;

        if (connect(fd, p_alias->ai_addr, p_alias->ai_addrlen) == 0)
        {
            if (ssl_enabled)
            {
                rc = SSL_Connect();
                if (!LB_OK(rc))
                {
                    CLOSE(fd);
                    fd = -1;
                    continue;
                } // end if no ssl
            } // end if ssl issue

            return LB_Make();
        } // end if connect ok

        CLOSE(fd);
        fd = -1;
    } while ((p_alias = p_alias->ai_next) != NULL);

    // at the end of the day if socket is null
    return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_SYS,
        LBCode::LB_CODE_NONE, errno);
} // end Connect


/**
 * @brief Wraps around send system call
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above
 *
 * @return LB_Ok or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::Send(const void* buf, const int len)
{
	return (this->*pSend)(buf, len);
} // end Send


/**
 * @brief Wraps around recv or whichever is better system call
 *
 * @param buf space to store the received bytes
 * @param len length of the buffer above
 *
 * @return LB_Ok or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::Recv(void* buf, const int len)
{
	return (this->*pRecv)(buf, len);
} // end Recv



//===============================================================================|
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
 * @brief fills in the protocol independant structure addrinfo; from there the 
 *  kernel determines what protocol to use.
 * 
 * @return LB_Status code, LB_Ok or 0 on success, LB_FAIL on failed.
 */
LBStatus TcpClient::Fill_Addr()
{
    struct addrinfo hints; 

    // is this Windows?
#if defined(WINDOWS)
    WSADATA wsa;        // required by the startup function
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
    {
        return LB_Make(LB_Action::LB_FAIL, LB_Domain::LB_DOM_SYS, 
            LBCode::LB_CODE_NONE, static_cast<u32>(WSAGetLastErrror()));
    } // end if start up

#endif

    iZero(&hints, sizeof(hints));
    hints.ai_flags = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    // get an address info
    if (getaddrinfo(hostname.c_str(), port.c_str(), &hints, &paddr) != 0)
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_SYS,
            LBCode::LB_CODE_NONE, errno);

    return LB_Make();       // success
} // end Fill_Addr


/**
 * @brief Initalizes SSL library and creates context and ssl object. For first
 *  time it also loads and initalizes openssl libs.
 *
 * @return packed status code with LB_OK for success or LB_FAIL on failure
 */
LBStatus TcpClient::Init_SSL()
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
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_SSL,
            LBCode::LB_CODE_NONE, static_cast<u32>(ERR_get_error()));

    ssl = SSL_new(ctx);
    return LB_Make();
} // end Init_SSL


/**
 * @brief performs ssl handshake with the peer after connecting tcp socket.
 *
 * @return LB_OK on success alas, LB_FAIL on failure to connect.
 */
LBStatus TcpClient::SSL_Connect()
{
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) <= 0)
    {
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_SSL,
            LBCode::LB_CODE_NONE, static_cast<u32>(ERR_get_error()));
	} // end if ssl connect error

    return LB_Make();
} // end SSL_Connect


/**
 * @brief wraps around blocking send system call with error handling
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above also returned with actual bytes read
 *
 * @return LB_Ok or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::Send_Tcp(const void * buf, const int len)
{
    int bytes_sent = 0;
    char* alias = (char*)buf;

    do {
        ssize_t n = send(fd, alias, len - bytes_sent, 0);
        if (n <= 0)
        {
            if (errno == EINTR)
            {
                std::this_thread::yield();   // wait some and try again
                continue;
            } // end if interrupted
            else 
                return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_SYS,
                    LBCode::LB_CODE_NONE, errno); 
        } // end if error

        bytes_sent += n;
        alias += n;
    } while (bytes_sent < len);

    return LB_OK_INFO(bytes_sent);
} // end Send_Tcp


/**
 * @brief wraps around blocking recv system call with error handling
 *
 * @param buf space to store the received bytes
 * @param len length of the buffer above also returned with actual bytes read
 *
 * @return LB_Ok or 0 on success, with actual bytes read packed into LBStatus return
 */
LBStatus TcpClient::Recv_Tcp(void * buf, const int len)
{
    ssize_t n = recv(fd, buf, len, 0);
    if (n <= 0)
    {
        if (errno == EINTR)
            return LB_Make(LBAction::LB_WAIT, LBDomain::LB_DOM_SYS);          // try again a little later
        else return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_SYS,
            LBCode::LB_CODE_NONE, errno);     // other system call errors
	} // end if error

    return LB_OK_INFO(n);
} // end Recv_Tcp


/**
 * @brief wraps around non-blocking send system call with error handling
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above also returned with actual bytes read
 *
 * @return LB_Ok or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::Send_Tcp_NonBlock(const void * buf, const int len)
{
    int bytes_sent = 0;
    char* alias = (char*)buf;

    do {
        ssize_t n = send(fd, alias, len - bytes_sent, 0);
        if (n <= 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::this_thread::yield();      // try again a little later
                continue;
            } // end if
            else 
                return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_SYS, 
                    LBCode::LB_CODE_NONE, errno);
        } // end if error condition

        bytes_sent += n;
        alias += n;
    } while (bytes_sent < len);

    return LB_OK_INFO(bytes_sent);
} // end Send_Tcp_NonBlock


/**
 * @brief wraps around non-blocking recv system call with error handling
 * 
 * @param buf space to store the received bytes
 * @param len length of the buffer above also returned with actual bytes read
 * 
 * @return LB_Ok_INFO or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::Recv_Tcp_NonBlock(void * buf, const int len)
{
    ssize_t n = recv(fd, buf, len, 0);
    if (n <= 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return LB_Make(LBAction::LB_WAIT, LBDomain::LB_DOM_SYS);   // try again a little later
        else return LB_Make(LBAction::LB_RETRY, LBDomain::LB_DOM_SYS, 
            LBCode::LB_CODE_NONE, errno);     // other system call errors
    } // end if error condition

    return LB_OK_INFO(static_cast<u32>(n));
} // end Recv_Tcp_NonBlock


/**
 * @brief wraps around ssl send call with error handling
 *
 * @param buf space containing the bytes to send to peer
 * @param len length of the buffer above also returned with actual bytes read
 *
 * @return LB_Ok or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::SSL_Send(const void * buf, const int len)
{
    int bytes_sent = 0;
    char* alias = (char*)buf;

    do {
        int n = SSL_write(ssl, alias, len - bytes_sent);
        if (n <= 0)
        {
            int ssl_err = SSL_get_error(ssl, n);
            if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE)
            {
                std::this_thread::yield();
                continue;
            } // end if 
            else
            {
                return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_SSL,
                    LBCode::LB_CODE_NONE, static_cast<u32>(ERR_get_error())); 
            } // end else
        } // end if error condition

        bytes_sent += n;
        alias += bytes_sent;
    } while (bytes_sent < len);

    return LB_OK_INFO(bytes_sent);
} // end SSL_Send


/**
 * @brief wraps around ssl recv call with error handling
 *
 * @param buf space to store the received bytes
 * @param len length of the buffer above also returned with actual bytes read
 *
 * @return LB_Ok or 0 on success, with actual bytes packed into LBStatus return
 */
LBStatus TcpClient::SSL_Recv(void * buf, const int len)
{
    int n = SSL_read(ssl, buf, len);
    if (n <= 0)
    {
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE)
            return LB_Make(LBAction::LB_WAIT, LBDomain::LB_DOM_SSL);          // try again a little later
        else
        {
            return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_SSL,
                LBCode::LB_CODE_NONE, static_cast<u32>(ERR_get_error()));   // other ssl errors
		} // end else
    } // end if error condition

    return LB_OK_INFO(n);
} // end SSL_Recv