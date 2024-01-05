cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(imkcpp)

option(IMKCPP_SHARED "Build shared library" ON)

set(SRC_FILES
    src/imkcpp.cpp
)

include_directories(include)

if(IMKCPP_SHARED)
    add_library(imkcpp SHARED ${SRC_FILES})
else()
    add_library(imkcpp STATIC ${SRC_FILES})
endif()
