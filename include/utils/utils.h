/**
 * @file utils.h
 * @author Rediet Worku, Dr. aka Aethiopis II ben Zahab (PanaceaSolutionsEth@gmail.com)
 *
 * @brief contains prototypes and global objects used commonly through out by different apps. 
 *  These include ablity to read/write from/to configuration file and so on so forth.
 * @version 1.4
 * @date 2024-03-03, Sunday
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef UTILS_H
#define UTILS_H



//=====================================================================================|
//          INCLUDES
//=====================================================================================|
#include "basics.h"



//=====================================================================================|
//          DEFINES
//=====================================================================================|



//=====================================================================================|
//          TYPES
//=====================================================================================|
/**
 * @brief 
 *  a little system configuration container
 */
typedef struct SYSTEM_CONFIGURATION
{
    std::unordered_map<std::string, std::string> config;
} SYS_CONFIG, *SYS_CONFIG_PTR;





//=====================================================================================|
//          GLOBALS
//=====================================================================================|
//extern SYS_CONFIG sys_config; // an instance of system config globally visible to all





//=====================================================================================|
//          PROTOTYPES
//=====================================================================================|
int Init_Configuration(const std::string &filename, SYS_CONFIG &sys_config);
void Dump_Hex(const char *buf, const size_t len);
std::vector<std::string> Split_String(const std::string &str, const char tokken);
std::string Get_Formatted_String(const std::string &app_name = APP_NAME);
void Replace_String(std::string &str, const std::string &patt, const std::string &replace);
std::string Format_Numerics(const double num);
bool Is_Number(const std::string &s);
void Print(const char *s, ...);
std::string Console_Out(const std::string app_name = APP_NAME);
std::string Generate_UUID();
void Print_Title(const std::string coname = "Panacea Solutions, Plc", 
    const std::string url = "http://www.lone_star_rises.panaceasolutions.com");
void Swap(u64 *num);

#endif