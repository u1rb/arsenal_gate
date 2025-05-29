# Parquet FFI

This library provides a C interface to read and write Parquet files using Rust's Arrow and Parquet libraries.

## Features

- Write Parquet files with custom schema definitions
- Configure compression, row groups, and other Parquet settings
- Read Parquet files with batch-based processing
- Support for common data types: boolean, int32, int64, float, double, string, etc.
- **Full C++ compatibility** - All headers work seamlessly in C++ projects
- **High-performance multi-threaded reading** - 2.19x faster processing with queue-based threading

## Performance

The library includes a high-performance, multi-threaded Parquet reader with significant performance improvements:

- **🚀 2.19x faster** processing for CPU-intensive workloads
- **📈 17.25M rows/sec** throughput (vs 7.86M synchronous)
- **⚡ Queue-based threading** with producer-consumer pattern
- **💾 Low memory overhead** with circular buffer design

See [`THREADING_PERFORMANCE_SUMMARY.md`](THREADING_PERFORMANCE_SUMMARY.md) for detailed benchmarks and analysis.

## C++ Support

The library provides full C++ compatibility while maintaining its C interface. All C headers can be included and used directly in C++ projects.

Key features:
- Proper `extern "C"` blocks in all headers
- C++ compatible memory management
- Easy integration with STL containers
- RAII-friendly resource management
- **Templated row structures with compile-time type safety (C++20)**

See [`README_CPP_SUPPORT.md`](README_CPP_SUPPORT.md) for detailed documentation and examples.

### C++ Example

```cpp
#include "parquet_ffi/parquet_writer_stream.h"
#include "parquet_ffi/parquet_writer_cpp.hpp"

// Define row structure with compile-time type safety
using PersonRow = std::tuple<std::string, int32_t, double, bool>;
//                           name        age      salary  is_active

// Create templated writer with automatic schema generation
std::vector<std::string> columns = {"name", "age", "salary", "is_active"};
ParquetWriterCpp<PersonRow> writer("people.parquet", columns);

// Add rows with compile-time type checking
writer.addRow({"Alice", 30, 75000.0, true});
writer.addRow({"Bob", 25, 65000.0, true});

// Automatic cleanup and error handling
writer.flush();
writer.close();
```

## Building

To build the library:

```bash
cargo build
```

This will create:
- `target/debug/libparquet_ffi.so` (Linux/macOS) shared library
- `target/debug/libparquet_ffi.a` static library

## Examples

There are examples in two directories:

1. `examples/` - Rust examples showing how to use the library from Rust
2. `examples_cpp/` - C/C++ examples showing how to use the library from C/C++

### Running Examples

#### Rust Examples

```bash
cargo run --example write
cargo run --example read
```

#### C/C++ Examples

```bash
cd examples_cpp
./build.sh
```

Or use the provided run script to run all examples:

```bash
./run.sh
```

## API Overview

### Writing Parquet Files

1. Define your schema with `ParquetColumnDef` structs
2. Configure writer options with `ParquetWriterOptions`
3. Open a writer with `parquet_writer_open()`
4. Start a row group with `parquet_writer_start_row_group()`
5. Write column data with:
   - `parquet_writer_write_primitive_column()` (for all data types)
   - Or write row-by-row with `parquet_writer_write_row()`
6. End the row group with `parquet_writer_end_row_group()`
7. Close the writer with `parquet_writer_close()`

### Reading Parquet Files

1. Configure reader options with `ParquetReaderOptions`
2. Open a reader with `parquet_reader_open()`
3. Get file information with:
   - `parquet_reader_get_num_rows()`
   - `parquet_reader_get_num_columns()`
   - `parquet_reader_get_schema()`
4. Read batches of rows with `parquet_reader_read_next_batch()`
5. Free batch resources with `parquet_reader_free_batch()`
6. Close the reader with `parquet_reader_close()`

## Example Usage

```c
// Create schema
ParquetColumnDef columns[2];
columns[0].name = "id";
columns[0].data_type = PARQUET_INT32;
columns[0].nullable = false;

columns[1].name = "name";
columns[1].data_type = PARQUET_STRING;
columns[1].nullable = true;

// Configure writer
ParquetWriterOptions options;
options.row_group_size = 1024;
options.compression = PARQUET_COMPRESSION_SNAPPY;
options.enable_dictionary = true;
options.enable_statistics = true;
options.enable_bloom_filter = true;

// Open writer
ParquetWriterHandle* writer;
parquet_writer_open("data.parquet", columns, 2, options, &writer);

// Write data...

// Close writer
parquet_writer_close(writer);
```

See the example programs for complete demonstrations. 

# Building with CMake

In addition to using Cargo directly, this project can be built using CMake with the Corrosion integration.

## Prerequisites

- CMake 3.16 or higher
- Rust and Cargo
- C compiler (gcc, clang, etc.)

## Building

Create a build directory and run CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

This will build the Rust library using Corrosion.

## Integration

To use this library in your own CMake project, you can:

1. Use FetchContent to include this project (like in examples_cpp):
   ```cmake
   include(FetchContent)
   FetchContent_Declare(
       parquet_ffi
       GIT_REPOSITORY https://github.com/yourusername/parquet_ffi.git
       GIT_TAG v0.1.0  # Use a specific version/tag for stability
   )
   FetchContent_MakeAvailable(parquet_ffi)
   ```

2. Link against the library in your targets:
   ```cmake
   target_link_libraries(your_target 
       PRIVATE parquet_ffi
       PRIVATE parquet_ffi_header
   )
   ```

See the `examples_cpp/CMakeLists.txt` for a complete example. 