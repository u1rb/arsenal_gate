#!/bin/bash

set -uexo pipefail

CELLS=""

for arg in "$@"; do
    case $arg in
        --cell=*)
            CELLS="${arg#*=}"
            FOUND_CELL_ARG=true
            ;;
        help|--help|-h)
            echo "Usage: $0 [--cell=write|read|build|format|clean]"
            ;;
        *)
            echo "Error: Unknown argument '$arg'"
            echo "Usage: $0 [--cell=write|read|build|format|clean]"
            ;;
    esac
done

if [ -z "$CELLS" ]; then
    echo "Error: No cells specified"
    echo "Usage: $0 [--cell=write|read|build|format|clean]"
    exit 1
fi

has_cell() {
    [[ ",$CELLS," == *",$1,"* ]]
}

cd $(dirname $0)/..

if has_cell "clean"; then
    echo "Cleaning build artifacts..."
    rm -rf build
    rm -rf target
    rm -rf cxx_examples/build
    echo "Clean completed!"
fi

if has_cell "format"; then
    echo "Formatting C/C++ files with clang-format..."
    
    # Check if clang-format is available
    if ! command -v clang-format &> /dev/null; then
        echo "Error: clang-format not found. Please install clang-format."
        echo "On Ubuntu/Debian: sudo apt install clang-format"
        echo "On macOS: brew install clang-format"
        exit 1
    fi
    
    # Format all C/C++ files in cxx_examples directory (excluding build directories)
    find cxx_examples -name "build" -prune -o -name "_deps" -prune -o \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print | while read -r file; do
        echo "Formatting: $file"
        clang-format -i "$file"
    done
    
    echo "Formatting completed!"
fi

mkdir -p build
cd build

if has_cell "build"; then
    echo "Building examples..."
    cmake -B . \
      -S ../cxx_examples \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build .
    echo "Build completed!"
fi

rm -rf *.log

if has_cell "write"; then
    rm -rf *.parquet*
    time ./a0_write_stream stream_output.parquet 10000000 100000
    
    echo "=== Verifying original output ==="
    duckdb -s "SELECT *,hex(binary_data) FROM 'stream_output.parquet' LIMIT 10;"
    duckdb -s "SELECT COUNT(*) FROM 'stream_output.parquet';"
fi


if has_cell "read"; then
    ./a1_read_stream stream_output.parquet
    ./a1_read_stream stream_output.parquet stream_output.parquet
    ./a1_read_stream stream_output.parquet stream_output.parquet stream_output.parquet
fi

echo "Examples completed successfully!" 