cmake_minimum_required(VERSION 3.14)
project(librdkafka_test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if(NOT DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(CMAKE_BUILD_PARALLEL_LEVEL)
endif()

find_package(Catch2)
find_package(fmt)
find_package(librdkafka)

add_executable(tests
    tests/test_librdkafka.cpp
)

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    fmt::fmt
    rdkafka::rdkafka++
    rdkafka::rdkafka
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_test(NAME tests COMMAND tests)