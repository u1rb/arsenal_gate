#!/bin/bash

BUILD_TYPE=${BUILD_TYPE:-Debug}
BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || echo 4)

parse_cells() {
    local cells=$1
    IFS=',' read -ra CELL_ARRAY <<< "$cells"
    echo "${CELL_ARRAY[@]}"
}

cell_clean() {
    echo "=== Cleaning build directory ==="
    rm -rf "$BUILD_DIR"
    echo "Clean complete"
}

cell_build() {
    echo "=== Building project (${BUILD_TYPE}) ==="
    
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
    fi
    
    cd "$BUILD_DIR" || exit 1
    
    echo "Configuring CMake with Ninja..."
    cmake .. \
        -GNinja \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || {
        echo "CMake configuration failed"
        exit 1
    }
    
    echo "Building with Ninja..."
    ninja || {
        echo "Build failed"
        exit 1
    }
    
    cd .. || exit 1
    echo "Build complete"
}

cell_run_test() {
    echo "=== Running tests ==="
    
    if [ ! -f "$BUILD_DIR/tests" ]; then
        echo "Error: Test binary not found. Please build first."
        exit 1
    fi
    
    cd "$BUILD_DIR" || exit 1
    
    if [ -n "$TEST_FILTER" ]; then
        echo "Running tests with filter: $TEST_FILTER"
        ./tests "$TEST_FILTER"
    else
        echo "Running all tests..."
        ./tests
    fi
    
    TEST_RESULT=$?
    cd .. || exit 1
    
    if [ $TEST_RESULT -eq 0 ]; then
        echo "All tests passed!"
    else
        echo "Some tests failed"
        exit $TEST_RESULT
    fi
}

show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
    --cell=CELLS    Comma-separated list of cells to run
                    Available cells: clean, build, run_test
                    Example: --cell=build,run_test
    
    --help          Show this help message

Environment Variables:
    BUILD_TYPE      Build type (Debug/Release), default: Debug
    TEST_FILTER     Test filter pattern for Catch2
    JOBS           Number of parallel build jobs

Examples:
    # Build and run tests
    $0 --cell=build,run_test
    
    # Clean, build, and test
    $0 --cell=clean,build,run_test
    
    # Release build with tests
    BUILD_TYPE=Release $0 --cell=build,run_test
    
    # Run specific tests
    TEST_FILTER="[librdkafka]" $0 --cell=run_test
EOF
}

main() {
    local cells=""
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --cell=*)
                cells="${1#*=}"
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    if [ -z "$cells" ]; then
        echo "Error: No cells specified"
        show_usage
        exit 1
    fi
    
    IFS=',' read -ra CELL_ARRAY <<< "$cells"
    
    for cell in "${CELL_ARRAY[@]}"; do
        case $cell in
            clean)
                cell_clean
                ;;
            build)
                cell_build
                ;;
            run_test)
                cell_run_test
                ;;
            *)
                echo "Error: Unknown cell '$cell'"
                echo "Available cells: clean, build, run_test"
                exit 1
                ;;
        esac
    done
}

main "$@"