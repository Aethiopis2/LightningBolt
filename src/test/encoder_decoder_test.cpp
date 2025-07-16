/**
 * @file main.cpp
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 * 
 * @brief stress testing bolt encoder and decoder speeds
 * @version 1.2
 * @date 9th of April 2025, Wednesday
 * 
 * @copyright Copyright (c) 2025
 * 
 */



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "bolt/bolt_encoder.h"
#include "bolt/bolt_decoder.h"
#include "bolt/boltvalue_pool.h"



#include "utils/utils.h"
#include "utils/errors.h"

#include <assert.h>
#include <random>
using namespace std;




//===============================================================================|
//          FUNCTIONS
//===============================================================================|
// SYS_CONFIG sys_config;
// int daemon_proc = 0;




//===============================================================================|
//          FUNCTIONS
//===============================================================================|
using Clock = std::chrono::high_resolution_clock;
using ns = std::chrono::nanoseconds;

template<typename F>
double Benchmark(F&& fn, size_t iterations)
{
    auto start = Clock::now();
    for (size_t i = 0; i < iterations; ++i)
        fn();
    auto end = Clock::now();
    ns dur = std::chrono::duration_cast<ns>(end - start);
    return static_cast<double>(dur.count()) / iterations;
}

void PrintResult(const std::string& name, double avg_ns)
{
    double throughput = 1e9 / avg_ns;
    std::cout << std::left << std::setw(15) << name
              << std::right << std::setw(20) << avg_ns
              << std::setw(25) << throughput << "\n";
}


void Encode_Test(size_t iterations)
{
    std::cout << "Encoding Benchmark (" << iterations << " iterations)\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << std::left << std::setw(15) << "Operation"
              << std::right << std::setw(20) << "Avg Time (ns)"
              << std::setw(25) << "Throughput (ops/sec)" << "\n";
    std::cout << "-----------------------------------------------\n";

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltValue val;
        
        // Null
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(nullptr);
        }, iterations);
        PrintResult("Null", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);

        // Bool
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(true);
        }, iterations);
        PrintResult("Bool", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);

        // Integer
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(1234567890);
        }, iterations);
        PrintResult("Integer", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);

        // Float
        auto avg_ns = Benchmark([&] {
            u8 out;
            buf.Reset(); 
            encoder.Encode((double)1.23);
        }, iterations);
        PrintResult("Float", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);

        // String
        std::string str = "Hello, world! This is the Doctor speaking";
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(str.c_str(), str.length());
        }, iterations);
        PrintResult("String", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        
        // Bytes
        std::vector<u8> bytes{0xAB, 0xCD, 0xEF};
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(bytes);
        }, iterations);
        PrintResult("Bytes", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltValue list = { 
            {1, "Hi", 3, true, 512 }, 
            {"Ok, I wrote some post everyone freaks out? What I do?", "nested?", 678984, false},
            {"five", 35}, //{val}, 
            {"true", {1, 2, true, false, "another nested text", 3.14567889} }  
        };
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(list);
        }, iterations);

        PrintResult("List", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltValue complexMap = {
            mp("statement", "MATCH (n:Person)-[:KNOWS]->(m:Person) "
                            "WHERE n.name = $name AND m.age > $minAge "
                            "RETURN m.name, m.age, m.location ORDER BY m.age DESC"),
            mp("parameters", BoltValue({
                mp("name", "Alice"),
                mp("minAge", 30),
                mp("includeDetails", true),
                mp("filters", BoltValue({
                    mp("location", "Europe"),
                    mp("interests", BoltValue({
                        "hiking", "reading", "travel"
                    }))
                }))
            }))
        };
        
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(complexMap);
        }, iterations);
        PrintResult("Map", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }


    {
        BoltBuf buf;
        BoltEncoder encoder(buf);


        // struct
        BoltValue map = {
            mp("one", "two"), 
            mp("key","val"), 
            mp("C","C++"), 
            mp("four", "4"), 
            mp("five", true), 
            mp("Six", BoltValue::Make_Null())
        };

        BoltValue bolt_struct(
            0x00, {"Hit", "the", "road", "jack",
            BoltValue({
                {1, "Hi", 3, true, 512 }, 
                {"Ok, I wrote some post everyone freaks out? What I do?", "nested?", 678984, false},
                {"five", 35}, 
                {"true", {1, 2, true, false, "another nested text", 3.14567889} }  
            }), map
        }
        );
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(bolt_struct);
        }, iterations);
        PrintResult("Struct", avg_ns);
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }


    {
        BoltBuf buf;
        BoltEncoder encoder(buf);


        // struct
        BoltMessage msg( BoltValue(
            0x00, {"Hit", "the", "road", "jack",
            BoltValue({
                {1, "Hi", 3, true, 512 }, 
                {"Ok, I wrote some post everyone freaks out? What I do?", "nested?", 678984, false},
                {"five", 35}, 
                {"true", {1, 2, true, false, "another nested text", 3.14567889} }  
            }), {
                    mp("statement", "MATCH (n:Person)-[:KNOWS]->(m:Person) "
                                    "WHERE n.name = $name AND m.age > $minAge "
                                    "RETURN m.name, m.age, m.location ORDER BY m.age DESC"),
                    mp("parameters", BoltValue({
                        mp("name", "Alice"),
                        mp("minAge", 30),
                        mp("includeDetails", true),
                        mp("filters", BoltValue({
                            mp("location", "Europe"),
                            mp("interests", BoltValue({
                                "hiking", "reading", "travel"
                            }))
                        }))
                    }))
                }
        }
        ) );
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            encoder.Encode(msg);
        }, iterations);
        PrintResult("Message", avg_ns);        
        //Dump_Hex((const char*)buf.Data(), buf.Size());
    }
} // end Encode_Test


void Decode_Test(size_t iterations)
{
    std::cout << "Decoding Benchmark (" << iterations << " iterations)\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << std::left << std::setw(15) << "Operation"
              << std::right << std::setw(20) << "Avg Time (ns)"
              << std::setw(25) << "Throughput (ops/sec)" << "\n";
    std::cout << "-----------------------------------------------\n";

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;
        

        // Null
        encoder.Encode(nullptr);
        auto avg_ns = Benchmark([&] {
            decoder.Decode(val);
            buf.Reset_Read(); 
        }, iterations);
        PrintResult("Null", avg_ns);
        //std::cout << val.ToString() << std::endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;

        // Bool
        encoder.Encode(false);
        auto avg_ns = Benchmark([&] { 
            decoder.Decode(val);
            buf.Reset_Read();
        }, iterations);
        PrintResult("Bool", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;

        // Integer
        encoder.Encode(1234567890);
        auto avg_ns = Benchmark([&] { 
            decoder.Decode(val);
            buf.Reset_Read();
        }, iterations);
        PrintResult("Integer", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;

        // Float
        encoder.Encode((double)1.23);
        auto avg_ns = Benchmark([&] {
            decoder.Decode(val);
            buf.Reset_Read(); 
        }, iterations);
        PrintResult("Float", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;

        // String
        string str="Hello, world! This is the Doctor speaking";
        encoder.Encode(str);
        auto avg_ns = Benchmark([&] { 
            decoder.Decode(val);
            buf.Reset_Read();
        }, iterations);
        PrintResult("String", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;
        
        // Bytes
        std::vector<u8> bytes{0xAB, 0xCD, 0xEF};
        encoder.Encode(bytes);
        auto avg_ns = Benchmark([&] { 
            decoder.Decode(val);
            buf.Reset_Read();
        }, iterations);
        PrintResult("Bytes", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;
        std::string s;

        // List
        BoltValue list = { 
            {5, "Hi", 3, true, 512 }, 
            {"Ok, I wrote some post everyone freaks out? What I do?", "nested?", 678984, false},
            { {"key","value"}, {"five", 35}, 
              "true", {1, 2, true, false, "another nested text", 3.14567889} }  
        };
        encoder.Encode(list);

        auto avg_ns = Benchmark([&] {
            buf.Reset_Read(); 
            decoder.Decode(val);
        }, iterations);
        PrintResult("List", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;

        // Map
        BoltValue complexMap = {
            mp("statement", "MATCH (n:Person)-[:KNOWS]->(m:Person) "
                            "WHERE n.name = $name AND m.age > $minAge "
                            "RETURN m.name, m.age, m.location ORDER BY m.age DESC"),
            mp("parameters", BoltValue({
                mp("name", "Alice"),
                mp("minAge", 30),
                mp("includeDetails", true),
                mp("filters", BoltValue({
                    mp("location", "Europe"),
                    mp("interests", BoltValue({
                        "hiking", "reading", "travel"
                    }))
                }))
            }))
        };
        encoder.Encode(complexMap);

        auto avg_ns = Benchmark([&] {
            decoder.Decode(val);
            buf.Reset_Read();
        }, iterations);
        PrintResult("Map", avg_ns);
        //cout << val.ToString() << endl;
    }


    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltValue val;

        // struct
        BoltValue bolt_struct(
            0x00, {"Hit", "the", "road", "jack", {
                        mp("one", "two"), 
                        mp("key", BoltValue({
                            "The story of the deep nest in maps proven",
                            false,
                            "With hetro data, super duper neat",
                            456778990, 
                            3.142
                        })), 
                        mp("C","C++"), 
                        mp("four", 4), 
                        mp("five", true), 
                        mp("Six", BoltValue::Make_Null())
                    }, 
                { 
                    {1, "Hi", 3, true, 512 }, 
                    {"Ok, I wrote some post everyone freaks out? What I do?", "nested?", 678984, false},
                    { {"key","value"}, {"five", 35}, 
                      "true", {1, 2, true, false, "another nested text", 3.14567889} }  
                } 
            }
        );
        encoder.Encode(bolt_struct);
        
        auto avg_ns = Benchmark([&] {
            decoder.Decode(val);
            buf.Reset_Read();
        }, iterations);
        PrintResult("Struct", avg_ns);
        //cout << val.ToString() << endl;
    }

    {
        BoltBuf buf;
        BoltEncoder encoder(buf);
        BoltDecoder decoder(buf);
        BoltMessage val;

        // msg
        BoltMessage msg( BoltValue(
            0x00, {"Hit", "the", "road", "jack",
            BoltValue({
                {1, "Hi", 3, true, 512 }, 
                {"Ok, I wrote some post everyone freaks out? What I do?", "nested?", 678984, false},
                {"five", 35}, 
                {"true", {1, 2, true, false, "another nested text", 3.14567889} }  
            }), {
                    mp("statement", "MATCH (n:Person)-[:KNOWS]->(m:Person) "
                                    "WHERE n.name = $name AND m.age > $minAge "
                                    "RETURN m.name, m.age, m.location ORDER BY m.age DESC"),
                    mp("parameters", BoltValue({
                        mp("name", "Alice"),
                        mp("minAge", 30),
                        mp("includeDetails", true),
                        mp("filters", BoltValue({
                            mp("location", "Europe"),
                            mp("interests", BoltValue({
                                "hiking", "reading", "travel"
                            }))
                        }))
                    }))
                }
        }
        ) );

        encoder.Encode(msg);

        std::string s;
        auto avg_ns = Benchmark([&] {
            buf.Reset(); 
            decoder.Decode(val);
        }, iterations);
        PrintResult("Message", avg_ns);        
        //cout << val.ToString() << endl;
    }
} // end Decode_Test


int main() 
{
    Print_Title();

    constexpr size_t iterations = 1'000'000;
    Encode_Test(iterations);
    cout << endl;
    Decode_Test(iterations);

    return 0;
}