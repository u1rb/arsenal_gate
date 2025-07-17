cmake_minimum_required(VERSION 3.14)
project(q_spsc VERSION 1.0.0 LANGUAGES CXX)

# Option to build examples
option(Q_SPSC_BUILD_EXAMPLES "Build examples" ON)
option(Q_SPSC_BUILD_BENCHMARKS "Build benchmarks" ON)
option(Q_SPSC_BUILD_TESTS "Build tests" ON)

# Create header-only library
add_library(q_spsc INTERFACE)
add_library(q_spsc::q_spsc ALIAS q_spsc)

# Set include directories
target_include_directories(q_spsc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Set C++ standard
target_compile_features(q_spsc INTERFACE cxx_std_17)

# Platform-specific compile options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(q_spsc INTERFACE
        -march=native
        -mtune=native
    )
endif()


# Build examples if requested
if(Q_SPSC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build benchmarks if requested
if(Q_SPSC_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Build tests if requested
if(Q_SPSC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()