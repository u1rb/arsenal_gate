cmake_minimum_required(VERSION 3.16)
project(parquet_ffi_examples C CXX)

# Set C++ standard for C++ examples
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(parquet_ffi REQUIRED)

# =============================================================================
# C Examples
# =============================================================================

# Reader example using the header-only reader library
add_executable(a1_read_stream a1_read_stream.c)
target_link_libraries(a1_read_stream PRIVATE parquet_ffi)

# Writer example using the header-only writer library  
add_executable(a0_write_stream a0_write_stream.c)
target_link_libraries(a0_write_stream PRIVATE parquet_ffi)

# CSV to Parquet converter for testing
add_executable(csv_to_parquet csv_to_parquet.c)
target_link_libraries(csv_to_parquet PRIVATE parquet_ffi)

# Parquet to CSV converter for testing
add_executable(parquet_to_csv parquet_to_csv.c)
target_link_libraries(parquet_to_csv PRIVATE parquet_ffi)

# =============================================================================
# C++ Examples
# =============================================================================

# C++ compatibility test
add_executable(test_cpp_compatibility test_cpp_compatibility.cpp)
target_link_libraries(test_cpp_compatibility PRIVATE parquet_ffi)

# C++ writer example (requires C++20 for std::span)
add_executable(cpp_writer_example cpp_writer_example.cpp)
target_link_libraries(cpp_writer_example PRIVATE parquet_ffi)

# Simple templated example (requires C++20)
add_executable(simple_templated_example simple_templated_example.cpp)
target_link_libraries(simple_templated_example PRIVATE parquet_ffi)

# Threading performance test
add_executable(test_threading test_threading.cpp)
target_link_libraries(test_threading PRIVATE 
    parquet_ffi
)