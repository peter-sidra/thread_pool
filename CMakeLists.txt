cmake_minimum_required(VERSION 3.19.0)
project(thread_pool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -pedantic")
elseif (MSVC)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W4")
endif()

add_library(thread_pool INTERFACE include/thread_pool.hpp)
target_include_directories(thread_pool INTERFACE include/)

add_executable(thread_pool_example examples/example1.cpp)
target_link_libraries(thread_pool_example PRIVATE thread_pool)