# Parquet Reader Stream - Header-Only Library

## Overview

`parquet_reader_stream.h` is a high-performance, header-only C library for reading Parquet files. It provides a zero-copy interface with support for single file reading and multi-file merging with sorted output.

## Features

- **Header-only**: Just include one file, no separate compilation needed
- **High performance**: Zero-copy data access with batch processing
- **Single file reading**: Stream through large Parquet files efficiently
- **Multi-file merging**: Merge multiple sorted Parquet files using min-heap
- **Type-safe access**: Dedicated functions for each Arrow data type
- **Memory efficient**: Automatic resource management and cleanup
- **Schema introspection**: Access column names, types, and metadata

## Quick Start

### 1. Include the Library

```c
#include "parquet_reader_stream.h"
```

### 2. Single File Reading

```c
// Open a single Parquet file
PacketStream *stream = parquet_reader_init_stream("data.parquet");
if (!stream) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
}

// Read all rows
while (packet_stream_next(stream)) {
    // Access data using type-specific functions
    int32_t id = packet_stream_get_int32(stream, 0);
    char *name = packet_stream_get_string(stream, 1);
    double value = packet_stream_get_double(stream, 2);
    
    printf("ID: %d, Name: %s, Value: %.2f\n", id, name, value);
    
    free(name); // Free strings returned by get_string
}

// Cleanup
packet_stream_release(stream);
```

### 3. Multi-File Merging

```c
// Merge multiple sorted files
const char *files[] = {"file1.parquet", "file2.parquet", "file3.parquet"};
int key_column = 0; // Sort by first column

PacketStream *stream = parquet_merger_init_stream(files, 3, key_column);
if (!stream) {
    fprintf(stderr, "Failed to initialize merger\n");
    return 1;
}

// Read merged data in sorted order
while (packet_stream_next(stream)) {
    // Process merged data...
}

packet_stream_release(stream);
```

## API Reference

### Stream Management

| Function | Description |
|----------|-------------|
| `parquet_reader_init_stream(path)` | Open single Parquet file |
| `parquet_merger_init_stream(paths, count, key_col)` | Merge multiple files |
| `packet_stream_next(stream)` | Advance to next row |
| `packet_stream_release(stream)` | Cleanup and free resources |

### Schema Access

| Function | Description |
|----------|-------------|
| `packet_stream_get_column_count(stream)` | Get number of columns |
| `packet_stream_get_column_name(stream, col)` | Get column name |
| `packet_stream_get_column_format(stream, col)` | Get Arrow format string |

### Data Access

| Function | Return Type | Arrow Format |
|----------|-------------|--------------|
| `packet_stream_get_int32(stream, col)` | `int32_t` | "i" |
| `packet_stream_get_int64(stream, col)` | `int64_t` | "l" |
| `packet_stream_get_double(stream, col)` | `double` | "g" |
| `packet_stream_get_string(stream, col)` | `char*` | "u" |
| `packet_stream_is_null(stream, col)` | `bool` | any |
| `packet_stream_get_binary_size(stream, col)` | `size_t` | "z" |

**Note**: Strings returned by `packet_stream_get_string()` must be freed by the caller.

## Examples

### Schema Introspection

```c
PacketStream *stream = parquet_reader_init_stream("data.parquet");

int num_columns = packet_stream_get_column_count(stream);
printf("Schema (%d columns):\n", num_columns);

for (int i = 0; i < num_columns; i++) {
    const char *name = packet_stream_get_column_name(stream, i);
    const char *format = packet_stream_get_column_format(stream, i);
    printf("  Column %d: %s (%s)\n", i, name, format);
}
```

### Null Value Handling

```c
while (packet_stream_next(stream)) {
    if (packet_stream_is_null(stream, 1)) {
        printf("Name: NULL\n");
    } else {
        char *name = packet_stream_get_string(stream, 1);
        printf("Name: %s\n", name);
        free(name);
    }
}
```

### Performance Measurement

```c
#include <sys/time.h>

struct timeval start, end;
gettimeofday(&start, NULL);

int row_count = 0;
while (packet_stream_next(stream)) {
    row_count++;
    // Process data...
}

gettimeofday(&end, NULL);
double elapsed = (end.tv_sec - start.tv_sec) + 
                 (end.tv_usec - start.tv_usec) / 1000000.0;
double rate = row_count / elapsed;

printf("Processed %d rows in %.3f seconds (%.1f rows/sec)\n",
       row_count, elapsed, rate);
```

## Arrow Format Strings

| Format | Type | Description |
|--------|------|-------------|
| "i" | int32 | 32-bit signed integer |
| "l" | int64 | 64-bit signed integer |
| "L" | uint64 | 64-bit unsigned integer |
| "f" | float32 | 32-bit floating point |
| "g" | double | 64-bit floating point |
| "b" | boolean | Boolean value |
| "c" | int8 | 8-bit signed integer |
| "C" | uint8 | 8-bit unsigned integer |
| "s" | int16 | 16-bit signed integer |
| "S" | uint16 | 16-bit unsigned integer |
| "I" | uint32 | 32-bit unsigned integer |
| "u" | string | UTF-8 string |
| "z" | binary | Binary data |

## Multi-File Merger Requirements

For the merger to work correctly:

1. **Key column**: All files must have the same key column type (int32 or int64)
2. **Schema compatibility**: All files should have the same schema
3. **Sorted data**: Each individual file should be sorted by the key column
4. **Output**: The merger produces globally sorted output

## Performance Tips

1. **Batch processing**: Process data in batches for better cache locality
2. **String handling**: Minimize string allocations by reusing buffers when possible
3. **Column access**: Access columns in order (0, 1, 2...) for better performance
4. **Memory management**: Always free strings returned by `get_string()`

## Error Handling

- Functions return `NULL` or default values on error
- Error messages are printed to `stderr`
- Always check return values from init functions
- Use `packet_stream_release()` for cleanup even on errors

## Dependencies

- Standard C library
- `parquet_stream.h` (Rust FFI interface)
- POSIX headers (`sys/time.h`, `sys/stat.h`)

## Building

The library is header-only, so just include it in your project:

```c
#include "parquet_reader_stream.h"
```

Make sure to link against the Parquet FFI library when building your application.

## Examples in This Directory

- `d_read_stream.c` - Full-featured demo with performance measurement
- `simple_read_example.c` - Minimal usage example

## License

Same as the parent project. 