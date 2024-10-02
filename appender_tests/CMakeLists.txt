cmake_minimum_required(VERSION 3.20)

project(appender_tests)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # Use the latest stable release
)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/10.1.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(fmt)

# Detect system architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(DUCKDB_ARCH "aarch64")
else()
    set(DUCKDB_ARCH "amd64")
endif()

message(STATUS "Detected architecture: ${DUCKDB_ARCH}")

FetchContent_Declare(
    duckdb
    URL https://github.com/duckdb/duckdb/releases/download/v1.2.0/libduckdb-linux-${DUCKDB_ARCH}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(duckdb)
find_library(DUCKDB_LIBRARY 
    NAMES duckdb
    PATHS ${duckdb_SOURCE_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

add_library(duckdb::duckdb SHARED IMPORTED)
set_target_properties(duckdb::duckdb PROPERTIES
    IMPORTED_LOCATION ${DUCKDB_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${duckdb_SOURCE_DIR}"
)

add_library(test_suite INTERFACE)

target_link_libraries(test_suite
    INTERFACE
        Catch2::Catch2WithMain
        fmt::fmt
)

add_executable(main main.cpp)

target_include_directories(main 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include 
)

target_link_libraries(main 
    PRIVATE 
        duckdb::duckdb
        test_suite
)

