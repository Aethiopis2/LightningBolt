/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @version 1.0
 * @date 13th of April 2025, Sunday.
 * 
 * @copyright Copyright (c) 2025 - 2026.
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "bolt/bolt_buf.h"
#include "bolt/bolt_message.h"
#include "utils/utils.h"





//===============================================================================|
//          GLOBALS
//===============================================================================|




//===============================================================================|
//          CLASS
//===============================================================================|
/**
 * @brief Encoder is more of stream packer according to bolt protocol specs. The
 *  class packs values passed into its only interface Encode() using PackStream v1
 *  specifications. Thus the class specializes in encoding, Nulls, Ints, Floats, 
 *  Bytes [], Chars/Strings, Bools - as wrappers and pointers around standard C++
 *  objects, and Maps/Dictionary, Lists, Structs, and Messages as containers to
 *  those primitive types.
 */
class BoltEncoder
{
public:

    explicit BoltEncoder(BoltBuf &b) 
        : buf(b) {}

    /**
     * @brief encode's objects according to bolt protocol PackStream
     *  format.
     * 
     * @param val the value/object to encode
     * @param len optional: if sizeof value can't be inferred.
     */
    template <typename T>
    inline int Encode(const T& val, const size_t len = 0) 
    {
        if (!Has_Free(val, len))
            return -1;

        if constexpr (std::is_same_v<T, std::nullptr_t>) 
        {
            Encode_Null();
        } // end if null
        else if constexpr (std::is_same_v<T, bool>) 
        {
            Encode_Bool(val);
        } // end else bool
        else if constexpr (std::is_same_v<T, std::string>) 
        {
            Encode_String(val);
        } // end else string
        else if constexpr (std::is_same_v<T, const char*>) 
        {
            Encode_String(val, len);
        } // end else string
        else if constexpr (std::is_same_v<T, std::vector<u8>>) 
        {
            Encode_Bytes(val);
        } // end else vector
        else if constexpr (std::is_same_v<T, BoltMessage>)
        {
            //std::cout << "Encode message" << std::endl;
            Encode_Message(val);
        } // end else encode message
        else if constexpr (std::is_same_v<T, BoltValue>)
        {
            // check the type
            switch (val.type)
            {
                /** basic primitives */
                case BoltType::Null:
                    Encode_Null();
                    break;

                case BoltType::Bool:
                    Encode_Bool(val.bool_val);
                    break;

                case BoltType::Int:
                    Encode_Int(val.int_val);
                    break;
                    
                case BoltType::Float:
                    Encode_Float(val.float_val);
                    break;

                case BoltType::String:
                    Encode_String(val.str_val.str, val.str_val.length);
                    break;

                case BoltType::Bytes:
                    Encode_Bytes(val);
                    break;

                /** compound primitives */
                case BoltType::List:
                    Encode_List(val);
                    break;

                case BoltType::Map:
                    Encode_Map(val);
                    break;

                case BoltType::Struct:
                    Encode_Struct(val);
                    break;
            } // end switch
        } // end else if
        else if (std::is_integral_v<T>) 
        {
            Encode_Int(val);
        } // end else int
        else if constexpr (std::is_same_v<T, double>)
        {
            Encode_Float(val);
        } // end else float
        else
        {
            std::cout << "Type: " << typeid(val).name() << "\n";
        } // end else

        return 0;
    } // end Encode

private:

    BoltBuf &buf;
    

    /**
     * @brief mirrors Encode, with the difference that this is meant to be called only once
     *  per encode and makes sure we have enough space to write to buffer. If space is enough
     *  simply returns alas, attempts to grow buffer.
     * 
     * @return true is space is available or false if buffer can't grow or val cant be encoded.
     */
    template <typename T>
    inline bool Has_Free(const T& val, const size_t len = 0)
    {
        size_t size = 0;
        if constexpr (std::is_same_v<T, std::nullptr_t>) size = 1;
        else if constexpr (std::is_same_v<T, bool>) size = 1;
        else if constexpr (std::is_same_v<T, std::string>)
        {
            size = val.length();
            if (size < 16) size += 1;
            else if (size < 256) size += 2;
            else if (size < 65536) size += 3;
            else size += 5;
        } // end else string
        else if constexpr (std::is_same_v<T, const char*>)
        {
            size = len;
            if (size < 16) size += 1;
            else if (size < 256) size += 2;
            else if (size < 65536) size += 3;
            else size += 5;
        } // end else string
        else if constexpr (std::is_same_v<T, std::vector<u8>>)
        {
            size = val.size();
            if (size < 255) size += 1;
            else if (size < 65536) size += 3;
            else size += 5;
        } // end else vector
        else if constexpr (std::is_same_v<T, BoltMessage>) size = val.msg.size + 4;
        else if constexpr (std::is_same_v<T, BoltValue>) size = val.size;
        else if (std::is_integral_v<T>)
        {
            if (val >= -16 && val <= 127) size = 1;
            else if (val >= INT8_MIN && val <= INT8_MAX) size = 2;
            else if (val >= INT16_MIN && val <= INT16_MAX) size = 3;
            else if (val >= INT32_MIN && val <= INT32_MAX) size = 5;
            else size = 9;
        } // end else int
        else if constexpr (std::is_same_v<T, double>) size = 9;
        else return false;

        // attempt to grow if 2x, if size is not enough
        if (size >= buf.Writable_Size())
            if (buf.Grow(buf.Capacity() << 1) < 0) return false;
      
        return true;
    } // end Has_Free


    /**
     * @brief encode's a null value for sending via bolt
     */
    inline void Encode_Null() 
    {
        Write_Bits<u8>(BOLT_NULL);
    } // end Encode_Null


    /**
     * @brief encode's the boolean value 
     * 
     * @param b the boolean
     */
    inline void Encode_Bool(bool b) 
    {
        Write_Bits<u8>((b ? BOLT_BOOL_TRUE : BOLT_BOOL_FALSE));
    } // end Encode_Bool


    /**
     * @brief encode's an integer value based on it's size
     * 
     * @param value the integer value to encode
     */
    inline void Encode_Int(s64 value) 
    {
        if (value >= -16 && value <= 127) 
        {
            Write_Bits<u8>(value);
        } // end if tiny
        else if (value >= INT8_MIN && value <= INT8_MAX) 
        {
            Write_Bits<u8>(BOLT_INT8);
            Write_Bits<s8>(static_cast<s8>(value));
        }  // end else if byte
        else if (value >= INT16_MIN && value <= INT16_MAX) 
        {
            Write_Bits<u8>(BOLT_INT16);
            Write_Bits<s16>(static_cast<s16>(htons(value)));
        } // end else if words
        else if (value >= INT32_MIN && value <= INT32_MAX) 
        {
            Write_Bits<u8>(BOLT_INT32);
            Write_Bits<s32>(static_cast<s32>(htonl(value)));
        } // end else if value 32-bits
        else 
        {
            Write_Bits<u8>(BOLT_INT64);
            Write_Bits<s64>(static_cast<s64>(htonll(value)));
        } // end else quad
    } // end Encode_Int


    /**
     * @brief encode's a float value
     * 
     * @param value the float value to encode
     */
    inline void Encode_Float(double value)
    {
        Write_Bits<u8>(BOLT_FLOAT64);
        value = swap_endian_double(value);
        iCpy(buf.Write_Ptr(), &value, sizeof(double));
        buf.Advance(sizeof(double));
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    } // end Encode_Float


    /**
     * @brief encode's an array of bytes.
     * 
     * @param bytes the bytes to write
     * @param len length of the bytes
     */
    inline void Encode_Bytes(const std::vector<u8> &bytes) 
    {
        size_t len = bytes.size();
        if (len <= 255) {
            Write_Bits<u16>((BOLT_BYTES8 << 8) | static_cast<u8>(len));
        } // end if <= 255
        else if (len <= 0xFFFF) {
            Write_Bits<u8>(BOLT_BYTES16);
            Write_Bits<u16>(static_cast<u16>(htons(len)));
        } // end else if 16-bits
        else 
        {
            Write_Bits<u8>(BOLT_BYTES32);
            Write_Bits<u32>(static_cast<u32>(htonl(len)));
        } // end else

        buf.Write(bytes.data(), len);
    } // end Encode bytes
    

    /**
     * @brief encode's an array of bytes.
     * 
     * @param val point to the start of the array
     */
    inline void Encode_Bytes(const BoltValue &val) 
    {
        size_t len = val.byte_val.size;
        if (len <= 255) {
            Write_Bits<u16>((BOLT_BYTES8 << 8) | static_cast<u8>(len));
        } // end if <= 255
        else if (len <= 0xFFFF) {
            Write_Bits<u8>(BOLT_BYTES16);
            Write_Bits<u16>(static_cast<u16>(htons(len)));
        } // end else if 16-bits
        else 
        {
            Write_Bits<u8>(BOLT_BYTES32);
            Write_Bits<u32>(static_cast<u32>(htonl(len)));
        } // end else

        buf.Write(val.byte_val.ptr, len);
    } // end Encode bytes

    
    /**
     * @brief encode's a neo4j string value (from C style chars)
     * 
     * @param str the string to encode
     * @param len length of the characters/string
     */
    inline void Encode_String(const char *str, const size_t len)
    {
        if (len <= 0x0F) 
        {
            Write_Bits<u8>((0x80 | static_cast<uint8_t>(len)));
        } // end if tiny string
        else if (len <= 0xFF)
        {
            Write_Bits<u16>((BOLT_STRING8 << 8) | static_cast<u8>(len));
        } // end else 8-bit
        else if (len <= 0xFFFF)
        {
            Write_Bits<u8>(BOLT_STRING16);
            Write_Bits<u16>(htons(len));
        } // end else if 16-bit
        else 
        {
            Write_Bits<u8>(BOLT_STRING32);
            Write_Bits<u32>(htonl(len));
        } // end else upto 32-bits

        buf.Write(reinterpret_cast<const u8*>(str), len);
    } // end Encode_String


    /**
     * @brief encode's a neo4j string value (overloaded)
     * 
     * @param str the string to encode
     */
    inline void Encode_String(const std::string &str) 
    {
        Encode_String(&str[0], str.length());
    } // end Encode_String


    /**
     * @brief encode's a list 
     * 
     * @param list to encode
     */
    inline void Encode_List(const BoltValue& list) 
    {
        size_t len = list.list_val.size;
        
        if (len <= 0x0F) 
        {
            Write_Bits<u8>(0x90 | static_cast<u8>(len));
        } // end if tiny
        else if (len <= 0xFF)
        {
            Write_Bits<u8>(BOLT_LIST8);
            Write_Bits<u8>(static_cast<u8>(len));
        } // end else if 8-bit
        else if (len <= 0xFFFF) 
        {
            Write_Bits<u8>(BOLT_LIST16);
            Write_Bits<u16>(static_cast<u16>(htons(len)));
        } // end else if 16-bit
        else 
        {
            Write_Bits<u8>(BOLT_LIST32);
            Write_Bits<u32>(static_cast<u32>(htonl(len)));
        } // end else

        for (int i{0}; i < len; i++) 
        {
            auto* p = list.pool->Get(list.list_val.offset + i);
            Encode(*p);
        } // end for
    } // end for


    /**
     * @brief encode's map/dicitionary objects
     */
    inline void Encode_Map(const BoltValue &map)
    {
        size_t count = map.map_val.size;
        if (count <= 15) 
        {
            Write_Bits<u8>(BOLT_MAPTINY | static_cast<u8>(count));
        } 
        else if (count <= 255)
        {
            Write_Bits<u8>(BOLT_MAP8);
            Write_Bits<u8>(static_cast<u8>(count));
        } 
        else if (count <= 0xFFFF) 
        {
            Write_Bits<u8>(BOLT_MAP16);
            Write_Bits<u16>(static_cast<u16>(count));
        } 
        else 
        {
            Write_Bits<u8>(BOLT_MAP32);
            Write_Bits<u32>(static_cast<u32>(count));
        }

        for (int i = 0; i < count; i++) 
        {
            auto* key = map.pool->Get(map.map_val.key_offset + i);
            auto* value = map.pool->Get(map.map_val.value_offset + i);
            Encode(*key);    // key
            Encode(*value);  // value
        } // end 
    } // end Encode_Map
    


    /**
     * @brief encode's struct elements
     */
    inline void Encode_Struct(const BoltValue &val) 
    {
        size_t len = val.struct_val.size;
        Write_Bits<u8>((BOLT_STRUCT | static_cast<u8>(len)));
        Write_Bits<u8>(val.struct_val.tag);

        for (int i = 0; i < len; i++)
        {
            auto* field = val.pool->Get(val.struct_val.offset + i);
            Encode(*field);
        } // end for 
    } // end Encode_Struct


    /**
     * @brief
     */
    inline void Encode_Message(const BoltMessage &msg)
    {
        buf.Skip(2);        // jump the first two bytes

        u8* start_addr = buf.Write_Ptr();
        Encode(msg.msg);
        u8* end_addr = buf.Write_Ptr();

        u16 size = static_cast<u16>((end_addr - start_addr)); 
        size = htons(size);

        iCpy(start_addr - 2, (const u8*)&size, sizeof(u16));
        buf.Write((const u8*)&msg.padding, sizeof(u16));
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    } // end Encode_Message


    /**
     * @brief write's a btyes into buffer
     * 
     * @param val the value to write
     */
    template<typename T>
    inline void Write_Bits(T val) 
    {
        if constexpr (sizeof(T) == 1) 
        {
            *reinterpret_cast<u8*>(buf.Write_Ptr()) = static_cast<u8>(val);
            buf.Advance(1);
        } // end if byte
        else if constexpr (sizeof(T) == 2) 
        {
            *reinterpret_cast<u16*>(buf.Write_Ptr()) = static_cast<u16>(htons(val));
            buf.Advance(2);
        } // end if word
        else if constexpr (sizeof(T) == 4) 
        {
            *reinterpret_cast<u32*>(buf.Write_Ptr()) = static_cast<u32>(htonl(val));
            buf.Advance(4);
        } // end if double word
        else if constexpr (sizeof(T) == 8) 
        {
            *reinterpret_cast<u64*>(buf.Write_Ptr()) = static_cast<u64>(htonll(val));
            buf.Advance(8);
        } // end if quad word
        else 
        {
            iCpy(buf.Write_Ptr(), &val, sizeof(T));
            buf.Advance(sizeof(T));
        } // end else
    } // end Write_Bits


    /**
     * @brief s string to append
     */
    inline void Append_Str(const std::string& s) 
    {
        Encode_String(s);
    } // end Append_Str
};