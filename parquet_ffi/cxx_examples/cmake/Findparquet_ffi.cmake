include(FetchContent)
FetchContent_Declare(
    parquet_ffi
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..
)
FetchContent_MakeAvailable(parquet_ffi)