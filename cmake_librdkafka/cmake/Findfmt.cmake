include(FetchContent)

FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/11.2.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(fmt)

if(NOT TARGET fmt::fmt)
    add_library(fmt::fmt ALIAS fmt)
endif()