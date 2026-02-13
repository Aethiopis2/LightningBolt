cmake_minimum_required(VERSION 3.15)
project(lightiningbolt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")

add_library(driver SHARED
    src/neocell.cpp
    src/connection/tcp_client.cpp
    src/connection/neoconnection.cpp
    src/bolt/bolt_encoder.cpp
    src/bolt/bolt_jump_table.cpp
    src/utils/utils.cpp
    src/utils/errors.cpp
    "include/neopool.h" "src/neopool.cpp" "src/neoerr.cpp")

find_package(OpenSSL REQUIRED)
target_include_directories(driver PUBLIC include ${OPENSSL_INCLUDE_DIR})
target_link_libraries(driver PUBLIC OpenSSL::SSL OpenSSL::Crypto)


# encoder decoder tests - measures the speed of bolt protocol using PackStream 
#   serialization/deserialization or encoding/decoding.
add_executable(encoder_decoder_test src/test/encoder_decoder_test.cpp "src/neoerr.cpp")
target_link_libraries(encoder_decoder_test driver)
set_target_properties(encoder_decoder_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# streaming batch test - measures speed under chunks, i.e. ability to handle chunks
#   very cool for large sets of data.
add_executable(streaming_batch_test src/test/streaming_batch_tests.cpp "src/neodriver.cpp" "src/neopool.cpp" "src/neoerr.cpp")
target_link_libraries(streaming_batch_test driver)
set_target_properties(streaming_batch_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# connection test - measures simple connect/disconnect with Neo4j database server.
#   A simple stability test.
add_executable(connection_test src/test/connection_test.cpp "src/neodriver.cpp" "src/neopool.cpp" "src/neoerr.cpp")
target_link_libraries(connection_test driver)
set_target_properties(connection_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# basic query streaming test - measures speed basic query streaming to and from
#   Neo4j db server over bolt protocol.
add_executable(basic_query_test src/test/basic_query_test.cpp "src/neodriver.cpp" "src/neopool.cpp" "src/neoerr.cpp")
target_link_libraries(basic_query_test driver)
set_target_properties(basic_query_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# async qery test - measures async pooled connection and async callback functionality 
#   query streaming speeds.
add_executable(async_qbenchmark_test src/test/async_qbenchmark_test.cpp "include/neodriver.h" "src/neodriver.cpp" "src/neopool.cpp" "src/neoerr.cpp")
target_link_libraries(async_qbenchmark_test driver)
set_target_properties(async_qbenchmark_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)