#!/bin/bash

# Test script for Rust FFI integration
# This script builds the Rust library and tests the C interface

set -e  # Exit on error

echo "=== Testing Rust FFI Integration ==="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print test results
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
    else
        echo -e "${RED}✗ $2${NC}"
        exit 1
    fi
}

# 1. Build the Rust library
echo "1. Building Rust library..."
cargo build --release
print_result $? "Rust library built successfully"
echo

# 2. Build and run C examples
echo "2. Testing C FFI examples..."
cd examples

# Build the C example
echo "   Building C example..."
# Just remove the old C example binary, not the Rust library
rm -f c_example || true
# Build the C example with explicit compiler and linker flags
gcc -Wall -O3 -I../include -o c_example c_example.c -L../target/release -lq_spsc -lpthread -ldl
print_result $? "C example compiled successfully"

# Run the C example
echo "   Running C example..."
LD_LIBRARY_PATH=../target/release ./c_example > test_output.log 2>&1
RESULT=$?
if [ $RESULT -eq 0 ]; then
    print_result 0 "C example ran successfully"
    # Show a sample of the output
    echo "   Sample output:"
    head -n 10 test_output.log | sed 's/^/     /'
else
    print_result 1 "C example failed"
    echo "   Error output:"
    cat test_output.log | sed 's/^/     /'
fi
rm -f test_output.log

# Clean up
rm -f c_example || true
cd ..
echo

# 3. Run Rust FFI tests
echo "3. Running Rust FFI tests..."
cargo test --verbose -- --nocapture ffi
RESULT=$?
print_result $RESULT "Rust FFI tests passed"
echo

echo "=== All FFI tests passed! ==="