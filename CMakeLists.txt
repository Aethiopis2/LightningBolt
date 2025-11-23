cmake_minimum_required(VERSION 3.15)
project(lightiningbolt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")

add_library(driver SHARED
    src/connection/tcp_client.cpp
    src/connection/neoconnection.cpp
    src/connection/central_dispatcher.cpp
    src/bolt/bolt_encoder.cpp
    src/bolt/bolt_jump_table.cpp
    src/adaptive_buffer.cpp
    src/utils/utils.cpp
    src/utils/errors.cpp
)

target_include_directories(driver PUBLIC include)

add_executable(streaming_batch_test src/test/streaming_batch_tests.cpp)
target_link_libraries(streaming_batch_test driver)
set_target_properties(streaming_batch_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)