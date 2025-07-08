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

add_executable(connection_test src/test/connection_test.cpp)
target_link_libraries(connection_test driver)
set_target_properties(connection_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)