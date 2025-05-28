# Advanced Features: Compression and Encoding Support

The enhanced `parquet_writer_zerocopy.h` library now supports advanced Parquet features including per-column encodings, compression codecs, and fine-grained writer configuration.

## New Features

### 🗜️ **Compression Codecs**
- **ZSTD**: High compression ratio with good performance
- **LZ4**: Ultra-fast compression/decompression
- **Snappy**: Balanced speed and compression (default)
- **Gzip**: Standard compression with configurable levels
- **Brotli**: High compression ratio for web applications
- **LZO**: Fast compression (legacy support)
- **Uncompressed**: No compression for maximum speed

### 📊 **Per-Column Encodings**
- **DELTA_BINARY_PACKED**: Optimal for sorted integers/timestamps
- **DICTIONARY**: Excellent for strings with low cardinality
- **DELTA_LENGTH_BYTE_ARRAY**: Efficient for variable-length binary data
- **BYTE_STREAM_SPLIT**: Specialized for floating-point data
- **PLAIN**: Simple encoding for all data types
- **RLE**: Run-length encoding for repetitive data

### ⚙️ **Writer Configuration**
- Row group size control
- Compression level tuning
- Statistics and bloom filter configuration
- Page size optimization
- Dictionary encoding control

## API Reference

### Enhanced ColumnDef Structure

```c
typedef struct {
  const char *name;                // Column name
  const char *format;              // Arrow format string
  bool nullable;                   // Nullable flag
  ParquetEncoding encoding;        // Per-column encoding
  ParquetCompression compression;  // Per-column compression (optional)
  bool use_dictionary;            // Per-column dictionary enable/disable
  bool enable_bloom_filter;       // Per-column bloom filter
  bool enable_statistics;         // Per-column statistics
} ColumnDef;
```

### Writer Options

```c
typedef struct {
  ParquetCompression compression;     // Global compression codec
  CompressionLevels compression_levels; // Compression level settings
  uint32_t row_group_size;           // Rows per row group
  bool enable_dictionary;            // Global dictionary setting
  bool enable_statistics;            // Global statistics setting
  bool enable_bloom_filter;          // Global bloom filter setting
  uint32_t max_row_group_size;       // Max row group size in bytes
  uint32_t data_page_size;           // Data page size
  uint32_t dict_page_size;           // Dictionary page size
  bool enable_page_index;            // Page index for filtering
  bool enable_column_index;          // Column index for filtering
} WriterOptions;
```

### Compression Levels

```c
typedef struct {
  int32_t gzip_level;    // 1-9, default 6
  int32_t brotli_level;  // 1-11, default 1
  int32_t zstd_level;    // 1-22, default 3
} CompressionLevels;
```

## Usage Examples

### 1. Basic Usage with Compression

```c
#include "parquet_writer_zerocopy.h"

// Simple schema with automatic encoding selection
static const ColumnDef SCHEMA[] = {
    COLUMN_DEF("id", "i", false),        // int32 with delta encoding
    COLUMN_DEF("name", "u", false),      // string with dictionary encoding
    COLUMN_DEF("price", "g", true)       // double with byte stream split
};

int main() {
    // Create writer with ZSTD compression
    StreamWriter *writer = create_writer_with_compression(
        "output.parquet", 10000, SCHEMA, 3, PARQUET_COMPRESSION_ZSTD);
    
    // Add data...
    close_writer(writer);
    free_writer(writer);
    return 0;
}
```

### 2. Advanced Configuration

```c
// Custom schema with specific encodings
static const ColumnDef ADVANCED_SCHEMA[] = {
    {
        .name = "timestamp",
        .format = "L",
        .nullable = false,
        .encoding = PARQUET_ENCODING_DELTA_BINARY_PACKED,
        .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
        .use_dictionary = false,
        .enable_bloom_filter = false,
        .enable_statistics = true
    },
    {
        .name = "symbol",
        .format = "u",
        .nullable = false,
        .encoding = PARQUET_ENCODING_DICTIONARY,
        .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
        .use_dictionary = true,
        .enable_bloom_filter = true,
        .enable_statistics = true
    }
};

int main() {
    // Create custom writer options
    WriterOptions options = create_default_writer_options();
    options.compression = PARQUET_COMPRESSION_ZSTD;
    options.compression_levels.zstd_level = 9;  // Maximum compression
    options.row_group_size = 50000;
    options.enable_bloom_filter = true;
    
    StreamWriter *writer = create_writer_with_options(
        "advanced.parquet", 10000, ADVANCED_SCHEMA, 2, options);
    
    // Add data...
    close_writer(writer);
    free_writer(writer);
    return 0;
}
```

### 3. Performance Optimization Example

```c
// High-performance configuration
static const ColumnDef PERF_SCHEMA[] = {
    COLUMN_DEF_WITH_ENCODING("id", "i", false, PARQUET_ENCODING_DELTA_BINARY_PACKED),
    COLUMN_DEF_WITH_ENCODING("data", "z", false, PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY)
};

int main() {
    WriterOptions options = create_default_writer_options();
    options.compression = PARQUET_COMPRESSION_LZ4;  // Fastest compression
    options.row_group_size = 1000000;               // Large row groups
    options.enable_dictionary = false;              // Disable for speed
    options.enable_statistics = false;              // Disable for speed
    options.enable_bloom_filter = false;            // Disable for speed
    
    StreamWriter *writer = create_writer_with_options(
        "fast.parquet", 100000, PERF_SCHEMA, 2, options);
    
    // Add data...
    return 0;
}
```

## Compression Codec Comparison

| Codec | Speed | Compression Ratio | Use Case |
|-------|-------|------------------|----------|
| **LZ4** | ⭐⭐⭐⭐⭐ | ⭐⭐ | Real-time applications |
| **Snappy** | ⭐⭐⭐⭐ | ⭐⭐⭐ | Balanced performance (default) |
| **ZSTD** | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Storage optimization |
| **Gzip** | ⭐⭐ | ⭐⭐⭐⭐ | Standard compression |
| **Brotli** | ⭐⭐ | ⭐⭐⭐⭐⭐ | Web applications |
| **Uncompressed** | ⭐⭐⭐⭐⭐ | ⭐ | Maximum write speed |

## Encoding Recommendations

### Integer Data
```c
// Timestamps, IDs, counters
.encoding = PARQUET_ENCODING_DELTA_BINARY_PACKED
```

### String Data
```c
// Low cardinality strings (symbols, categories)
.encoding = PARQUET_ENCODING_DICTIONARY
.use_dictionary = true

// High cardinality strings (UUIDs, descriptions)
.encoding = PARQUET_ENCODING_PLAIN
.use_dictionary = false
```

### Binary Data
```c
// Variable-length binary data
.encoding = PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY
```

### Floating-Point Data
```c
// Float/double columns
.encoding = PARQUET_ENCODING_BYTE_STREAM_SPLIT
```

## Performance Tips

### 1. **Choose the Right Compression**
- **LZ4** for real-time streaming
- **ZSTD** for archival storage
- **Snappy** for general use

### 2. **Optimize Row Group Size**
- Larger row groups = better compression
- Smaller row groups = better query performance
- Sweet spot: 100K-1M rows

### 3. **Use Appropriate Encodings**
- Delta encoding for sorted data
- Dictionary encoding for categorical data
- Byte stream split for floating-point data

### 4. **Enable Features Selectively**
- Bloom filters: Only for frequently filtered columns
- Statistics: Enable for query optimization
- Dictionary: Only for low-cardinality strings

## Example Programs

### Basic Example
```bash
# Compile and run basic example
gcc write_stream.c -I../include -L../target/debug -lparquet_ffi -o write_stream
./write_stream output.parquet 100000 10000
```

### Advanced Example
```bash
# Compile and run advanced example with ZSTD compression
gcc write_stream_advanced.c -I../include -L../target/debug -lparquet_ffi -o write_stream_advanced
./write_stream_advanced advanced.parquet 100000 10000 zstd
```

### Performance Comparison
```bash
# Test different compression codecs
./write_stream_advanced test_lz4.parquet 1000000 50000 lz4
./write_stream_advanced test_snappy.parquet 1000000 50000 snappy
./write_stream_advanced test_zstd.parquet 1000000 50000 zstd

# Compare file sizes
ls -lh test_*.parquet
```

## Migration Guide

### From Basic to Advanced

**Old Code:**
```c
static const ColumnDef SCHEMA[] = {
    {"id", "i", false},
    {"name", "u", false}
};

StreamWriter *writer = create_writer("output.parquet", 10000, SCHEMA, 2);
```

**New Code:**
```c
static const ColumnDef SCHEMA[] = {
    COLUMN_DEF("id", "i", false),           // Uses optimal encoding
    COLUMN_DEF("name", "u", false)          // Uses dictionary encoding
};

// Option 1: Use defaults (backward compatible)
StreamWriter *writer = create_writer("output.parquet", 10000, SCHEMA, 2);

// Option 2: Specify compression
StreamWriter *writer = create_writer_with_compression(
    "output.parquet", 10000, SCHEMA, 2, PARQUET_COMPRESSION_ZSTD);

// Option 3: Full control
WriterOptions options = create_default_writer_options();
options.compression = PARQUET_COMPRESSION_ZSTD;
options.compression_levels.zstd_level = 6;
StreamWriter *writer = create_writer_with_options(
    "output.parquet", 10000, SCHEMA, 2, options);
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   - Ensure you're using the latest `parquet_writer_zerocopy.h`
   - Check that all enum values are properly defined

2. **Performance Issues**
   - Try different compression codecs
   - Adjust row group size
   - Disable unnecessary features (bloom filters, statistics)

3. **File Size Issues**
   - Use ZSTD or Brotli for maximum compression
   - Enable dictionary encoding for strings
   - Use appropriate encodings for data types

### Debug Information

Enable debug output to see encoding and compression choices:
```c
// Add this before creating the writer
printf("Using compression: %d\n", options.compression);
printf("Row group size: %u\n", options.row_group_size);
```

## Future Enhancements

- [ ] Support for nested data types
- [ ] Custom compression parameters per column
- [ ] Adaptive encoding selection
- [ ] Compression ratio reporting
- [ ] Performance profiling integration 