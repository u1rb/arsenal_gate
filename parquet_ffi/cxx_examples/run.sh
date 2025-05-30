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
            echo "Usage: $0 [--cell=write|read|build|format|clean|cpp_writer|threading|test_write]"
            echo ""
            echo "Available cells:"
            echo "  build      - Build all examples"
            echo "  format     - Format C/C++ code"
            echo "  write      - Create test Parquet files"
            echo "  read       - Test reading capabilities"
            echo "  cpp_writer - Test C++ writer examples"
            echo "  threading  - Test threading performance"
            echo "  test_write - Run Python unit tests for write functionality (CSV round-trip)"
            echo "  clean      - Clean build artifacts"
            echo ""
            echo "Examples:"
            echo "  $0 --cell=build,write,read          # Standard test sequence"
            echo "  $0 --cell=build,write,threading     # Focus on threading performance"
            echo "  $0 --cell=format,build,write,read   # Format code and test"
            echo "  $0 --cell=build,test_write          # Build and run Python round-trip tests"
            ;;
        *)
            echo "Error: Unknown argument '$arg'"
            echo "Usage: $0 [--cell=write|read|build|format|clean|cpp_writer|threading|test_write]"
            ;;
    esac
done

if [ -z "$CELLS" ]; then
    echo "Error: No cells specified"
    echo "Usage: $0 [--cell=write|read|build|format|clean|cpp_writer|threading|test_write]"
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
    
    # Format all C/C++ files in cxx_examples and include/ directory (excluding build directories)
    find cxx_examples include -name "build" -prune -o -name "_deps" -prune -o \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print | while read -r file; do
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
    echo "=== Testing Synchronous Reading ==="
    ./a1_read_stream stream_output.parquet
    ./a1_read_stream stream_output.parquet stream_output.parquet
    ./a1_read_stream stream_output.parquet stream_output.parquet stream_output.parquet
    
    echo ""
    echo "=== Testing Threaded Reading ==="
    ./a1_read_stream --threaded stream_output.parquet
    ./a1_read_stream --threaded stream_output.parquet stream_output.parquet
    ./a1_read_stream --threaded stream_output.parquet stream_output.parquet stream_output.parquet
    
    echo ""
    echo "=== Performance Comparison (Single File) ==="
    echo "Synchronous mode:"
    ./a1_read_stream stream_output.parquet | grep "Processed.*rows/sec"
    echo "Threaded mode:"
    ./a1_read_stream --threaded stream_output.parquet | grep "Processed.*rows/sec"
fi

if has_cell "threading"; then
    echo "=== Threading Performance Tests ==="
     
    ITERATIONS=2
    ROWS=100000000 # 100M rows
    WRITE_BATCH_SIZE=65536
    READ_BATCH_SIZE=1000000 # 1M rows

    echo "Large file ($ROWS rows):"

    # skip if file exists
    if [ -f large_test.parquet ]; then
        echo "File large_test.parquet already exists, skipping write"
    else
        echo "Writing file large_test.parquet"
        time ./a0_write_stream large_test.parquet $ROWS $WRITE_BATCH_SIZE
    fi
    
    echo ""
    echo "=== Performance Comparison Tests ==="
    
    echo ""
    echo "=== Full Processing Comparison ==="
    echo "Synchronous mode ($ROWS rows with full processing):"
    ./a1_read_stream \
        --workload=heavy \
        --iterations=$ITERATIONS \
        --batch-size=$READ_BATCH_SIZE \
        large_test.parquet | grep "Processed.*rows/sec"
    
    echo "Threaded mode ($ROWS rows with full processing):"
    ./a1_read_stream \
        --threaded \
        --workload=heavy \
        --iterations=$ITERATIONS \
        --batch-size=$READ_BATCH_SIZE \
        large_test.parquet | grep "Processed.*rows/sec"
    
    echo ""
    echo "=== Cleanup ==="
    #rm -rf large_test.parquet
    
    echo ""
    echo "Threading performance tests completed!"
    echo "See THREADING_PERFORMANCE_SUMMARY.md for detailed analysis."
fi

if has_cell "cpp_writer"; then
    echo "=== Running C++ Writer Example ==="
    rm -rf person_data.parquet binary_data.parquet
    time ./cpp_writer_example
    
    echo "=== Verifying person_data.parquet ==="
    duckdb -s "SELECT * FROM 'person_data.parquet' LIMIT 10;"
    duckdb -s "SELECT COUNT(*) FROM 'person_data.parquet';"
    
    echo "=== Verifying binary_data.parquet ==="
    duckdb -s "SELECT id, description, hex(binary_data), score FROM 'binary_data.parquet' LIMIT 10;"
    duckdb -s "SELECT COUNT(*) FROM 'binary_data.parquet';"
fi

# =============================================================================
# Python Unit Tests for Write Functionality
# =============================================================================

if has_cell "test_write"; then
    echo "🧪 Running comprehensive CSV-to-Parquet tests with round-trip validation..."
    echo "This tests:"
    echo "  - CSV generation and parsing"
    echo "  - Schema detection and type inference"
    echo "  - All compression codecs (ZSTD, LZ4, Snappy, Gzip, Brotli)"
    echo "  - Different batch sizes"
    echo "  - Edge cases and large files"
    echo "  - Full round-trip validation (CSV → Parquet → CSV)"
    echo ""
    
    # Check dependencies and files
    command -v uv >/dev/null || { echo "Error: uv not found. Install with: curl -LsSf https://astral.sh/uv/install.sh | sh"; exit 1; }
    cd ../cxx_examples
    [ -f "test_write_stream.py" ] || { echo "Error: test_write_stream.py not found"; exit 1; }
    [ -f "../build/csv_to_parquet" ] || { echo "Error: csv_to_parquet not built. Run build first."; exit 1; }
    [ -f "../build/parquet_to_csv" ] || { echo "Error: parquet_to_csv not built. Run build first."; exit 1; }
    
    echo "Running comprehensive Python unit tests with CSV round-trip validation..."
    echo "Tests: CSV generation, schema detection, compression codecs, batch sizes, edge cases"
    echo "Note: All test artifacts will be created in build/test_artifacts/"
    echo ""
    
    # Run tests from cxx_examples directory, but executables are in build/
    if uv run --with pyarrow --with pandas test_write_stream.py; then
        echo "✅ All Python unit tests passed! CSV-to-Parquet conversion working correctly."
    else
        echo "❌ Some Python unit tests failed. Check output above."
        exit 1
    fi
fi

echo "Examples completed successfully!" 