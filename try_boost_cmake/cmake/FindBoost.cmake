# Custom FindBoost.cmake that uses FetchContent to download Boost
include(FetchContent)

FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(BOOST_INCLUDE_LIBRARIES pfr)
set(BOOST_ENABLE_CMAKE ON)

FetchContent_MakeAvailable(Boost)

# Set variables that find_package expects
set(Boost_FOUND TRUE)
set(Boost_LIBRARIES Boost::pfr)