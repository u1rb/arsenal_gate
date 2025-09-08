# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System & Commands

### Build and Test Commands
```bash
# Build and run tests (most common workflow)
bash run.sh --cell=build,run_test

# Clean build
bash run.sh --cell=clean,build,run_test

# Build only
bash run.sh --cell=build

# Run tests only (assumes already built)
bash run.sh --cell=run_test

# Debug vs Release builds
BUILD_TYPE=Release bash run.sh --cell=build,run_test
BUILD_TYPE=Debug bash run.sh --cell=build,run_test  # default
```

### Running Individual Tests
```bash
# Run specific test suite after building
./build/tests "[test-tag]"  # e.g., ./tests "[nbbo]" or ./tests "[composite_id]"

# List all test cases
./build/tests --list-tests
```

## Adding Dependencies

Dependencies are managed through CMake's FetchContent in `cmake/Find*.cmake` files:

1. Create `cmake/Find<LibraryName>.cmake`:
```cmake
include(FetchContent)

FetchContent_Declare(
    <library_name>
    GIT_REPOSITORY https://github.com/<org>/<repo>.git
    GIT_TAG <version>
)

FetchContent_MakeAvailable(<library_name>)

if(NOT TARGET <namespace>::<target>)
    add_library(<namespace>::<target> ALIAS <library_name>)
endif()
```

2. Update `CMakeLists.txt`:
```cmake
find_package(<LibraryName>)
# Add to target_link_libraries for tests
```

Current dependencies:
- **Catch2**: Testing framework
- **fmt**: String formatting library
