cmake_minimum_required(VERSION 3.15)
project(lightningbolt)

set(CMAKE_CXX_STANDARD 20)

# default build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
endif()

# compiler flags per build type
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG -march=native")

add_library(driver SHARED
    src/connection/tcp_client.cpp
    src/connection/neoconnection.cpp
    src/neocell.cpp
    src/bolt/bolt_encoder.cpp
    src/bolt/bolt_decoder.cpp
    src/bolt/bolt_jump_table.cpp
    src/utils/utils.cpp
    src/utils/errors.cpp
    src/neodriver.cpp
    src/neopool.cpp 
    src/neoerr.cpp)

find_package(OpenSSL REQUIRED)
target_include_directories(driver PUBLIC include ${OPENSSL_INCLUDE_DIR})
target_link_libraries(driver PUBLIC OpenSSL::SSL OpenSSL::Crypto)


# Tests
foreach(test IN ITEMS encoder_decoder_test streaming_batch_test connection_test basic_query_test async_qbenchmark_test percentile_test driver_test)
    add_executable(${test} src/test/${test}.cpp)
    target_link_libraries(${test} driver)
    set_target_properties(${test} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endforeach()