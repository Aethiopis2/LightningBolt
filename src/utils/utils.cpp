/**
 * @file utils.cpp
 * @author Rediet Worku, Dr. aka Aethiopis II ben Zahab (PanaceaSolutionsEth@gmail.com)
 * 
 * @brief contains defintion for utils.h file function prototypes
 * @version 1.4
 * @date 2024-03-03, Sunday
 * 
 * @copyright Copyright (c) 2024
 * 
 */



//=====================================================================================|
//          INCLUDES
//=====================================================================================|
#include "utils/utils.h"





//=====================================================================================|
//          DEFINES
//=====================================================================================|





//=====================================================================================|
//          TYPES
//=====================================================================================|






//=====================================================================================|
//          GLOBALS
//=====================================================================================|
const std::string CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";






//=====================================================================================|
//          FUNCTIONS
//=====================================================================================|
/**
 * @brief initalizes the global sys_config structure which contains a map of key-value 
 *  pairs that store various system related configuration.
 * 
 * @param filename a full filename + path containing the config file usually named "config.dat" 
 * 
 * @return int a 0 on success, a -1 if file open failed.
 */
int Init_Configuration(const std::string &filename, SYS_CONFIG &sys_config)
{
    std::string sz_first, sz_second;
    std::ifstream config_file{filename.c_str()};

    if (!config_file)
        return -1;

    sys_config.config.clear();

    while (config_file >> quoted(sz_first) >> quoted(sz_second))
        sys_config.config[sz_first] = sz_second;

    config_file.close();
    
    // success
    return 0;
} // end Init_Configuration

//=====================================================================================|
/**
 * @brief Splits a string using the sz_token provided and returns them as vector of strings
 * 
 * @param str the string to split   
 * @param token the token that let's the program know at which points it must split
 * 
 * @return std::vector<std::string> a vector of strings split is returned
 */
std::vector<std::string> Split_String(const std::string &str, const char tokken)
{
    std::string s;      // get's the split one at a time
    std::vector<std::string> vec;
    std::istringstream istr(str.c_str());
    
    while (std::getline(istr, s, tokken))
        vec.push_back(s);

    return vec;
} // end Split_String

//=====================================================================================|
/**
 * @brief Prints the contents of buffer in hex notation along side it's ASCII form much 
 *  like hex viewer's do it.
 * 
 * @param buf the information to dump as hex and char arrary treated as a char array. 
 * @param len the length of the buffer above
 */
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cctype>

void Dump_Hex(const char* buf, size_t len)
{
    const size_t columns = 16;   // 16 bytes per row
    size_t i, j;

    // Print header
    std::cout << "\n          ";
    for (i = 0; i < columns; i++)
    {
        std::cout << "\033[36m" << std::setw(2) << std::setfill('0')
            << std::hex << std::uppercase << i << " ";
    } // end for header
    std::cout << "\033[37m\n";

    for (i = 0; i < len; i += columns)
    {
        // Print 32-bit address offset
        std::cout << "\033[36m" << std::setw(8) << std::setfill('0') 
            << std::hex << std::uppercase << static_cast<uint32_t>(i) 
            << ":\033[37m ";

        // Print hex values
        size_t remaining = std::min(columns, len - i);
        for (j = 0; j < remaining; j++)
        {
            std::cout << std::setw(2) << std::setfill('0') << std::hex
                << static_cast<uint32_t>(static_cast<uint8_t>(buf[i + j])) << " ";
        } // end for hex

        // Fill blanks if last row is short
        if (remaining < columns)
        {
            for (j = 0; j < columns - remaining; j++)
                std::cout << "   ";
        } // end if

        std::cout << "\t\t";

        // Print ASCII representation
        for (j = 0; j < remaining; j++)
        {
            char c = buf[i + j];
            if (std::isprint(static_cast<unsigned char>(c)))
                std::cout << c << " ";
            else
                std::cout << ". ";
        } // end for ascii

        std::cout << "\n";
    }

    std::cout << "\n";
} // end Dump_Hex

//=====================================================================================|
/**
 * @brief Formats the date and time for displaying on the screen; so that my applications
 *  could have a standard look and feel when run through the console.
 * 
 * @return std::string a formatted datetime
 */
std::string Get_Formatted_String(const std::string &app_name)
{
    time_t curr_time;
    char buf[MAXPATH];

    std::time(&curr_time);
    snprintf(buf, MAXPATH, "\033[33m%s\033[37m", app_name.c_str());
    std::strftime(buf + strlen(buf), MAXPATH, " \033[34m%d-%b-%y, %T\033[37m: ", 
        localtime(&curr_time));

    return buf;
} // end display_time

//=====================================================================================|
/**
 * @brief Neatly replaces the string patt from str using the string replace. It basically
 *  splits the string in half and insert's the replace string in between.
 * 
 * @param str the original string, when function returns the replaced string if successful
 * @param patt the pattern to replace
 * @param replace the replacement string
 * 
 */
void Replace_String(std::string &str, const std::string &patt, const std::string &replace)
{
    size_t pos;
    if (( pos = str.find(patt)) != std::string::npos)
    {
        str = str.substr(0, pos) + replace +  
            str.substr(pos + patt.length(), str.length() - pos);
    } // end find
} // end Replace_String

//=====================================================================================|
/**
 * @brief Format's the numerical value with comma's. Useful in financial apps where its
 *  common to format numerics in such manner.
 * 
 * @param num the number to format
 * 
 * @return std::string a formatted numerical string 
 */
std::string Format_Numerics(const double num)
{
    std::ostringstream ostream;
    ostream << std::setprecision(2) << std::fixed << num;

    std::string s{ostream.str()};
    int pos = s.find(".");

    for (int i = pos - 3; i >= 1; i -= 3)
    {
        char t = s[i];

        for (int j = i + 1; j < (int)s.length(); j++)
        {
            char k = s[j];
            s[j] = t;
            t = k;
        } // end nested for

        s[i] = ',';
        s += t;
    } // end for

    return s;
} // end Format_Numerics

//=====================================================================================|
/**
 * @brief Determines if the string s is a number or not.
 * 
 * @param s the string to test
 * 
 * @return true 
 * @return false 
 */
bool Is_Number(const std::string &s)
{
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) 
        { return !isdigit(c); }) == s.end();
} // end Is_Number

//=====================================================================================|
/**
 * @brief Prints a formatted stylized string message to console (Good ol' C style)
 * 
 * @param s the message/string to print on console
 */
void Print(const char *s, ...)
{
    va_list arg_list;
    char buf[MAXLINE+1];

    va_start(arg_list, s);

    vsnprintf(buf, MAXLINE, s, arg_list);
    printf("%s%s\n", Get_Formatted_String().c_str(), buf);
    va_end(arg_list);
} // end Print

//=====================================================================================|
/**
 * @brief Formats the date and time for displaying on the screen; so that my applications
 *  could have a standard look and feel when run through the console.
 * 
 * @return std::string a formatted datetime
 */
std::string Console_Out(const std::string app_name)
{
    time_t curr_time;
    char buf[MAXPATH];

    std::time(&curr_time);
    snprintf(buf, MAXPATH, "\033[33m%s\033[37m", app_name.c_str());
    std::strftime(buf + strlen(buf), MAXPATH, " \033[34m%d-%b-%y, %T\033[37m: ", 
        localtime(&curr_time));

    return buf;
} // end display_time

//=====================================================================================|
/**
 * @brief Generates a uuid for use in unique identification. The algorithim is very code
 *  and I did not want to use boost library just for this. 
 *  Courtesy of github; gist.github.com/fernandomv3/46a6d7656f50ee8d39dc
 * 
 * @return std::string a uuid that should be unique
 */
std::string Generate_UUID()
{
    std::string uuid = std::string(36, ' ');
    int rnd = 0;

    uuid[8] = uuid[13] = uuid[18] = uuid[23] = '-';
    uuid[14] = '4';
    std::srand(time(NULL));

    for (int i = 0; i < 36; i++) 
    {
        if (i != 8 && i != 13 && i != 14 && i != 23)
        {
            if (rnd < 0x02)
                rnd = 0x2000000 + (std::rand() * 0x1000000);
            
            rnd >>= 4;
            uuid[i] = CHARS[(i == 19) ? ((rnd &0xf) & 0x3) | 0x8 : rnd & 0xf];
        } // end if
    } // end for

    return uuid;
} // end Generate_UUID

//=====================================================================================|
/**
 * @brief Prints title; i.e. company name and website info among other things.
 * 
 */
void Print_Title(const std::string coname, const std::string url)
{
    std::cout << "\n\t     \033[36m" << coname << "\033[37m\n"
        << "\t\t\033[34m" << url << "\033[037m\n" << std::endl;
} // end Print_Ttile

//=====================================================================================|
/**
 * @brief A kooked swapping algorthim that is pretty predicatable, the same re-application
 *  results in the orginal value.
 * 
 * @param num the number to swap
 */
void Swap(u64 *num)
{
    char *ptr = (char *)num;
    char swap = ptr[5];
    ptr[5] = ptr[7];
    ptr[7] = swap;
    
    swap = ptr[2];
    ptr[2] = ptr[3];
    ptr[3] = swap;

    swap = ptr[1];
    ptr[1] = ptr[4];
    ptr[4] = swap; 
} // end Swap

//=====================================================================================|
/**
 * @brief converts a everything alphabetic in the string str to lower case. The function
 *  performs inplace conversion
 * 
 * @param str the string to convert and store
 * @param len length of the string
 */
void String_ToLower(char* str, const size_t len)
{
    // loop and convert each char
    for (size_t i = 0; i < len; i++)
        str[i] = tolower((unsigned char)str[i]);
} // end String_ToLower

//=====================================================================================|
/**
 * @brief converts a everything alphabetic in the string str to upper case. The function
 *  performs inplace conversion like its buddy above
 *
 * @param str the string to convert and store
 * @param len length of the string
 */
void String_ToUpper(char* str, const size_t len)
{
    // loop and convert each char
    for (size_t i = 0; i < len; i++)
        str[i] = toupper((unsigned char)str[i]);
} // end String_ToLower