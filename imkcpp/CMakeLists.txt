cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(imkcpp)

set(SRC_FILES
        src/imkcpp.cpp
)

include_directories(include)
add_library(imkcpp STATIC ${SRC_FILES})