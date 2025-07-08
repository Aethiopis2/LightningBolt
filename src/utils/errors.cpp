/**
 * @file errors.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Implementation detail for errors.h
 * @version 1.2
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "utils/errors.h"



//===============================================================================|
//          MACROS
//===============================================================================|



//===============================================================================|
//          GLOBALS
//===============================================================================|



//===============================================================================|
//          FUNCTIONS
//===============================================================================|
/**
 * @brief this is the function that does the actual printing of messages to either 
 *  the console or a log file, depending on the defintions for the external 
 *  daemon_proc global. Should this external variable be set then we would be logging 
 *  to log file using syslog() system call alas output goes to the standard output 
 *  and error streams.
 * 
 * @param errno_flag 0 to mean app sepecific error, while 1 to mean syscall error
 * @param level used for syslog()
 * @param fmt formatted buffer
 * @param ap list of variable length parameters
 */
static void Output_Err(int errno_flag, int level, const char *fmt, va_list ap)
{
    size_t n;                       // stores string length for buffers
    char buf[MAXLINE + 1];     

    int saved_errno = errno;        /* save the errno */

    vsnprintf(buf, MAXLINE, fmt, ap);
    n = strlen(buf);

    // test if we have the errno flag set
    if (errno_flag) 
        snprintf(buf + n, MAXLINE - n, ": \033[31m%s\033[37m", strerror(saved_errno));
    strcat(buf, "\n");      // append a new line

    int dummy = level;
    level = dummy;
    // now are we logging to system log or dumping to standard out
    // if (daemon_proc)
    //     syslog(level, buf);
    // else 
    {
        /*fflush(stdout);            // flush any pending outputs
        fputs(sz_buf, stderr);       // dump to stderr and flush output
        fflush(stderr);*/

        // do it the C++ way
        std::cerr << "\033[31m\t*** Err \033[37m" << buf << "\033[37m" << std::endl;
    } // end else
} // end Output_Err



//===============================================================================|
/**
 * @brief terminates the process in one of the following ways. If the env't 
 *  variable EF_DUMPCORE has been defined it calls abort to produce an error dump, 
 *  or using exit() or _exit() system calls which have the effect of flushing the 
 *  buffer in the latter case
 * 
 * @param use_exit3 boolean value indicating which way to end; using exit or _exit.
 * 
 */
static void Terminate(Boolean use_exit3)
{
    char *sz_s;

    /* Dump core if EF_DUMPCORE is defined and is a non-empty string; otherwise call
     *  exit(3) or _exit(2) depending on the value of use_exit variable
     */
    sz_s = getenv("EF_DUMPCORE");

    if (sz_s != NULL && *sz_s != '\0')
        abort();
    else if (use_exit3)
        exit(EXIT_FAILURE);
    else 
        _exit(EXIT_FAILURE);
} // end Terminate



//===============================================================================|
/**
 * @brief function is used to print/dump messages (error or not) into the standard 
 *  output stream if daemon_proc global has been defined to be 0. If not, then all 
 *  output goes to the loggig system using syslog() function call.
 *  
 * @param fmt string containing error description along with formatting options if any
 */
void Dump_Err(const char *fmt, ...)
{
    va_list arg_list;           /* handles the variable length arguments */

    va_start(arg_list, fmt);
    Output_Err(1, LOG_INFO, fmt, arg_list);
    va_end(arg_list);
} // end Dump_Err



//===============================================================================|
/**
 * @brief as Dump_Err function only difference is this is used to abruptly 
 *  terminate the process with/without some core dump files produced.
 *  
 * @param fmt string containing error description along with formatting options if any
 */
void Dump_Err_Exit(const char *fmt, ...)
{
    va_list arg_list;

    va_start(arg_list, fmt);
    Output_Err(1, LOG_INFO, fmt, arg_list);
    va_end(arg_list);

    Terminate(TRUE);
} // end Dump_Err_Exit



//===============================================================================|
/**
 * @brief this function is used for dumping errors of non system call origin to 
 *  console/log file and terminate the application, since these are application 
 *  wide fatal errors.
 * 
 * @param fmt string containing error description along with formatting options if any
 */
void Fatal(const char *fmt, ...)
{
    va_list arg_list;

    va_start(arg_list, fmt);
    Output_Err(0, LOG_INFO, fmt, arg_list);
    va_end(arg_list);

    Terminate(TRUE);
} // end Fatal



//===============================================================================|
/**
 * @brief Dump's application specific errors, much like Dump_Err & Dump_Err_Exit, 
 *  except this won't kill the app or use the standard err no.
 * 
 * @param fmt string containing error description along with formatting options if any 
 */
void Dump_App_Err(const char *fmt, ...)
{
    va_list arg_list;

    va_start(arg_list, fmt);
    Output_Err(0, LOG_INFO, fmt, arg_list);
    va_end(arg_list);
} // end Dump_App_Err