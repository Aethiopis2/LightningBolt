/**
 * @author Rediet Worku, Dr. aka Aethiopis II ben Zahab (PanaceaSolutionsEth@gmail.com)
 * 
 * @version 1.4
 * @date created 3rd of March, Sunday
 * @date updated 18th of January 2026, Sunday
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
namespace Utils
{
    int Init_Configuration(const std::string& filename, SYS_CONFIG& sys_config);
    void Dump_Hex(const char* buf, const size_t len);
    std::vector<std::string> Split_String(const std::string& str, const char tokken);
    std::string Get_Formatted_String(const std::string& app_name = APP_NAME);
    void Replace_String(std::string& str, const std::string& patt, const std::string& replace);
    std::string Format_Numerics(const double num);
    bool Is_Number(const std::string& s);
    void Print(const char* s, ...);
    std::string Console_Out(const std::string app_name = APP_NAME);
    std::string Generate_UUID();
    void Print_Title(const std::string coname = "RedLabs",
        const std::string url = "Email: PanaceaSolutionsEth@Gmail.com");
    void Swap(u64* num);
    std::string String_ToLower(const std::string& str);
    std::string String_ToUpper(const std::string& str);
}

#endif