# Header-Only Library Export

## Overview

The parquet_ffi project exports two high-performance header-only libraries through CMake:

- **`parquet_ffi/parquet_reader_stream.h`** - Zero-copy Parquet reader with multi-file merging
- **`parquet_ffi/parquet_writer_stream.h`** - Zero-copy Parquet writer with compression support

All headers are organized under the `parquet_ffi/` namespace for clean integration into external projects.

## Project Structure

```
parquet_ffi/
├── include/
│   └── parquet_ffi/
│       ├── parquet_reader_stream.h    # Header-only reader library
│       ├── parquet_writer_stream.h    # Header-only writer library
│       ├── parquet_stream.h           # Core FFI interface
│       └── arrow_c.h                  # Arrow C data interface
├── cxx_examples/
│   ├── a1_read_stream.c               # Reader example
│   └── a0_write_stream.c              # Writer example
└── CMakeLists.txt                     # Main CMake configuration
```

## CMake Export Configuration

The main `CMakeLists.txt` creates three targets for FetchContent and subdirectory usage:

1. **`parquet_ffi`** - Main Rust FFI library
2. **`parquet_reader_stream`** - Header-only reader library
3. **`parquet_writer_stream`** - Header-only writer library

### Interface Targets

```cmake
# Header-only library targets
add_library(parquet_reader_stream INTERFACE)
add_library(parquet_writer_stream INTERFACE)

# Include directories
target_include_directories(parquet_reader_stream INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(parquet_writer_stream INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(parquet_reader_stream INTERFACE parquet_ffi)
target_link_libraries(parquet_writer_stream INTERFACE parquet_ffi)
```

## Using in External Projects

### Method 1: FetchContent (Recommended)

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_parquet_app)

include(FetchContent)
FetchContent_Declare(
    parquet_ffi
    GIT_REPOSITORY https://github.com/your-org/parquet_ffi.git
    GIT_TAG main
)
FetchContent_MakeAvailable(parquet_ffi)

# For reader functionality
add_executable(my_reader reader.c)
target_link_libraries(my_reader PRIVATE parquet_reader_stream)

# For writer functionality
add_executable(my_writer writer.c)
target_link_libraries(my_writer PRIVATE parquet_writer_stream)

# For both reader and writer
add_executable(my_app app.c)
target_link_libraries(my_app PRIVATE 
    parquet_reader_stream 
    parquet_writer_stream
)
```

### Method 2: Subdirectory

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_parquet_app)

add_subdirectory(path/to/parquet_ffi)

add_executable(my_reader reader.c)
target_link_libraries(my_reader PRIVATE parquet_reader_stream)

add_executable(my_writer writer.c)
target_link_libraries(my_writer PRIVATE parquet_writer_stream)
```

## Usage Examples

### Reader Example

```c
#include <parquet_ffi/parquet_reader_stream.h>

int main() {
    // Single file reading
    PacketStream *stream = parquet_reader_init_stream("data.parquet");
    
    while (packet_stream_next(stream)) {
        int32_t id = packet_stream_get_value_int32(stream, 0);
        
        // Zero-copy string access
        const char *name;
        size_t name_len;
        packet_stream_get_string_zerocopy(stream, 1, &name, &name_len);
        
        printf("ID: %d, Name: %.*s\n", id, (int)name_len, name);
    }
    
    packet_stream_release(stream);
    return 0;
}
```

### Writer Example

```c
#include <parquet_ffi/parquet_writer_stream.h>

int main() {
    // Define schema
    static const ColumnDef SCHEMA[] = {
        {"id", "i", false},
        {"name", "u", false},
        {"value", "g", true}
    };
    
    // Create writer
    StreamWriter *writer = create_writer("output.parquet", 10000, SCHEMA, 3);
    
    // Add data
    const void *values[3];
    bool nulls[3] = {false, false, false};
    size_t sizes[3] = {0};
    
    int32_t id = 1;
    const char *name = "example";
    double value = 3.14;
    
    values[0] = &id;
    values[1] = name;
    values[2] = &value;
    
    add_row(writer, values, nulls, sizes);
    
    // Close and cleanup
    close_writer(writer);
    free_writer(writer);
    return 0;
}
```

### Multi-File Merger Example

```c
#include <parquet_ffi/parquet_reader_stream.h>

int main() {
    const char *files[] = {
        "file1.parquet",
        "file2.parquet", 
        "file3.parquet"
    };
    
    // Merge files sorted by column 0
    PacketStream *stream = parquet_merger_init_stream(files, 3, 0);
    
    while (packet_stream_next(stream)) {
        // Process merged data in sorted order
        int32_t key = packet_stream_get_value_int32(stream, 0);
        printf("Key: %d\n", key);
    }
    
    packet_stream_release(stream);
    return 0;
}
```

## Header Organization

All headers are organized under the `parquet_ffi/` namespace:

| Header | Purpose |
|--------|---------|
| `parquet_ffi/parquet_reader_stream.h` | Zero-copy reader with multi-file merging |
| `parquet_ffi/parquet_writer_stream.h` | Zero-copy writer with compression support |
| `parquet_ffi/parquet_stream.h` | Core FFI interface |
| `parquet_ffi/arrow_c.h` | Arrow C data interface |

## Benefits

### For Library Users
- **Clean namespace**: All headers under `parquet_ffi/` prefix
- **Easy integration**: Just link against the appropriate target
- **Header-only**: No separate compilation of library code needed
- **Zero-copy performance**: Built-in optimizations for high throughput
- **Type safety**: Compile-time validation of data access patterns

### For Developers
- **Clean separation**: Reader and writer functionality isolated
- **Simple CMake**: No complex installation configuration
- **Flexible usage**: Can use reader-only, writer-only, or both
- **Future-proof**: Easy to extend with additional header-only components
- **Namespace organization**: Clear separation from other libraries

## Performance

Both header-only libraries provide excellent performance:

- **Reader**: 17+ million rows/sec with zero-copy string/binary access
- **Writer**: 115+ MB/s write speeds with compression support
- **Merger**: Efficient K-way merge with min-heap sorting
- **Memory**: Minimal allocations with automatic resource management

## Compatibility

- **C99 compatible**: Works with any modern C compiler
- **CMake 3.16+**: Uses modern CMake practices
- **Cross-platform**: Linux, macOS, Windows support
- **Arrow compatible**: Uses Arrow C data interface standards
- **Namespace clean**: All headers under `parquet_ffi/` prefix
- **FetchContent ready**: Optimized for modern CMake workflows 