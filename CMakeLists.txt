cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(imkcpp_root)

option(BUILD_TESTS "Build tests" OFF)

add_subdirectory(imkcpp)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()