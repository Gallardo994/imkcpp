cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(imkcpp_benchmarks)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)

add_subdirectory(lib)

add_executable(imkcpp_benchmarks
        main.cpp
        original/ikcp.c

        original_send.cpp
        imkcpp_send.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/imkcpp/include)
target_link_libraries(imkcpp_benchmarks benchmark::benchmark)