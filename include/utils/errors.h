/**
 * @file errors.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief Simple C style error handlers used for mostly displaying error messages 
 *  and terminating the app on err.
 * @version 1.2
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef ERRORS_H
#define ERRORS_H



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"
#include <syslog.h>         /* for syslog() */




//===============================================================================|
//          MACROS
//===============================================================================|
/* this macro stops 'gcc -Wall' complaining that "control reaches end of non void 
 *  function" if we use the following functions to terminate main() or some other 
 *  non-void function */
#ifdef __GNUC__

#define NORETURN __attribute__ ((__noreturn__))
#else
#define NORETURN

#endif



//===============================================================================|
//          GLOBALS
//===============================================================================|
// extern int daemon_proc; /* set by daemon_init() to a non zero value 
//                          *  whenever the process runs as a daemon */





//===============================================================================|
//          PROTOTYPES
//===============================================================================|
void Dump_Err(const char *fmt, ...);
void Dump_App_Err(const char *fmt, ...);
void Dump_Err_Exit(const char *fmt, ...);
void Fatal(const char *fmt, ...);



#endif