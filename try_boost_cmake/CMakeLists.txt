cmake_minimum_required(VERSION 3.14)
project(BoostPFRExample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add cmake directory to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find packages
find_package(Boost REQUIRED)
find_package(Fmt REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE Boost::pfr fmt::fmt)