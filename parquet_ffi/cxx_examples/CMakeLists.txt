cmake_minimum_required(VERSION 3.16)
project(parquet_ffi_examples C CXX)

# Set C++ standard for C++ examples
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    parquet_ffi
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..
)
FetchContent_MakeAvailable(parquet_ffi)

# =============================================================================
# Header-Only Library Examples
# =============================================================================

# Reader example using the header-only reader library
add_executable(a1_read_stream a1_read_stream.c)
target_link_libraries(a1_read_stream PRIVATE parquet_reader_stream)

# Writer example using the header-only writer library  
add_executable(a0_write_stream a0_write_stream.c)
target_link_libraries(a0_write_stream PRIVATE parquet_writer_stream)

# =============================================================================
# C++ Compatibility Test
# =============================================================================

# C++ compatibility test
add_executable(test_cpp_compatibility test_cpp_compatibility.cpp)
target_link_libraries(test_cpp_compatibility PRIVATE parquet_ffi)

# C++ writer example (requires C++20 for std::span)
add_executable(cpp_writer_example cpp_writer_example.cpp)
target_link_libraries(cpp_writer_example PRIVATE parquet_writer_stream)

# Simple templated example (requires C++20)
add_executable(simple_templated_example simple_templated_example.cpp)
target_link_libraries(simple_templated_example PRIVATE parquet_writer_stream)

# =============================================================================
# Usage Examples for External Projects
# =============================================================================
#
# When using this library in external projects, you can use:
#
# find_package(parquet_ffi REQUIRED)
#
# # For reader functionality:
# target_link_libraries(your_reader_app PRIVATE parquet_ffi::parquet_reader_stream)
#
# # For writer functionality:
# target_link_libraries(your_writer_app PRIVATE parquet_ffi::parquet_writer_stream)
#
# # For both (or direct FFI access):
# target_link_libraries(your_app PRIVATE parquet_ffi::parquet_ffi)
#
# =============================================================================
