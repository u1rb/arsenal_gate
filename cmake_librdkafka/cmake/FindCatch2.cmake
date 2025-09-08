include(FetchContent)

FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.10.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(Catch2)

if(NOT TARGET Catch2::Catch2WithMain)
    add_library(Catch2::Catch2WithMain ALIAS Catch2WithMain)
endif()