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
    void* bolt_pool;
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
                BoltValue* list;  // used for encoding 
                u8* ptr;          // used for decoding
            };
            bool is_decoded;
            size_t size;   
        } list_val;
        
        struct 
        { 
            BoltValue* key;     // used for encoding
            BoltValue* value;
            u8* ptr;            // used for decoding
            bool is_decoded;
            size_t size;   
        } map_val;

        struct 
        {
            u8 tag;
            union 
            {
                BoltValue* fields;  // during encoding
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
        auto& pool = GetBoltPool<BoltValue>();
        list_val.size = init.size();
        list_val.is_decoded = false;
        list_val.list = pool.Alloc(list_val.size);

        size_t i = 0;
        for (auto &v : init)
        {
            list_val.list[i++] = v;
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
        auto& pool = GetBoltPool<BoltValue>();

        map_val.size = init.size();
        map_val.is_decoded = false;
        BoltValue* mem = pool.Alloc(map_val.size << 1);
        map_val.key = mem;
        map_val.value = mem + map_val.size;

        for (auto& list : init)
        {
            map_val.key[i] = BoltValue(list.first);
            map_val.value[i++] = list.second;
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
        auto& pool = GetBoltPool<BoltValue>();
        struct_val.size = init.size();
        struct_val.is_decoded = false;
        struct_val.tag = tag;
        struct_val.fields = pool.Alloc(struct_val.size);

        size_t i = 0;
        for (auto &k : init)
            struct_val.fields[i++] = k;
    } // end BoltValue

    /**
     * @brief
     */
    // BoltValue(const BoltValue& other)
    // {
    //     type = other.type;
    //     if (other.type == BoltType::Bool)
    //     {
    //         bool_val = other.bool_val;
    //     } // end if bool
    //     else if (other.type == BoltType::Int)
    //     {
    //         int_val = other.int_val;
    //     } // end else int
    //     else if (other.type == BoltType::Float)
    //     {
    //         float_val = other.float_val;
    //     } // end else float
    //     else if (other.type == BoltType::String)
    //     {
    //         str_val.str = other.str_val.str;
    //         str_val.length = other.str_val.length;
    //     } // end else string  
    //     else if (other.type == BoltType::Bytes)
    //     {
    //         byte_val.ptr = other.byte_val.ptr;
    //         byte_val.size = other.byte_val.size;
    //     } // end else bytes
    //     else if (other.type == BoltType::List)
    //     {
    //         list_val.is_decoded = other.list_val.is_decoded;
    //         list_val.list = other.list_val.list;
    //         list_val.ptr = other.list_val.ptr;
    //         list_val.size = other.list_val.size;
    //     } // end else if list
    //     else if (other.type == BoltType::Map)
    //     {
    //         map_val.is_decoded = other.map_val.is_decoded;
    //         map_val.key = other.map_val.key;
    //         map_val.value = other.map_val.value;
    //         map_val.ptr = other.map_val.ptr;
    //         map_val.size = other.map_val.size;
    //     } // end else if map
    //     else if (other.type == BoltType::Struct)
    //     {
    //         struct_val.is_decoded = other.struct_val.is_decoded;
    //         struct_val.tag = other.struct_val.tag;
    //         struct_val.fields = other.struct_val.fields;
    //         struct_val.ptr = other.struct_val.ptr;
    //         struct_val.size = other.struct_val.size;
    //     } // end else if struct
    //     else
    //     {
    //         type = BoltType::Null;  // persume null
    //     } // end else null or unknown
    // } // end BoltValue 


    /**
     * @brief destructor
     */
    // ~BoltValue()
    // {
    //     Free_Bolt_Value(*this);
    // }


    // BoltValue* operator=(const BoltValue& other)
    // {
    //     type = other.type;
    //     if (other.type == BoltType::Bool)
    //     {
    //         bool_val = other.bool_val;
    //     } // end if bool
    //     else if (other.type == BoltType::Int)
    //     {
    //         int_val = other.int_val;
    //     } // end else int
    //     else if (other.type == BoltType::Float)
    //     {
    //         float_val = other.float_val;
    //     } // end else float
    //     else if (other.type == BoltType::String)
    //     {
    //         str_val.str = other.str_val.str;
    //         str_val.length = other.str_val.length;
    //     } // end else string  
    //     else if (other.type == BoltType::Bytes)
    //     {
    //         byte_val.ptr = other.byte_val.ptr;
    //         byte_val.size = other.byte_val.size;
    //     } // end else bytes
    //     else if (other.type == BoltType::List)
    //     {
    //         list_val.is_decoded = other.list_val.is_decoded;
    //         list_val.list = other.list_val.list;
    //         list_val.ptr = other.list_val.ptr;
    //         list_val.size = other.list_val.size;
    //     } // end else if list
    //     else if (other.type == BoltType::Map)
    //     {
    //         map_val.is_decoded = other.map_val.is_decoded;
    //         map_val.key = other.map_val.key;
    //         map_val.value = other.map_val.value;
    //         map_val.ptr = other.map_val.ptr;
    //         map_val.size = other.map_val.size;
    //     } // end else if map
    //     else if (other.type == BoltType::Struct)
    //     {
    //         struct_val.is_decoded = other.struct_val.is_decoded;
    //         struct_val.tag = other.struct_val.tag;
    //         struct_val.fields = other.struct_val.fields;
    //         struct_val.ptr = other.struct_val.ptr;
    //         struct_val.size = other.struct_val.size;
    //     } // end else if struct
    //     else
    //     {
    //         type = BoltType::Null;  // persume null
    //     } // end else null or unknown

    //     return this;
    // } // end BoltValue 

    BoltValue operator()(const size_t index)
    {
        size_t size;
        u8* ptr;
        BoltValue* alias;
        bool is_decoded;

        
        if (type == BoltType::Struct)
        { 
            size = struct_val.size;
            ptr = struct_val.ptr;
            alias = struct_val.fields;
            is_decoded = struct_val.is_decoded;
        } // end if
        else if (type == BoltType::List)
        {
            size = list_val.size;
            ptr = list_val.ptr;
            alias = list_val.list;
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
            if (index < size)
                return alias[index];
        } // end else
        
        return BoltValue::Make_Unknown();
    } // end operator


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
                for (size_t i = 0; i < map_val.size; i++)
                {
                    if (map_val.key[i].str_val.length == length && 
                        !strncmp(map_val.key[i].str_val.str, key, length))
                    {
                        return map_val.value[i];
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

    
    static BoltValue Make_List(const size_t size)
    {
        BoltValue v;
        v.type = BoltType::List;
        v.list_val.is_decoded = false;
        v.list_val.size = size;
        v.list_val.list = GetBoltPool<BoltValue>().Alloc(size);
        return v;
    } // end Make_List


    size_t current{0};
    void Append_List(BoltValue &val)
    {
        //std::cout << current << '\n';
        if (current < list_val.size)
        {
            list_val.list[current] = val;
            ++current;
        } // end if
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
            int_val = *((T*)ptr);
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
     * @brief 
     */
    static std::string ToString_Null(const BoltValue *ptr)
    {
        return "null";
    } // end ToString_Null


    /**
     * @brief
     */
    static std::string ToString_Bool(const BoltValue *ptr)
    {
        return (ptr->bool_val ? "true" : "false");
    } // end ToString_Bool


    /**
     * @brief
     */
    static std::string ToString_Int(const BoltValue *ptr)
    {
        return std::to_string(ptr->int_val);
    } // end ToString_Int


    /**
     * @brief
     */
    static std::string ToString_Float(const BoltValue* ptr)
    {
        return std::to_string(ptr->float_val);
    } // end ToString_Float


    /**
     * @brief
     */
    static std::string ToString_String(const BoltValue* ptr)
    {
        if (!ptr->str_val.str || ptr->str_val.length == 0)
            return "\"\"";
        return std::string(ptr->str_val.str, ptr->str_val.length);
    } // end ToString_String


    /**
     * @brief convert's a BoltValue Byte type as comma sperated string 
     *  list enclosed in square brackets, [].
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
    } // end Json_Bytes


    /**
     * @brief
     */
    static std::string ToString_List(const BoltValue *pval)
    {
        if (pval->list_val.size == 0)
            return "[]";

        std::string s = "[";
        if (!pval->list_val.is_decoded) 
        {
            for (size_t i = 0; i < pval->list_val.size; i++) 
            {
                s += pval->list_val.list[i].ToString();
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
     * @brief
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
            for (size_t i = 0; i < pval->map_val.size; i++) 
            {
                s += pval->map_val.key[i].ToString() + ":" +
                    pval->map_val.value[i].ToString();
                if (i != pval->map_val.size - 1)
                    s += ",";
            } // end for
        } // end else

        s += "}";
        return s;
    } // end ToString_Map


    /**
     * @brief
     */
    static std::string ToString_Struct(const BoltValue *pval)
    {
        if (pval->struct_val.size == 0)
            return "{}";

        std::string s = "{";
        if (pval->struct_val.is_decoded) 
        {
            u8* ptr = pval->struct_val.ptr;
            // if (pval->struct_val.tag == 0x4E)
            // {
            //     s += ToString_Node(ptr);
            // } // end else if Node
            // else if (pval->struct_val.tag == 0x58)
            // {
            //     s += ToString_Point2D(ptr);
            // } // end else if Point2D
            // else
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
            for (size_t i = 0; i < pval->struct_val.size; i++) 
            {
                s += pval->struct_val.fields[i].ToString();
                if (i != pval->struct_val.size - 1)
                    s += ",";
            } // end for
        } // end else not

        s += "}";
        return s;
    } // end ToString_Struct


    /**
     * @brief
     */
    static std::string ToString_Node(u8*& ptr)
    {
        BoltValue temp_id, temp_lables, temp_props, temp_elemid;
        std::string s = "Node:{id:";
        ptr += 2;   // bolt struct + point tags

        jump_table[*ptr](ptr, temp_lables);     // node list of labels
        jump_table[*ptr](ptr, temp_id);         // the  node id
        jump_table[*ptr](ptr, temp_props);      // node poperties
        jump_table[*ptr](ptr, temp_elemid);     // node element id

        s += std::to_string(temp_id.int_val) + ",lables:" +
            temp_lables.ToString() + ",Properties:" + temp_props.ToString() + 
            ",element_id:" + temp_elemid.ToString() + "}";
        return s;
    } // end ToString_Point2D


    /**
     * @brief
     */
    static std::string ToString_Point2D(u8*& ptr)
    {
        BoltValue tempi, tempx, tempy;
        std::string s = "Point2D:{srid:";
        ptr += 2;   // bolt struct + point tags

        jump_table[*ptr](ptr, tempi);    // integer value
        jump_table[*ptr](ptr, tempx);    // float x value
        jump_table[*ptr](ptr, tempy);    // float y value

        s += std::to_string(static_cast<u32>(tempi.int_val)) + ",x:" +
            tempx.ToString() + ",y:" + tempy.ToString() + "}";
        return s;
    } // end ToString_Point2D


    static std::string ToString_Unk(const BoltValue *pval)
    {
        return "<?>";
    } // end Unk


    std::string ToString() const
    {
        return str_jump[static_cast<u8>(type)](this);
    } // end ToString


    static void Free_Bolt_Value(BoltValue& val) 
    {
        if (val.type == BoltType::List && !val.list_val.is_decoded && val.list_val.list)
            GetBoltPool<BoltValue>().Release(val.list_val.size);

        else if (val.type == BoltType::Map && !val.map_val.is_decoded && val.map_val.key)
            GetBoltPool<BoltValue>().Release(val.map_val.size << 1);

        else if (val.type == BoltType::Struct && !val.struct_val.is_decoded && val.struct_val.fields)
            GetBoltPool<BoltValue>().Release(val.struct_val.size);
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