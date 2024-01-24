cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(imkcpp_tests)

add_subdirectory(lib)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
include_directories(${gmock_SOURCE_DIR}/include ${gmock_SOURCE_DIR})

add_executable(imkcpp_tests
        Send_Tests.cpp
        RtoCalculator_Tests.cpp
        Flusher_Tests.cpp
        ReceiverBuffer_Tests.cpp
        SenderBuffer_Tests.cpp
        CongestionController_Tests.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/imkcpp/include)
target_link_libraries(imkcpp_tests gtest gtest_main gmock gmock_main)