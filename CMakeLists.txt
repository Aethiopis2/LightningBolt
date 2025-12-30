cmake_minimum_required(VERSION 3.15)
project(lightiningbolt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -march=native")

add_library(driver SHARED
    src/neocell.cpp
    src/connection/tcp_client.cpp
    src/connection/neoconnection.cpp
    src/bolt/bolt_encoder.cpp
    src/bolt/bolt_jump_table.cpp
    src/utils/utils.cpp
    src/utils/errors.cpp
    "include/neocellworker.h"
    "include/neopool.h")

target_include_directories(driver PUBLIC include)

add_executable(connection_test src/test/connection_test.cpp)
target_link_libraries(connection_test driver)
set_target_properties(connection_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)