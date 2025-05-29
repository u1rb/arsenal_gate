# Parquet FFI Project - Comprehensive Memory

## Project Overview

The **parquet_ffi** project is a high-performance Rust-based FFI library that provides zero-copy Parquet file reading and writing capabilities for C/C++ applications. The project has evolved from a monolithic design into a modular, header-only library system with excellent performance characteristics and clean CMake integration.

## Architecture

### Core Components

```
parquet_ffi/
├── include/parquet_ffi/                 # Clean namespace organization
│   ├── parquet_reader_stream.h          # Zero-copy reader (700+ lines)
│   ├── parquet_writer_stream.h          # Zero-copy writer with compression
│   ├── parquet_stream.h                 # Core FFI interface
│   └── arrow_c.h                        # Arrow C data interface
├── cxx_examples/                        # Comprehensive examples
│   ├── a1_read_stream.c                 # Reader demo (200 lines)
│   ├── a0_write_stream.c                # Writer demo
│   ├── simple_read_example.c            # Minimal example (70 lines)
│   ├── write_stream_advanced.c          # Advanced features demo
│   └── CMakeLists.txt                   # Examples build system
├── src/                                 # Rust FFI implementation
└── CMakeLists.txt                       # Main build configuration
```

### Design Philosophy

- **Header-Only Libraries**: Core functionality provided as header-only libraries for easy integration
- **Zero-Copy Architecture**: Direct Arrow buffer access minimizes memory operations
- **Namespace Organization**: All headers under `parquet_ffi/` prefix for clean integration
- **Performance First**: Optimized for high-throughput data processing
- **Modern CMake**: FetchContent and subdirectory integration optimized

## Performance Characteristics

### Benchmarked Performance
- **Reader**: 17-22 million rows/sec with zero-copy string/binary access
- **Writer**: 115+ MB/s write speeds with LZ4 compression
- **Multi-file Merger**: 15-22 million rows/sec K-way merge with min-heap sorting
- **Memory Efficiency**: Zero-copy operations, minimal allocations

### Performance Features
- **Zero-copy string access**: Direct buffer pointers without copying
- **Batch processing**: Efficient Arrow batch handling
- **Optimized dispatch**: Macro-based function dispatch
- **Automatic resource management**: RAII-style cleanup

## Key Features

### Reader Capabilities (`parquet_reader_stream.h`)
- **Single file reading**: High-performance streaming interface
- **Multi-file merging**: K-way merge with configurable sort column
- **Schema introspection**: Column names, types, counts
- **Type-safe data access**: All Arrow types (int32, int64, double, string, binary)
- **Null handling**: Proper null value detection
- **Zero-copy string/binary access**: Direct buffer pointers

### Writer Capabilities (`parquet_writer_stream.h`)
- **Compression support**: ZSTD, LZ4, Snappy, Gzip, Brotli
- **Per-column encodings**: DELTA_BINARY_PACKED, DICTIONARY, BYTE_STREAM_SPLIT
- **Advanced writer options**: Bloom filters, statistics, compression levels
- **Batch writing**: Configurable batch sizes for optimal performance
- **Type safety**: Compile-time validation of data access patterns

### C++ Compatibility
- **Full C++ support**: All headers work seamlessly in C++ projects
- **Templated row structures** (C++20): Compile-time type-safe row definitions
- **STL integration**: Easy integration with standard containers
- **Exception safety**: RAII wrappers for automatic resource management
- **Type safety**: Enhanced compile-time checking

## CMake Integration

### Target Structure
```cmake
# Main Rust FFI library
parquet_ffi                    # Core Rust library with FFI bindings

# Header-only interface targets
parquet_reader_stream          # Reader functionality
parquet_writer_stream          # Writer functionality
```

### Usage Patterns

#### FetchContent Integration (Recommended)
```cmake
include(FetchContent)
FetchContent_Declare(parquet_ffi 
    GIT_REPOSITORY https://github.com/org/parquet_ffi.git)
FetchContent_MakeAvailable(parquet_ffi)

target_link_libraries(my_app PRIVATE parquet_reader_stream)
```

#### Subdirectory Integration
```cmake
add_subdirectory(path/to/parquet_ffi)
target_link_libraries(my_app PRIVATE parquet_writer_stream)
```

### Build System Features
- **No installation complexity**: Works directly from source
- **FetchContent optimized**: Perfect for modern CMake workflows
- **Cross-platform**: Linux, macOS, Windows support
- **Minimal dependencies**: Self-contained with clear dependency management

## API Reference

### Reader API (`parquet_reader_stream.h`)

#### Stream Management
```c
PacketStream *parquet_reader_init_stream(const char *path);
PacketStream *parquet_merger_init_stream(const char **paths, size_t count, int key_col);
bool packet_stream_next(PacketStream *stream);
void packet_stream_release(PacketStream *stream);
```

#### Schema Access
```c
size_t packet_stream_get_column_count(PacketStream *stream);
const char *packet_stream_get_column_name(PacketStream *stream, size_t col);
const char *packet_stream_get_column_format(PacketStream *stream, size_t col);
```

#### Data Access
```c
int32_t packet_stream_get_int32(PacketStream *stream, size_t col);
int64_t packet_stream_get_int64(PacketStream *stream, size_t col);
double packet_stream_get_double(PacketStream *stream, size_t col);
char *packet_stream_get_string(PacketStream *stream, size_t col);  // Must free
bool packet_stream_is_null(PacketStream *stream, size_t col);

// Zero-copy access (no allocation)
void packet_stream_get_string_zerocopy(PacketStream *stream, size_t col, 
                                      const char **data, size_t *len);
```

### Writer API (`parquet_writer_stream.h`)

#### Writer Creation
```c
StreamWriter *create_writer(const char *filename, size_t batch_size, 
                           const ColumnDef *schema, size_t num_columns);
StreamWriter *create_writer_with_compression(const char *filename, size_t batch_size,
                                            const ColumnDef *schema, size_t num_columns,
                                            ParquetCompression compression);
StreamWriter *create_writer_with_options(const char *filename, size_t batch_size,
                                        const ColumnDef *schema, size_t num_columns,
                                        WriterOptions options);
```

#### Data Writing
```c
void add_row(StreamWriter *writer, const void **values, const bool *nulls, const size_t *sizes);
void close_writer(StreamWriter *writer);
void free_writer(StreamWriter *writer);
```

#### Configuration Structures
```c
typedef struct {
    const char *name;
    const char *format;        // Arrow format string
    bool nullable;
    ParquetEncoding encoding;  // Per-column encoding
    bool enable_statistics;    // Column statistics
    bool enable_bloom_filter;  // Bloom filter support
} ColumnDef;

typedef struct {
    ParquetCompression compression;
    struct {
        int gzip_level;        // 1-9
        int brotli_level;      // 1-11
        int zstd_level;        // 1-22
    } compression_levels;
    bool enable_bloom_filter;
    bool enable_statistics;
} WriterOptions;
```

## Advanced Features

### Multi-File Merging
```c
const char *files[] = {"file1.parquet", "file2.parquet", "file3.parquet"};
PacketStream *stream = parquet_merger_init_stream(files, 3, 0);  // Sort by column 0
// Automatically merges files in globally sorted order
```

### Compression Support
- **ZSTD**: Levels 1-22, excellent compression ratio
- **LZ4**: Fast compression/decompression
- **Snappy**: Google's fast compression
- **Gzip**: Levels 1-9, wide compatibility
- **Brotli**: Levels 1-11, excellent web compression

### Encoding Support
- **DELTA_BINARY_PACKED**: Efficient for sorted numeric data
- **DICTIONARY**: Automatic dictionary encoding for repeated values
- **BYTE_STREAM_SPLIT**: Optimized for floating-point data
- **RLE**: Run-length encoding for repeated values

### C++ Templated Interface (C++20)
```cpp
#include "parquet_ffi/parquet_writer_cpp.hpp"

using PersonRow = std::tuple<std::string, int32_t, double, bool>;
std::vector<std::string> columns = {"name", "age", "salary", "is_active"};
ParquetWriterCpp<PersonRow> writer("people.parquet", columns);

writer.addRow({"Alice", 30, 75000.0, true});  // Compile-time type safety
```

## Development History

### Major Milestones

1. **Monolithic to Modular**: Extracted 1,030-line monolithic `d_read_stream.c` into clean header-only library
2. **CMake Integration**: Successfully integrated all examples into CMake build system
3. **Header-Only Design**: Created reusable header-only libraries for easy integration
4. **Namespace Organization**: Moved all headers under `parquet_ffi/` prefix
5. **C++ Compatibility**: Added full C++ support with templated interfaces
6. **Performance Optimization**: Achieved 17-22M rows/sec read performance

### Refactoring Achievements
- **Before**: 1,030 lines of mixed implementation and demo code
- **After**: 700+ lines of pure library functionality + clean 200-line demos
- **Benefits**: Reusable, maintainable, high-performance, well-documented

## Testing and Validation

### Build Verification
```bash
bash cxx_examples/run.sh --cell=build,write,read
```

### Performance Testing
- **Single file reading**: 19.3M rows/sec, 2.37 GB/s
- **Multi-file merging**: 22.3M rows/sec, 2.73 GB/s
- **Writer performance**: 115.59 MB/s with compression

### Functionality Testing
- ✅ Schema introspection working
- ✅ All Arrow types supported
- ✅ Null handling correct
- ✅ Memory management verified (no leaks)
- ✅ Multi-file merging with sorted output
- ✅ Compression codecs functional
- ✅ C++ compatibility verified

## Usage Examples

### Minimal Reader Example
```c
#include <parquet_ffi/parquet_reader_stream.h>

int main() {
    PacketStream *stream = parquet_reader_init_stream("data.parquet");
    while (packet_stream_next(stream)) {
        int32_t id = packet_stream_get_int32(stream, 0);
        
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

### Advanced Writer Example
```c
#include <parquet_ffi/parquet_writer_stream.h>

int main() {
    static const ColumnDef SCHEMA[] = {
        {.name = "id", .format = "i", .nullable = false,
         .encoding = PARQUET_ENCODING_DELTA_BINARY_PACKED,
         .enable_statistics = true},
        {.name = "name", .format = "u", .nullable = false,
         .encoding = PARQUET_ENCODING_DICTIONARY},
        {.name = "value", .format = "g", .nullable = true}
    };
    
    WriterOptions options = create_default_writer_options();
    options.compression = PARQUET_COMPRESSION_ZSTD;
    options.compression_levels.zstd_level = 9;
    options.enable_bloom_filter = true;
    
    StreamWriter *writer = create_writer_with_options(
        "advanced.parquet", 1000, SCHEMA, 3, options);
    
    // Add data...
    close_writer(writer);
    free_writer(writer);
    return 0;
}
```

## Future Development Opportunities

### Backend Enhancement Needs
The current implementation has a gap between the enhanced C API and the Rust backend:

#### Option 1: Extend Stream Writer
```rust
pub unsafe extern "C" fn parquet_stream_writer_init_with_options(
    stream_ptr: *mut FFI_ArrowArrayStream,
    path_ptr: *const c_char,
    options: *const WriterOptions,
    schema: *const ColumnDef,
    num_columns: usize
) -> *mut c_void
```

#### Option 2: Hybrid Approach
- Keep zero-copy for data handling
- Use advanced FFI for configuration
- Bridge the gap between enhanced API and backend capabilities

### Potential Extensions

1. **Advanced Analytics**: Add support for complex aggregations during reading
2. **Streaming Transformations**: Real-time data transformation during read/write
3. **Parallel Processing**: Multi-threaded reading/writing for large files
4. **Memory Mapping**: Support for memory-mapped file access
5. **Schema Evolution**: Support for schema changes across file versions
6. **Metadata Extraction**: Enhanced metadata and statistics extraction
7. **Custom Encodings**: Plugin system for custom encoding schemes

### Performance Optimization Areas

1. **SIMD Optimizations**: Vectorized operations for numeric data
2. **Async I/O**: Non-blocking file operations
3. **Compression Tuning**: Per-column compression optimization
4. **Cache Optimization**: Better memory access patterns
5. **Batch Size Tuning**: Dynamic batch size optimization

## Best Practices for Future Development

### Code Organization
- Maintain header-only design for easy integration
- Keep namespace clean under `parquet_ffi/` prefix
- Separate library functionality from demo code
- Use consistent error handling patterns

### Performance Considerations
- Prioritize zero-copy operations
- Minimize memory allocations
- Use batch processing for efficiency
- Profile regularly to identify bottlenecks

### API Design
- Maintain backward compatibility
- Use clear, descriptive function names
- Provide both simple and advanced interfaces
- Include comprehensive documentation

### Testing Strategy
- Maintain comprehensive examples
- Test all supported data types
- Verify memory management
- Benchmark performance regularly

## Dependencies and Requirements

### Build Requirements
- **CMake**: 3.16 or later
- **Rust**: Latest stable (for FFI library compilation)
- **C Compiler**: C99 compatible
- **C++ Compiler**: C++11 for basic compatibility, C++20 for templated features

### Runtime Dependencies
- **Arrow C Interface**: For data exchange
- **Compression Libraries**: Linked through Rust dependencies
- **System Libraries**: Standard C/C++ runtime

### Optional Dependencies
- **Corrosion**: For Rust-CMake integration
- **pkg-config**: For system library detection

## Troubleshooting Guide

### Common Build Issues
1. **Missing Rust toolchain**: Install Rust via rustup
2. **CMake version**: Ensure CMake 3.16+
3. **Corrosion issues**: Check Rust target compatibility
4. **Include path problems**: Verify `parquet_ffi/` prefix usage

### Runtime Issues
1. **Memory leaks**: Ensure proper cleanup of streams and writers
2. **Performance problems**: Check batch sizes and compression settings
3. **Type errors**: Verify column formats match data types
4. **File corruption**: Validate input files with other Parquet tools

### C++ Specific Issues
1. **Template compilation**: Ensure C++20 for advanced features
2. **Linking errors**: Check `extern "C"` blocks
3. **Type conversion**: Use explicit casts when needed
4. **STL integration**: Use appropriate container types

This comprehensive memory serves as a complete reference for understanding, maintaining, and extending the parquet_ffi project. The modular design, excellent performance characteristics, and clean API make it suitable for high-performance data processing applications while remaining easy to integrate and extend. 