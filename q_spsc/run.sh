#!/bin/bash

# run.sh: call $0 --cell=build to build the project, it will cd to the correct directory and build the project

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

cd `dirname $BASH_SOURCE[0]`

# Parse --cell argument
CELLS=""
for arg in "$@"; do
    case $arg in
        --cell=*)
            CELLS="${arg#*=}"
            shift
            ;;
    esac
done

# Function to check if a cell should be executed
has_cell() {
    local cell=$1
    # Check if the cell is in the comma-separated list
    if [[ ",$CELLS," == *",$cell,"* ]]; then
        return 0
    fi
    return 1
}

# Check if --cell argument was provided
if [ -z "$CELLS" ]; then
    echo -e "${RED}Error: No --cell argument provided${NC}"
    echo ""
    echo "Usage: $0 --cell=<cell1,cell2,...>"
    echo ""
    echo "Available cells:"
    echo "  build         - Build the project (Release mode)"
    echo "  build-debug   - Build the project (Debug mode)"
    echo "  test          - Run unit tests"
    echo "  bench         - Run standard benchmarks"
    echo "  bench-all     - Run all benchmarks (including long ones)"
    echo "  quick         - Run quick performance test"
    echo "  examples      - Run examples"
    echo "  clean         - Clean the build directory"
    echo "  install       - Install the library"
    echo ""
    echo "Examples:"
    echo "  $0 --cell=clean,build,test"
    echo "  $0 --cell=build,bench"
    echo "  $0 --cell=clean,build-debug,test"
    echo "  $0 --cell=build,bench-all,examples"
    exit 1
fi

# Default build type is Release unless build-debug is specified
BUILD_TYPE="Release"
if has_cell "build-debug"; then
    BUILD_TYPE="Debug"
fi

# Clean step
if has_cell "clean"; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf build
fi

# Build step
if has_cell "build" || has_cell "build-debug"; then
    echo -e "${BLUE}Building Q SPSC library...${NC}"
    echo -e "${BLUE}Build type: $BUILD_TYPE${NC}\n"
    
    mkdir -p build
    pushd build > /dev/null

    echo -e "${YELLOW}Configuring CMake...${NC}"
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DQ_SPSC_BUILD_EXAMPLES=ON \
        -DQ_SPSC_BUILD_BENCHMARKS=ON \
        -DQ_SPSC_BUILD_TESTS=ON \
        -G Ninja

    echo -e "\n${YELLOW}Building...${NC}"
    cmake --build .

    echo -e "\n${GREEN}Build complete!${NC}"
    
    popd > /dev/null
fi

# Quick test mode
if has_cell "quick"; then
    pushd build > /dev/null
    
    echo -e "\n${YELLOW}Running quick performance test...${NC}\n"
    
    # Compile and run simple benchmarks
    echo -e "${BLUE}1. Single-thread operation latency:${NC}"
    g++ -O3 -march=native -I../include ../benchmarks/simple_latency_bench.cpp -o simple_latency_bench 2>/dev/null || true
    if [ -f simple_latency_bench ]; then
        ./simple_latency_bench
    fi
    
    echo -e "\n${BLUE}2. Throughput test (1M messages):${NC}"
    ./benchmarks/throughput_benchmark --messages 1000000 --capacity 4
    
    popd > /dev/null
fi

# Standard benchmark suite
if has_cell "bench"; then
    pushd build > /dev/null
    
    echo -e "${YELLOW}Running benchmark suite...${NC}\n"

    # 1. Single-thread latency
    echo -e "${BLUE}1. Single-thread operation latency:${NC}"
    if [ ! -f simple_latency_bench ]; then
        g++ -O3 -march=native -I../include ../benchmarks/simple_latency_bench.cpp -o simple_latency_bench
    fi
    ./simple_latency_bench

    # 2. Inter-thread ping-pong latency
    echo -e "\n${BLUE}2. Inter-thread communication latency:${NC}"
    if [ ! -f ping_pong_bench ]; then
        g++ -O3 -march=native -pthread -I../include ../benchmarks/ping_pong_bench.cpp -o ping_pong_bench
    fi
    ./ping_pong_bench

    # 3. Hot path latency for different message sizes
    echo -e "\n${BLUE}3. Hot path latency (32, 128, 512 byte messages):${NC}"
    if [ ! -f hot_path_latency ]; then
        g++ -O3 -march=native -pthread -I../include ../benchmarks/hot_path_latency.cpp -o hot_path_latency
    fi
    ./hot_path_latency --messages 1000000

    # 4. Throughput benchmark
    echo -e "\n${BLUE}4. Throughput benchmark:${NC}"
    ./benchmarks/throughput_benchmark --messages 10000000 --capacity 8
    
    popd > /dev/null
fi

# All benchmarks (including long ones)
if has_cell "bench-all"; then
    pushd build > /dev/null
    
    echo -e "${YELLOW}Running all benchmarks...${NC}\n"

    # 1. Single-thread latency
    echo -e "${BLUE}1. Single-thread operation latency:${NC}"
    if [ ! -f simple_latency_bench ]; then
        g++ -O3 -march=native -I../include ../benchmarks/simple_latency_bench.cpp -o simple_latency_bench
    fi
    ./simple_latency_bench

    # 2. Inter-thread ping-pong latency
    echo -e "\n${BLUE}2. Inter-thread communication latency:${NC}"
    if [ ! -f ping_pong_bench ]; then
        g++ -O3 -march=native -pthread -I../include ../benchmarks/ping_pong_bench.cpp -o ping_pong_bench
    fi
    ./ping_pong_bench

    # 3. Hot path latency for different message sizes
    echo -e "\n${BLUE}3. Hot path latency (32, 128, 512 byte messages):${NC}"
    if [ ! -f hot_path_latency ]; then
        g++ -O3 -march=native -pthread -I../include ../benchmarks/hot_path_latency.cpp -o hot_path_latency
    fi
    ./hot_path_latency --messages 1000000

    # 4. Throughput benchmark (large)
    echo -e "\n${BLUE}4. Throughput benchmark (100M messages):${NC}"
    ./benchmarks/throughput_benchmark --messages 100000000 --capacity 16

    # 5. Full latency benchmark
    echo -e "\n${BLUE}5. Full latency benchmark (this takes time):${NC}"
    ./benchmarks/latency_benchmark --messages 1000000 --capacity 8192
    
    popd > /dev/null
fi

# Run tests
if has_cell "test"; then
    pushd build > /dev/null
    
    echo -e "\n${YELLOW}Running unit tests...${NC}\n"
    
    echo -e "${BLUE}Unit tests:${NC}"
    ./tests/test_spsc
    
    echo -e "\n${BLUE}Performance tests:${NC}"
    ./tests/test_performance
    
    popd > /dev/null
fi

# Run examples
if has_cell "examples"; then
    pushd build > /dev/null
    
    echo -e "\n${YELLOW}Running examples...${NC}\n"
    
    echo -e "${BLUE}Basic example:${NC}"
    ./examples/basic_example
    
    echo -e "\n${BLUE}HFT example:${NC}"
    ./examples/hft_example
    
    echo -e "\n${BLUE}Opaque data example:${NC}"
    ./examples/opaque_data_example
    
    popd > /dev/null
fi

# Install
if has_cell "install"; then
    pushd build > /dev/null
    
    echo -e "\n${YELLOW}Installing Q SPSC library...${NC}"
    echo -e "${RED}Note: This may require sudo privileges${NC}\n"
    
    sudo cmake --install .
    
    echo -e "\n${GREEN}Installation complete!${NC}"
    
    popd > /dev/null
fi

echo -e "\n${GREEN}All requested tasks completed successfully!${NC}"