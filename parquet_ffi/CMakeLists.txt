cmake_minimum_required(VERSION 3.16)

project(parquet_ffi C CXX)

include(FetchContent)
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.5.1 # Use a specific version/tag for stability
)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(
    MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
    CRATES parquet_ffi
)

target_include_directories(parquet_ffi INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# =============================================================================
# Header-Only Library Exports
# =============================================================================

# Create interface targets for header-only libraries
add_library(parquet_reader_stream INTERFACE)
add_library(parquet_writer_stream INTERFACE)

# Set include directories for header-only libraries
target_include_directories(parquet_reader_stream INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(parquet_writer_stream INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link dependencies for header-only libraries
target_link_libraries(parquet_reader_stream INTERFACE parquet_ffi)
target_link_libraries(parquet_writer_stream INTERFACE parquet_ffi)