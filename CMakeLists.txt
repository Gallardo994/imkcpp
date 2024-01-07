cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(imkcpp_root)

option(ENABLE_ASAN "Enable Address Sanitizer" ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_ASAN)
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Address Sanitizer is only supported by Clang")
    endif()

    message(STATUS "Enabling Address Sanitizer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

add_subdirectory(imkcpp)
add_subdirectory(tests)
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(benchmarks)
endif()