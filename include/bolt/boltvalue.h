/**
 * @file bolt_message.h
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief defintion of BoltValue that is an abstraction of neo4j types defined in
 *  boltprotocol using PackStream format. The goal here is to come up with a structure
 *  that abstracts neo4j types without sacrificing speed and making excessive copy.
 * 
 * @version 1.0
 * @date 16th of April 2025, Sunday.
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"
#include "bolt/boltvalue_pool.h"
#include "bolt/bolt_jump_table.h"





//===============================================================================|
//          GLOBALS
//===============================================================================|
#define mp  std::make_pair         // short-hand expression



/**
 * Noe4j Bolt PackStream types
 */
constexpr u8 BOLT_STRINGTINY   = 0x80;
constexpr u8 BOLT_LISTTINY     = 0x90;
constexpr u8 BOLT_MAPTINY      = 0xA0;
constexpr u8 BOLT_STRUCT       = 0xB0;
constexpr u8 BOLT_NULL         = 0xC0;
constexpr u8 BOLT_FLOAT64      = 0xC1;
constexpr u8 BOLT_BOOL_FALSE   = 0xC2;
constexpr u8 BOLT_BOOL_TRUE    = 0xC3;
constexpr u8 BOLT_INT8         = 0xC8;
constexpr u8 BOLT_INT16        = 0xC9;
constexpr u8 BOLT_INT32        = 0xCA;
constexpr u8 BOLT_INT64        = 0xCB;
constexpr u8 BOLT_BYTES8       = 0xCC;
constexpr u8 BOLT_BYTES16      = 0xCD;
constexpr u8 BOLT_BYTES32      = 0xCE;
constexpr u8 BOLT_STRING8      = 0xD0;
constexpr u8 BOLT_STRING16     = 0xD1;
constexpr u8 BOLT_STRING32     = 0xD2;
constexpr u8 BOLT_LIST8        = 0xD4;
constexpr u8 BOLT_LIST16       = 0xD5;
constexpr u8 BOLT_LIST32       = 0xD6;
constexpr u8 BOLT_MAP8         = 0xD8;
constexpr u8 BOLT_MAP16        = 0xD9;
constexpr u8 BOLT_MAP32        = 0xDA;
constexpr u8 BOLT_STRUCT8      = 0xDC;
constexpr u8 BOLT_STRUCT16     = 0xDD;


// bolt message types
constexpr u8 BOLT_HELLO         = 0x01;
constexpr u8 BOLT_GOODBYE       = 0x02;
constexpr u8 BOLT_ACK_FAILURE   = 0x0E;
constexpr u8 BOLT_RESET         = 0x0F;
constexpr u8 BOLT_RUN           = 0x10;
constexpr u8 BOLT_BEGIN         = 0x11;
constexpr u8 BOLT_COMMIT        = 0x12;
constexpr u8 BOLT_ROLLBACK      = 0x13;
constexpr u8 BOLT_DISCARD       = 0x2F;
constexpr u8 BOLT_PULL          = 0x3F;
constexpr u8 BOLT_TELEMETRY     = 0x54;
constexpr u8 BOLT_ROUTE         = 0x66;
constexpr u8 BOLT_LOGON         = 0x6A;
constexpr u8 BOLT_LOGOFF        = 0x6B;
constexpr u8 BOLT_SUCCESS       = 0x70;
constexpr u8 BOLT_RECORD        = 0x71;
constexpr u8 BOLT_IGNORED       = 0x7E;
constexpr u8 BOLT_FAILURE       = 0x7F;




//===============================================================================|
//          TYPES
//===============================================================================|
/**
 * Neo4j types:
 */
enum class BoltType : std::uint8_t 
{
    Null,
    Bool,
    Int,
    Float,
    String,
    Bytes,
    List,
    Map,
    Struct,
    Unk
};




//===============================================================================|
//          TYPES
//===============================================================================|
/**
 * @brief A union-based structure representing various Bolt protocol value types. It supports 
 *  multiple data types used in the Bolt protocol, including integers, floats, booleans, strings, 
 *  byte arrays, lists, maps, and structs. It utilizes a union to store these types efficiently, 
 *  along with a type identifier (BoltType) to determine the active member of the union.
 * 
 * - `type`: Specifies the data type of the value (e.g., Int, Bool, String).
 * - `padding`: Ensures proper alignment of the union members.
 * 
 * - The union contains fields for different value types:
 *   - `int_val`: A signed 64-bit integer value.
 *   - `float_val`: A double-precision floating point value.
 *   - `bool_val`: A boolean value.
 *   - `str_val`: A structure representing a string with a pointer and length.
 *   - `bytes_val`: A structure representing a byte array with a pointer and length.
 *   - `list_val`: A structure representing a list of Bolt values (heterogeneous).
 *   - `map_val`: A structure representing a map with key-value pairs (heterogeneous).
 *   - `struct_val`: A structure representing a Bolt struct with fields and a tag.
 * 
 * The structure supports several constructors and factory methods for creating Bolt values 
 *  from different types, including basic types (e.g., bool, int, double), strings, lists, maps, 
 *  and structs. It also provides a method for generating human-readable textual representations 
 *  of the value (`ToString`).
 */
struct BoltValue
{
    BoltType type;
    BoltPool<BoltValue>* pool = nullptr;  // pointer to the pool for memory management
    u8 padding[3];

    union 
    {
        s64 int_val;
        double float_val;
        bool bool_val;
  
        struct 
        {
            const char* str;
            size_t length;
        } str_val;

        struct 
        {
            u8* ptr;
            size_t size;
        } byte_val;

        struct 
        {
            union 
            {
                size_t offset;  // used for encoding 
                u8* ptr;          // used for decoding
            };
            bool is_decoded;
            size_t size;   
        } list_val;
        
        struct 
        { 
            size_t key_offset;      // used for encoding
            size_t value_offset;
            u8* ptr;                // used for decoding
            bool is_decoded;
            size_t size;   
        } map_val;

        struct 
        {
            u8 tag;
            union 
            {
                size_t offset;      // during encoding
                u8* ptr;            // while decoding
            };
            bool is_decoded;
            size_t size;
        } struct_val;
    };

    
    /* Constructors */
    
    /**
     * @brief
     */
    BoltValue() = default;


    /** 
     * @brief 
     */
    BoltValue(bool b)
    {
        type = BoltType::Bool;
        bool_val = b;
    } // end bool cntr


    /** 
     * @brief 
     */
    BoltValue(int i)
    {
        type = BoltType::Int;
        int_val = i;
    } // end int cntr


    /** 
     * @brief
     */
    BoltValue(s64 i)
    {
        type = BoltType::Int;
        int_val = i;
    } // end int cntr


    /** 
     * @brief
     */
    BoltValue(double d)
    {
        type = BoltType::Float;
        float_val = d;
    } // end double cntr


    /** 
     * @brief
     */
    BoltValue(const char* str)
    {
        type = BoltType::String;
        str_val.str = str;
        str_val.length = strlen(str);
    } // end char* cntr

    
    /**
     * @brief
     */
    BoltValue(const std::string& str)
    {
        type = BoltType::String;
        str_val.str = str.data();
        str_val.length = str.size();
    } // end string cntr


    /**
     * @brief initializer constructor for neo4j List types 
     *  with heterogeneous data.
     */
    BoltValue(std::initializer_list<BoltValue> init)
        : type(BoltType::List)
    {
        pool = GetBoltPool<BoltValue>();
        list_val.size = init.size();
        list_val.is_decoded = false;
        list_val.offset = pool->Alloc(list_val.size);

        size_t i = 0;
        for (auto &v : init)
        {
            *(pool->Get(list_val.offset + i)) = v;
            i++;
        } // end for
    } // end constructor


    /**
     * @brief initializer for noe4j dictionary types
     *  or what I like to call maps.
     */
    BoltValue(std::initializer_list<std::pair<const char*, BoltValue>> init)
        : type(BoltType::Map)
    {
        size_t i = 0;
        pool = GetBoltPool<BoltValue>();

        map_val.size = init.size();
        map_val.is_decoded = false;
        map_val.key_offset = pool->Alloc(map_val.size << 1);
        map_val.value_offset = map_val.key_offset + map_val.size;

        auto* key = pool->Get(map_val.key_offset);
        auto* value = pool->Get(map_val.value_offset);
        for (auto& list : init)
        {
            key[i] = BoltValue(list.first);
            value[i++] = list.second;
        } // end for 
    } // end initalizer


    /**
     * @brief initializer for neo4j struct types. All other compund
     *  types such as nodes and relataionships build on this struct. 
     * 
     * @param tag identifier/signature of the structure according
     *  neo4j bolt specs.
     */
    BoltValue(u8 tag, std::initializer_list<BoltValue> init)
        :type(BoltType::Struct)
    {
        pool = GetBoltPool<BoltValue>();
        struct_val.size = init.size();
        struct_val.is_decoded = false;
        struct_val.tag = tag;
        struct_val.offset = pool->Alloc(struct_val.size);

        size_t i = 0;
        auto *fields = pool->Get(struct_val.offset);
        for (auto &k : init)
            fields[i++] = k;
    } // end BoltValue


    /**
     * @brief destructor
     */
    ~BoltValue()
    {
        if (pool)
            Free_Bolt_Value(*this);
    } // end destructor


    BoltValue operator()(const size_t index)
    {
        size_t size;
        u8* ptr;
        size_t offset;
        bool is_decoded;

        
        if (type == BoltType::Struct)
        { 
            size = struct_val.size;
            ptr = struct_val.ptr;
            offset = struct_val.offset;
            is_decoded = struct_val.is_decoded;
        } // end if
        else if (type == BoltType::List)
        {
            size = list_val.size;
            ptr = list_val.ptr;
            offset = list_val.offset;
            is_decoded = struct_val.is_decoded;
        } // end else if

        if (is_decoded)
        {
            size_t i = 0;
            BoltValue v;
            for (i = 0; i < size; i++)
            {
                jump_table[*ptr](ptr, v);
                if (i == index)
                    return v;
            } // end while
        } // end if decoded
        else 
        {
            if (pool)
            {
                BoltValue* alias = pool->Get(offset);
                if (index >= size)
                    return BoltValue::Make_Unknown();
                
                //std::cout << "alias: " << alias[index].ToString() << '\n';
                return alias[index];
            } // end if pool
            
            //std::cout << "No pool\n";
            return BoltValue::Make_Unknown();
        } // end else
        
        return BoltValue::Make_Unknown();
    } // end operator


    /**
     * @brief operator overloading for map types
     */
    BoltValue operator[](const char *key)
    {
        size_t length = strlen(key);
        if (type == BoltType::Map)
        {
            if (map_val.is_decoded)
            {
                u8* ptr = map_val.ptr;
                for (size_t i = 0; i < map_val.size; i++)
                {
                    BoltValue out_key; 
                    BoltValue out_val; 

                    jump_table[*ptr](ptr, out_key);
                    jump_table[*ptr](ptr, out_val);
                    
                    if (out_key.ToString().length() == length && 
                        !strncmp(out_key.ToString().c_str(), key, length))
                    {
                        //std::cout << std::string(out_val.str_val.str, out_val.str_val.length) << '\n';
                        return out_val;
                    }
                } // end if decoded
            } // end if is decoded
            else 
            {
                auto* k = pool->Get(map_val.key_offset);
                auto* v = pool->Get(map_val.value_offset);

                for (size_t i = 0; i < map_val.size; i++)
                {
                    if (k[i].str_val.length == length && 
                        !strncmp(k[i].str_val.str, key, length))
                    {
                        return v[i];
                    } // end if
                } // end for
            } // end else not
        } // end if map type

        return BoltValue::Make_Unknown();
    } // end operator[]


    /* Small factories */

    /**
     * @brief factory for producing Null types at will
     */
    static BoltValue Make_Null() 
    {
        BoltValue v;
        v.type = BoltType::Null;
        return v;
    } // end Make_Null


    /**
     * @brief factory to produce boolean
     * 
     * @param b the boolean value to set
     */
    static BoltValue Make_Bool(bool b)
    {
        BoltValue v;
        v.type = BoltType::Bool;
        v.bool_val = b;
        return v;
    } // end Make_Int


    /**
     * @brief factory for integer values.
     * 
     * @param v a 64-bit signed integer value
     */
    static BoltValue Make_Int(s64 v)
    {
        BoltValue bv;
        bv.type = BoltType::Int;
        bv.int_val = v;
        return bv;
    } // end Make_Int

    
    /**
     * @brief factory for float/double values.
     * 
     * @param v the 64-bit signed float/double value
     */
    static BoltValue Make_Float(double f)
    {
        BoltValue v;
        v.type = BoltType::Float;
        v.float_val = f;
        return v;
    } // end Make_Float


    /**
     * @brief factory for Bytes. Which are really arrays of byte values
     *  like ASCII encoded strings; for claritiy simply classified as bytes 
     * 
     * @param ptr address of the byte data/array in the buffer
     * @param len the length of the buffer
     */
    static BoltValue Make_Bytes(u8* ptr, const u32 len)
    {
        BoltValue v;
        v.type = BoltType::Bytes;
        v.byte_val.ptr = ptr;
        v.byte_val.size = len;
        return v;
    } // end Make_Bytes


    /**
     * @brief factory of strings
     * 
     * @param ptr pointer to the start of the byte address
     * @param len the length of the buffer
     */
    static BoltValue Make_String(const char* str, u32 len)
    {
        BoltValue v;
        v.type = BoltType::String;
        v.str_val.str = str;
        v.str_val.length = len;
        return v;
    } // end Make_String


    /**
     * @brief factory for hetero-lists (lazy decoding style)
     * 
     * @param data_ptr start of buffer containing bolt encoded values
     * @param len the length of the buffer
     */
    static BoltValue Make_List(u8* data_ptr, const size_t len)
    {
        BoltValue v;
        v.type = BoltType::List;
        v.list_val.is_decoded = true;
        v.list_val.ptr = data_ptr;
        v.list_val.size = len;
        return v;
    } // end Make_List


    /**
     * @brief factory for hetero-lists (lazy decoding style)
     *  with a size hint.
     * 
     * @param size the size of the list
     */
    static BoltValue Make_List(const size_t size)
    {
        BoltValue v;
        v.type = BoltType::List;
        v.pool = GetBoltPool<BoltValue>();
        v.list_val.is_decoded = false;
        v.list_val.size = size;
        v.list_val.offset = v.pool->Alloc(size);
        return v;
    } // end Make_List


    size_t current{0};
    void Append_List(BoltValue &val)
    {
        //std::cout << current << '\n';
        // if (current < list_val.size)
        // {
        //     list_val.list[current] = val;
        //     ++current;
        // } // end if
    } // end Append_List


    /**
     * @brief factory for maps
     * 
     * @param offset into the pool
     * @param len count of key value pairs
     */
    static BoltValue Make_Map(u8* ptr, const size_t len)
    {
        BoltValue v;
        v.type = BoltType::Map;
        v.map_val.is_decoded = true;
        v.map_val.ptr = ptr;
        v.map_val.size = len;
        return v;
    } // end Make_List


    /**
     * @brief factor for maps
     */
    static BoltValue Make_Map()
    {
        BoltValue v;
        v.type = BoltType::Map;
        v.map_val.is_decoded = false;
        v.map_val.ptr = nullptr;
        v.map_val.size = 0;
        v.map_val.key_offset = 0;
        v.map_val.value_offset = 0;
        return v;
    } // end Make_List


    /**
     * @brief factory of structs
     * 
     * @param u8 the structure tag (see bolt packstream)
     * @param fields a pointer to fields to bind
     * @param len the count of fields
     */
    static BoltValue Make_Struct(u8* ptr, const u8 tag, const size_t len)
    {
        BoltValue v;
        v.type = BoltType::Struct;
        v.struct_val.size = len;
        v.struct_val.ptr = ptr;
        v.struct_val.is_decoded = true;
        v.struct_val.tag = tag;
        return v;
    } // end Make_Struct


    /**
     * @brief unknown factory
     */
    static BoltValue Make_Unknown()
    {
        BoltValue v;
        v.type = BoltType::Unk;
        return v;
    } // end Make_Unknown


    /**
     * @brief used to decode integer values from the buffer and ajust the endianess
     *  according to the machine.
     */
    template<typename T>
    void Set_Int_RawDirect(u8* ptr)
    {
        type = BoltType::Int;
        if constexpr (is_big_endian) 
        {
            int_val = *((T*)ptr);
            // T tmp;
            // iCpy(&tmp, ptr, sizeof(T));
            // tmp = byte_swap<T>(tmp); 
            // int_val = tmp;
        } // end if byte
        else 
        {
            iCpy(&int_val, ptr, sizeof(T));
            int_val = byte_swap<T>(int_val);
        } // end else little
    } // end Set_Int_RawDirect


    /**
     * @brief attempts to get the value supported if not compound alas
     *  return's a string representation based on type
     */
    // template<typename T>
    // T Get() const 
    // {
    //     if constexpr(std::is_same_v(T, nullptr_t))
    //         return Make_Null();
    //     if constexpr (std::is_same_v(T, bool))
    //         return bool_val;
    //     else if constexpr (std::is_same_v(T, int))
    //         return int_val;
    //     else if constexpr (std::is_same_v(T, double))
    //         return float_val;
    //     else if constexpr (std::is_same_v(T, const char*))
    //         return std::string(str_val.ptr, str_val.len);
    //     else if constexpr (std::is_same_v(T, std::string))
    //         return std::string(str_val.ptr, str_val.len);
    //     else
    //         return str_jump[(int)type](this);
    // } // end else


    /**
     * @brief Converts a BoltValue null to a human-readable string representation.
     *  The null value is represented as "null".
     * 
     * @param ptr Pointer to the BoltValue representing the null value.
     * @return std::string representing the BoltValue null.
     */
    static std::string ToString_Null(const BoltValue *ptr)
    {
        return "null";
    } // end ToString_Null


    /**
     * @brief Converts a BoltValue boolean to a human-readable string representation.
     *  The boolean is represented as "true" or "false".
     * 
     * @param ptr Pointer to the BoltValue representing the boolean.
     * @return std::string representing the BoltValue boolean.
     */
    static std::string ToString_Bool(const BoltValue *ptr)
    {
        return (ptr->bool_val ? "true" : "false");
    } // end ToString_Bool


    /**
     * @brief Converts a BoltValue integer to a human-readable string representation.
     *  The integer is represented as a decimal number.
     * 
     * @param ptr Pointer to the BoltValue representing the integer.
     * @return std::string representing the BoltValue integer.
     */
    static std::string ToString_Int(const BoltValue *ptr)
    {
        return std::to_string(ptr->int_val);
    } // end ToString_Int


    /**
     * @brief Converts a BoltValue float to a human-readable string representation.
     *  The float is represented as a decimal number.
     * 
     * @param ptr Pointer to the BoltValue representing the float.
     * @return std::string representing the BoltValue float.
     */
    static std::string ToString_Float(const BoltValue* ptr)
    {
        return std::to_string(ptr->float_val);
    } // end ToString_Float


    /**
     * @brief Converts a BoltValue string to a human-readable string representation.
     *  The string is represented as a quoted string.
     * 
     * @param ptr Pointer to the BoltValue representing the string.
     * @return std::string representing the BoltValue string.
     */
    static std::string ToString_String(const BoltValue* ptr)
    {
        if (!ptr->str_val.str || ptr->str_val.length == 0)
            return "\"\"";
        return std::string(ptr->str_val.str, ptr->str_val.length);
    } // end ToString_String


    /**
     * @brief Converts a BoltValue byte array to a human-readable string representation.
     *  The byte array is represented as a comma-separated list of hexadecimal values enclosed in square brackets.
     * 
     * @param ptr Pointer to the BoltValue representing the byte array.
     * @return std::string representing the BoltValue byte array.
     */
    static std::string ToString_Bytes(const BoltValue* ptr)
    {
        if (ptr->byte_val.size == 0)
            return "[]";
            
        std::ostringstream stream;
        for (size_t i = 0; i < ptr->byte_val.size; i++) 
        {
                stream << "0x"
                    << std::hex << std::uppercase
                    << std::setw(2) << std::setfill('0')
                    << static_cast<int>(ptr->byte_val.ptr[i]);
                if (i != ptr->byte_val.size - 1)
                    stream << ",";
        }
        stream << "]";
        return stream.str();
    } // end ToString_Bytes


    /**
     * @brief Converts a BoltValue list to a human-readable string representation.
     *  The list is represented as a comma-separated list of values enclosed in square brackets.
     * 
     * @param pval Pointer to the BoltValue representing the list.
     * @return std::string representing the BoltValue list.
     */
    static std::string ToString_List(const BoltValue *pval)
    {
        if (pval->list_val.size == 0)
            return "[]";

        std::string s = "[";
        if (!pval->list_val.is_decoded) 
        {
            auto* v = pval->pool->Get(pval->list_val.offset);
            for (size_t i = 0; i < pval->list_val.size; i++) 
            {
                s += v[i].ToString();
                if (i != pval->list_val.size - 1)
                    s += ",";
            } // end for
        } // end if decoded
        else 
        {
            u8* ptr = pval->list_val.ptr;
            for (size_t i = 0; i < pval->list_val.size; i++) 
            {
                BoltValue v;
                jump_table[*ptr](ptr, v);
                s += v.ToString();
                if (i != pval->list_val.size - 1)
                    s += ",";
            }
        } // end else not

        s += "]";
        return s;
    } // end ToString_List


    /**
     * @brief Converts a BoltValue map to a human-readable string representation.
     *  The map is represented as a comma-separated list of key-value pairs enclosed in curly braces.
     * 
     * @param pval Pointer to the BoltValue representing the map.
     * @return std::string representing the BoltValue map.
     */
    static std::string ToString_Map(const BoltValue *pval)
    {
        if (pval->map_val.size == 0)
            return "{}";

        std::string s = "{";
        if (pval->map_val.is_decoded) 
        {
            u8* ptr = pval->map_val.ptr;
            for (size_t i = 0; i < pval->map_val.size; i++) 
            {
                BoltValue k; 
                BoltValue v; 
                jump_table[*ptr](ptr, k);
                jump_table[*ptr](ptr, v);
                s += k.ToString() + ":" + v.ToString();
                if (i != pval->map_val.size - 1)
                    s += ",";
            } // end for
        } // end if decoded
        else 
        {
            auto* key = pval->pool->Get(pval->map_val.key_offset);
            auto* value = pval->pool->Get(pval->map_val.value_offset);
            for (size_t i = 0; i < pval->map_val.size; i++) 
            {
                s += key[i].ToString() + ":" +
                    value[i].ToString();
                if (i != pval->map_val.size - 1)
                    s += ",";
            } // end for
        } // end else

        s += "}";
        return s;
    } // end ToString_Map


    /**
     * @brief Converts a BoltValue struct to a human-readable string representation.
     *  The struct is represented as a comma-separated list of fields enclosed in curly braces.
     * 
     * @param pval Pointer to the BoltValue representing the struct.
     * @return std::string representing the BoltValue struct.
     */
    static std::string ToString_Struct(const BoltValue *pval)
    {
        if (pval->struct_val.size == 0)
            return "{}";

        std::string s = "{";
        if (pval->struct_val.is_decoded) 
        {
            u8* ptr = pval->struct_val.ptr;
            if (pval->struct_val.tag == 0x4E)
            {
                s += ToString_Node(ptr);
            } // end else if Node
            else if (pval->struct_val.tag == 0x52)
            {
                s += ToString_Relationship(ptr);
            } // end else if Relationship
            else if (pval->struct_val.tag == 0x72)
            {
                s += ToString_Unbound_Relationship(ptr);
            } // end else if UnboundRelationship
            else if (pval->struct_val.tag == 0x50)
            {
                s += ToString_Path(ptr);
            } // end else if Path
            else if (pval->struct_val.tag == 0x44)
            {
                s += ToString_Date(ptr);
            } // end else if Date
            else if (pval->struct_val.tag == 0x54)
            {
                s += ToString_Time(ptr);
            } // end else if Time
            else if (pval->struct_val.tag == 0x74)
            {
                s += ToString_LocalTime(ptr);
            } // end else if LocalTime
            else if (pval->struct_val.tag == 0x49 || pval->struct_val.tag == 0x46)
            {
                s += ToString_DateTime(ptr);
            } // end else if DateTime
            else if (pval->struct_val.tag == 0x69 || pval->struct_val.tag == 0x66)
            {
                s += ToString_DateTimeTimeZoneId(ptr);
            } // end else if DateTimeTimeZoneId
            else if (pval->struct_val.tag == 0x64)
            {
                s += ToString_LocalDateTime(ptr);
            } // end else if LocalDateTime
            else if (pval->struct_val.tag == 0x45)
            {
                s += ToString_Duration(ptr);
            } // end else if Duration
            else if (pval->struct_val.tag == 0x58)
            {
                s += ToString_Point2D(ptr);
            } // end else if Point2D
            else if (pval->struct_val.tag == 0x59)
            {
                s += ToString_Point3D(ptr);
            } // end else if Point3D
            else
            {
                for (size_t i = 0; i < pval->struct_val.size; i++) 
                {
                    BoltValue v; 
                    jump_table[*ptr](ptr, v);
                    s += v.ToString();
                        
                    if (i != pval->struct_val.size - 1)
                        s += ",";
                } // end for
            } // end else
        } // end if decoded
        else 
        {
            auto* fields = pval->pool->Get(pval->struct_val.offset);
            for (size_t i = 0; i < pval->struct_val.size; i++) 
            {
                s += fields[i].ToString();
                if (i != pval->struct_val.size - 1)
                    s += ",";
            } // end for
        } // end else not

        s += "}";
        return s;
    } // end ToString_Struct


    /**
     * @brief Converts a BoltValue Node to a human-readable string representation.
     *  The Node is represented with its ID, labels, properties, and element ID.
     * 
     * @param ptr Pointer to the BoltValue representing the Node.
     * 
     * @return std::string representing the BoltValue Node.
     */
    static std::string ToString_Node(u8*& ptr)
    {

        BoltValue temp_id, temp_lables, temp_props, temp_elemid;
        std::string s = "Node:{id:";

        jump_table[*ptr](ptr, temp_id);         // the  node id
        jump_table[*ptr](ptr, temp_lables);     // node list of labels
        jump_table[*ptr](ptr, temp_props);      // node poperties
        jump_table[*ptr](ptr, temp_elemid);     // node element id

        s += std::to_string(temp_id.int_val) + ",lables:" +
            temp_lables.ToString() + ",Properties:" + temp_props.ToString() + 
            ",element_id:" + temp_elemid.ToString() + "}";
        return s;
    } // end ToString_Point2D


    /**
     * @brief Converts a BoltValue Relationship to a human-readable string representation.
     *  The Relationship is represented with its ID, start node ID, end node ID, type, properties,
     *  element ID, and start/end node element IDs.
     * 
     * @param ptr Pointer to the BoltValue representing the Relationship.
     * 
     * @return std::string representing the BoltValue Relationship.
     */
    static std::string ToString_Relationship(u8*& ptr)
    {
        BoltValue temp_id, temp_start, temp_end, temp_type, temp_props, temp_elemid,
            temp_start_elemid, temp_end_elemid;

        std::string s = "Relationship:{id:";

        jump_table[*ptr](ptr, temp_id);         // the relationship id
        jump_table[*ptr](ptr, temp_start);      // start node id
        jump_table[*ptr](ptr, temp_end);        // end node id
        jump_table[*ptr](ptr, temp_type);       // relationship type
        jump_table[*ptr](ptr, temp_props);      // relationship properties
        jump_table[*ptr](ptr, temp_elemid);     // relationship element id
        jump_table[*ptr](ptr, temp_start_elemid); // start node element id
        jump_table[*ptr](ptr, temp_end_elemid);   // end node element id

        s += std::to_string(temp_id.int_val) + ",startNode:" +
            std::to_string(temp_start.int_val) + ",endNode:" +
            std::to_string(temp_end.int_val) + ",type:" +
            temp_type.ToString() + ",Properties:" + 
            temp_props.ToString() + ",element_id:" + 
            temp_elemid.ToString() + 
            ",startNodeElementId:" + temp_start_elemid.ToString() +
            ",endNodeElementId:" + temp_end_elemid.ToString() + "}";
        return s;
    } // end ToString_Relationship


    /**
     * @brief Converts a BoltValue UnboundRelationship to a human-readable string representation.
     *  The UnboundRelationship is represented with its ID, type, properties, and element ID.
     * 
     * @param ptr Pointer to the BoltValue representing the UnboundRelationship.
     * 
     * @return std::string representing the BoltValue UnboundRelationship.
     */
    static std::string ToString_Unbound_Relationship(u8*& ptr)
    {
        BoltValue temp_id, temp_type, temp_props, temp_elemid;
        std::string s = "UnboundRelationship:{id:";

        jump_table[*ptr](ptr, temp_id);         // the relationship id
        jump_table[*ptr](ptr, temp_type);       // relationship type
        jump_table[*ptr](ptr, temp_props);      // relationship properties
        jump_table[*ptr](ptr, temp_elemid);     // relationship element id

        s += std::to_string(temp_id.int_val) + ",type:" +
            temp_type.ToString() + ",Properties:" + 
            temp_props.ToString() + ",element_id:" + 
            temp_elemid.ToString() + "}";
        return s;
    } // end ToString_Unbound_Relationship


    /**
     * @brief Converts a BoltValue Path to a human-readable string representation.
     *  The Path is represented with its nodes, relationships, and indices.
     * 
     * @param ptr Pointer to the BoltValue representing the Path.
     * 
     * @return std::string representing the BoltValue Path.
     */
    static std::string ToString_Path(u8*& ptr)
    {
        BoltValue temp_nodes, temp_rels, temp_index;
        std::string s = "Path:{nodes:";

        jump_table[*ptr](ptr, temp_nodes);      // nodes
        jump_table[*ptr](ptr, temp_rels);       // relationships
        jump_table[*ptr](ptr, temp_index);      // idices
        

        s += temp_nodes.ToString() + ",relationships:" +
            temp_rels.ToString() + ",indices:" +
            temp_index.ToString() + "}";
        return s;
    } // end ToString_Path


    /**
     * @brief Converts a BoltValue Date to a human-readable string representation.
     *  The Date is represented with its days since epoch Jan 01 1970.
     * 
     * @param ptr Pointer to the BoltValue representing the Date.
     * 
     * @return std::string representing the BoltValue Date.
     */
    static std::string ToString_Date(u8*& ptr)
    {
        BoltValue temp_day;
        std::string s = "Date:{days:";

        jump_table[*ptr](ptr, temp_day);        // day

        s += std::to_string(temp_day.int_val) + "}";
        return s;
    } // end ToString_Date


    /**
     * @brief Converts a BoltValue Time to a human-readable string representation.
     *  The Time is represented with its nanoseconds and timezone offset in seconds.
     * 
     * @param ptr Pointer to the BoltValue representing the Time.
     * 
     * @return std::string representing the BoltValue Time.
     */
    static std::string ToString_Time(u8*& ptr)
    {
        BoltValue temp_nanosecond, temp_offset_second;
        std::string s = "Time:{nanoseconds:";

        jump_table[*ptr](ptr, temp_nanosecond);       // hour
        jump_table[*ptr](ptr, temp_offset_second);     // minute

        s += std::to_string(temp_nanosecond.int_val) + ",tz_offset_second:" +
            std::to_string(temp_offset_second.int_val)  + "}";
        return s;
    } // end ToString_Time


    /**
     * @brief Converts a BoltValue LocalTime to a human-readable string representation.
     *  The LocalTime is represented with its nanoseconds since midnight.
     * 
     * @param ptr Pointer to the BoltValue representing the LocalTime.
     * 
     * @return std::string representing the BoltValue LocalTime.
     */
    static std::string ToString_LocalTime(u8*& ptr)
    {
        BoltValue temp_nanosecond;
        std::string s = "LocalTime:{nanoseconds:";

        jump_table[*ptr](ptr, temp_nanosecond);       // hour

        s += std::to_string(temp_nanosecond.int_val) + "}";
        return s;
    } // end ToString_LocalTime


    /**
     * @brief Converts a BoltValue DateTime to a human-readable string representation.
     *  The DateTime is represented with its seconds since epoch, nanoseconds, and timezone offset in seconds.
     * 
     * @param ptr Pointer to the BoltValue representing the DateTime.
     * 
     * @return std::string representing the BoltValue DateTime.
     */
    static std::string ToString_DateTime(u8*& ptr)
    {
        BoltValue temp_seconds, temp_nanoseconds, temp_offset_seconds;
        std::string s = "DateTime:{seconds:";

        jump_table[*ptr](ptr, temp_seconds);            // date
        jump_table[*ptr](ptr, temp_nanoseconds);        // time
        jump_table[*ptr](ptr, temp_offset_seconds);     // time

        s += temp_seconds.ToString() + ",nanoseconds:" + 
            temp_nanoseconds.ToString() + ",tz_offset_seconds:" + 
            temp_offset_seconds.ToString() + "}";
        return s;
    } // end ToString_DateTime


    /**
     * @brief Converts a BoltValue DateTimeTimeZoneId to a human-readable string representation.
     *  The DateTimeTimeZoneId is represented with its seconds since epoch, nanoseconds, and timezone ID.
     * 
     * @param ptr Pointer to the BoltValue representing the DateTimeTimeZoneId.
     * 
     * @return std::string representing the BoltValue DateTimeTimeZoneId.
     */
    static std::string ToString_DateTimeTimeZoneId(u8*& ptr)
    {
        BoltValue temp_seconds, temp_nanoseconds, temp_id;
        std::string s = "DateTime:{seconds:";

        jump_table[*ptr](ptr, temp_seconds);            // date
        jump_table[*ptr](ptr, temp_nanoseconds);        // time
        jump_table[*ptr](ptr, temp_id);                 // id

        s += temp_seconds.ToString() + ",nanoseconds:" + 
            temp_nanoseconds.ToString() + ",tz_id:" + 
            temp_id.ToString() + "}";
        return s;
    } // end ToString_DateTimeTimeZoneId


    /**
     * @brief Converts a BoltValue LocalDateTime to a human-readable string representation.
     *  The LocalDateTime is represented with its seconds since epoch and nanoseconds.
     * 
     * @param ptr Pointer to the BoltValue representing the LocalDateTime.
     * 
     * @return std::string representing the BoltValue LocalDateTime.
     */
    static std::string ToString_LocalDateTime(u8*& ptr)
    {
        BoltValue temp_seconds, temp_nanoseconds;
        std::string s = "LocalDateTime:{seconds:";

        jump_table[*ptr](ptr, temp_seconds);            // date
        jump_table[*ptr](ptr, temp_nanoseconds);        // time

        s += temp_seconds.ToString() + ",seconds:" + 
            temp_nanoseconds.ToString() + "}";
        return s;
    } // end ToString_LocalDateTime


    /**
     * @brief Converts a BoltValue Duration to a human-readable string representation.
     *  The Duration is represented with its months, days, seconds, and nanoseconds.
     * 
     * @param ptr Pointer to the BoltValue representing the Duration.
     * 
     * @return std::string representing the BoltValue Duration.
     */
    static std::string ToString_Duration(u8*& ptr)
    {
        BoltValue temp_months, temp_days, temp_seconds, temp_nanoseconds;
        std::string s = "Duration:{months:";

        jump_table[*ptr](ptr, temp_months);            // months
        jump_table[*ptr](ptr, temp_days);              // days
        jump_table[*ptr](ptr, temp_seconds);           // seconds
        jump_table[*ptr](ptr, temp_nanoseconds);       // nanoseconds

        s += std::to_string(temp_months.int_val) + ",days:" +
            std::to_string(temp_days.int_val) + ",seconds:" +
            std::to_string(temp_seconds.int_val) + ",nanoseconds:" +
            std::to_string(temp_nanoseconds.int_val) + "}";
        return s;
    } // end ToString_Duration


    /**
     * @brief Converts a BoltValue Point2D to a human-readable string representation.
     *  The Point2D is represented with its SRID, x, and y coordinates.
     * 
     * @param ptr Pointer to the BoltValue representing the Point2D.
     * 
     * @return std::string representing the BoltValue Point2D.
     */
    static std::string ToString_Point2D(u8*& ptr)
    {
        BoltValue tempi, tempx, tempy;
        std::string s = "Point2D:{srid:";

        jump_table[*ptr](ptr, tempi);    // integer value
        jump_table[*ptr](ptr, tempx);    // float x value
        jump_table[*ptr](ptr, tempy);    // float y value

        s += std::to_string(static_cast<u32>(tempi.int_val)) + ",x:" +
            tempx.ToString() + ",y:" + tempy.ToString() + "}";
        return s;
    } // end ToString_Point2D


    /**
     * @brief Converts a BoltValue Point3D to a human-readable string representation.
     *  The Point3D is represented with its SRID, x, y, and z coordinates.
     * 
     * @param ptr Pointer to the BoltValue representing the Point3D.
     * 
     * @return std::string representing the BoltValue Point3D.
     */
    static std::string ToString_Point3D(u8*& ptr)
    {
        BoltValue tempi, tempx, tempy, tempz;
        std::string s = "Point3D:{srid:";

        jump_table[*ptr](ptr, tempi);    // integer value
        jump_table[*ptr](ptr, tempx);    // float x value
        jump_table[*ptr](ptr, tempy);    // float y value
        jump_table[*ptr](ptr, tempz);    // float z value

        s += std::to_string(static_cast<u32>(tempi.int_val)) + ",x:" +
            tempx.ToString() + ",y:" + tempy.ToString() + ",z:" + tempz.ToString() + "}";
        return s;
    } // end ToString_Point3D



    /**
     * @brief
     */
    static std::string ToString_Unk(const BoltValue *pval)
    {
        return "<?>";
    } // end Unk


    /**
     * @brief Converts the BoltValue to a human-readable string representation.
     *  The actual string representation depends on the type of the value.
     * 
     * @return std::string representing the BoltValue.
     */
    std::string ToString() const
    {
        return str_jump[static_cast<u8>(type)](this);
    } // end ToString


    /**
     * @brief Frees the BoltValue from the pool if it is not decoded.
     *  This is used to release resources when the value is no longer needed.
     * 
     * @param val The BoltValue to free.
     */
    static void Free_Bolt_Value(BoltValue& val) 
    {
        if (val.type == BoltType::List && !val.list_val.is_decoded)
            val.pool->Release();

        else if (val.type == BoltType::Map && !val.map_val.is_decoded)
            val.pool->Release();

        else if (val.type == BoltType::Struct && !val.struct_val.is_decoded )
            val.pool->Release();
    } // end FreeBoltValue


    // jump table for to_string parsing based on type
    using ToStr = std::string (*)(const BoltValue *);
    static constexpr ToStr str_jump[10] = {
        &ToString_Null,
        &ToString_Bool,
        &ToString_Int,
        &ToString_Float,
        &ToString_String,
        &ToString_Bytes,
        &ToString_List,
        &ToString_Map,
        &ToString_Struct,
        &ToString_Unk
    };
};