cmake_minimum_required(VERSION 3.16)

project(parquet_ffi C CXX)

# Find required packages
find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
    Corrosion
    URL https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.5.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP 1
)
FetchContent_MakeAvailable(Corrosion)

# Import the Rust FFI library
corrosion_import_crate(
    MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
    CRATES parquet_ffi
)

# Make headers available to consumers
target_include_directories(parquet_ffi INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link threading support for consumers
target_link_libraries(parquet_ffi INTERFACE Threads::Threads)