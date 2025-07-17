# Custom FindFmt.cmake that uses FetchContent to download fmt
include(FetchContent)

FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/releases/download/11.2.0/fmt-11.2.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(fmt)

# Set variables that find_package expects
set(Fmt_FOUND TRUE)
set(Fmt_LIBRARIES fmt::fmt)