/**
 * @file addicion.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief implementation detials for BoltDecoder object
 * 
 * @version 1.0
 * @date 14th of April 2025, Monday.
 * 
 * @copyright Copyright (c) 2025
 */


//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "bolt/boltvalue.h"
#include "bolt/bolt_jump_table.h"
#include "bolt/bolt_decoder.h"




//===============================================================================|
//          MACROS
//===============================================================================|
/**
 * @brief unimplementated protocol; program should not get here
 *  on normal circumstances.
 */
static inline bool UnImp(u8*& pos, BoltValue& out)
{
    std::cout << "Not supposed to be here" << std::endl;
    ++pos;
    out = BoltValue::Make_Unknown();
    return false;
} // end Unimplemented


/**
 * @brief decode's a null value
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
static inline bool Decode_Null(u8*& pos, BoltValue& out)
{
    ++pos;
    out = BoltValue::Make_Null();
    return true;
} // end Decode_Null


/**
 * @brief decode's a boolean true
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
static inline bool Decode_True(u8*& pos, BoltValue& out) 
{
    ++pos;
    out = BoltValue::Make_Bool(true);
    return true;
} // end Decode_True


/**
 * @brief decode's a boolean false
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
static inline bool Decode_False(u8*& pos, BoltValue& out) 
{
    ++pos;
    out = BoltValue::Make_Bool(false);
    return true;
} // end Decode_False


/**
 * @brief decode's integer values in the range of 0xF0 - 0x7F, in which
 *  the marker itself is the value.
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
static inline bool Decode_Tiny_Int(u8*& pos, BoltValue& out)
{
    out = BoltValue::Make_Int(static_cast<s8>(*pos++));
    return true;
} // end Decode_Tiny_Int


/**
 * @brief decode's integer values in the range with tags/markers 0xC8, 0xC9, 0xCA, and 0xCB 
 *  these tags define the next byte(s) is an 8, 16, 32, and 64 bit signed integer resp. 
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
template<typename T>
static inline bool Decode_Int(u8*& pos, BoltValue& out)
{
    out.Set_Int_RawDirect<T>(++pos);
    pos += sizeof(T);

    return true;
} // end Decode_Int


/**
 * @brief decode's float values with signature/marker 0xC1; which are 64-bit
 *  encoded values.
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
static inline bool Decode_Float(u8*& pos, BoltValue& out)
{
    double v;
    iCpy(&v, ++pos, sizeof(double));
    pos += sizeof(double);

    out = BoltValue::Make_Float(swap_endian_double(v));
    return true;
} // end Decode_Float


/**
 * @brief bytes are but arrays of bytes, shorts, and ints with marker
 *  0xCC, 0xCD, 0xCE resp. So being exactly like strings we can deal with them in a 
 *  similar way 
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
template <typename T>
static inline bool Decode_Bytes(u8*& pos, BoltValue& out)
{
    // const u8 marker = *pos++;
    T len; 
    if constexpr (sizeof(T) == 1) 
        len = static_cast<T>(*++pos);
    else if constexpr (sizeof(T) == 2)
        len = ntohs(len);
    else if constexpr (sizeof(T) == 4) //(marker == 0xCE)
        len = ntohl(len);

    pos += sizeof(T);
    out = BoltValue::Make_Bytes(pos, len);
    pos += (len);
    
    return true;
} // end Decode_Bytes


/**
 * @brief decodes strings that are having length <= 15 or markers that begin
 *  with 0x80
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
static inline bool Decode_Tiny_String(u8*& pos, BoltValue& out)
{
    u8 len = ((*pos) & 0x0F);
    ++pos;
    out = BoltValue::Make_String(reinterpret_cast<const char*>(pos), 
        static_cast<u32>(len) );
    pos += len;
    return true;
} // end Decode_Tiny_String


/**
 * @brief decodes strings that are having length > 15 or markers that begin
 *  with 0xD0 for strings of length <= 255, 0xD1 <= 64K or 0xD2 <= 4GB
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true
 */
template<typename T>
static inline bool Decode_String(u8*& pos, BoltValue& out)
{
    T len;
    iCpy(&len, ++pos, sizeof(T));
    if constexpr (sizeof(T) > 1)
    {
        if constexpr (is_big_endian)
            len = byte_swap(len);
    } // end if

    pos += sizeof(T);
    out = BoltValue::Make_String(reinterpret_cast<const char*>(pos), 
        static_cast<u32>(len) );
    pos += len;

    return true;
} // end Decode_String


/**
 * @brief decodes tiny list values which are tags from 0x90 - 0x9F. The difficulity
 *  of lists lies in the fact that they are hetergenous in nature; this might technically
 *  complicate things.
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true when successful alas a false
 */
static inline bool Decode_List_Tiny(u8*& pos, BoltValue& out)
{
    u8 header = *pos++;
    u8 size = header & 0x0F;
    out = BoltValue::Make_List(pos, size);

    BoltValue dummy;
    for (u8 i = 0; i < size; ++i)
    {
        if (!jump_table[*pos](pos, dummy))
            return false;
    } // end for

    return true;
} // end Decode_List_Tiny


/**
 * @brief decodes list values with more than 16 items; with tags 0xD4, 0xD5, 0xD6.
 *  the next byte, word, or double word determines the lenght of the string
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true when successful alas a false
 */
template<typename T>
static inline bool Decode_List(u8*& pos, BoltValue& out)
{
    T size;
    ++pos;
    iCpy(&size, pos, sizeof(T));
    if constexpr (sizeof(T) > 1)
    {
        if constexpr (is_big_endian)
            size = byte_swap(size);
    } // end if

    pos += sizeof(T);
    out = BoltValue::Make_List(pos, size);
    BoltValue dummy;
    for (T i = 0; i < size; i++)
    {
        u8 tag = *pos;
        if (!jump_table[tag](pos, dummy))
            return false;
    } // end for Decode_List

    return true;
} // end Decode_List


/**
 * @brief decodes map values of upto 16 items in length which are in 
 *  the range of tags 0xA0 - 0xAF
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true when successful alas a false
 */
static inline bool Decode_Map_Tiny(u8*& pos, BoltValue& out)
{
    u8 header = *pos++;   //*((*pos)++);
    u8 size = header & 0x0F;
    out = BoltValue::Make_Map(pos, size);

    BoltValue dummy;
    for (u8 i = 0; i < size; i++)
    {
        if (!jump_table[*pos](pos, dummy))  // key
            return false;
        if (!jump_table[*pos](pos, dummy))  // val
            return false;
    } // end for

    return true;
} // end Decode_Map_Tiny


/**
 * @brief decodes map values of more than 16-items in length upto the
 *  protocol maximum.
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true when successful alas a false
 */
template<typename T>
static inline bool Decode_Map(u8*& pos, BoltValue& out)
{
    T len;
    iCpy(&len, ++pos, sizeof(T));
    if constexpr (sizeof(T) > 1)
    {
        if constexpr (is_big_endian)
            len = byte_swap(len);
    } // end if

    pos += sizeof(T);
    out = BoltValue::Make_Map(pos, len);

    BoltValue dummy;
    for (u8 i = 0; i < len; i++)
    {
        if (!jump_table[*pos](pos, dummy))  // key
            return false;

        if (!jump_table[*pos](pos, dummy))  // val
            return false;
    } // end for

    return true;
} // end Decode_Map_Tiny


/**
 * @brief decodes structure values which are by default defined as tiny begining
 *  with tags 0xB0 and ending with tag 0xBF.
 * 
 * @param pos current position into the buffer being decoded
 * @param out the decoded boltvalue
 * 
 * @return a true when successful alas a false
 */
static inline bool Decode_Struct(u8*& pos, BoltValue& out)
{
    u8 header = *pos++; 
    u8 size = header & 0x0F;
    u8 tag = *pos++;  
    out = BoltValue::Make_Struct(pos, tag, size);

    BoltValue dummy;
    for (u8 i = 0; i < size; i++)
    {
        u8 tag = *pos;
        if (!jump_table[tag](pos, dummy))
            return false;
    } // end for

    return true;
} // end Decode_Struct



// -- jump table definiton
const DecodeFn jump_table[256] = {
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 0 - 7 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 8 - 15 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 16 - 23 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 24 -  31 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 32 - 39 */

    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 40 - 47 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 48 - 55 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 56 - 63 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 64 -  71 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 72 - 79 */

    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 80 - 87 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 88 - 95 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 96 - 103 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 104 - 111 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 112 - 119 */

    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 120 - 127 */
    &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String,             /* 128 - 135 */
    &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String, &Decode_Tiny_String,             /* 136 - 143 */
    &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny,             /* 144 - 151 */
    &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny, &Decode_List_Tiny,             /* 152 - 159 */

    &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny,             /* 160 - 167 */
    &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny, &Decode_Map_Tiny,             /* 168 - 175 */
    &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct,             /* 176 - 183 */
    &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct, &Decode_Struct,             /* 184 - 191 */
    &Decode_Null, &Decode_Float, &Decode_False, &Decode_True, &UnImp, &UnImp, &UnImp, &UnImp,             /* 192 - 199 */

    &Decode_Int<u8>, &Decode_Int<u16>, &Decode_Int<u32>, &Decode_Int<u64>, &Decode_Bytes<u8>, &Decode_Bytes<u16>, &Decode_Bytes<u32>, &UnImp,             /* 200 - 207 */
    &Decode_String<u8>, &Decode_String<u16>, &Decode_String<u32>, &UnImp, &Decode_List<u8>, &Decode_List<u16>, &Decode_List<u32>, &UnImp,             /* 208 - 215 */
    &Decode_Map<u8>, &Decode_Map<u16>, &Decode_Map<u32>, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp,             /* 216 - 223 */
    &UnImp, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp,             /* 224 - 231 */
    &UnImp, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp, &UnImp,             /* 232 - 239 */

    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int,             /* 240 - 247 */
    &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int, &Decode_Tiny_Int             /* 248 - 255 */
};