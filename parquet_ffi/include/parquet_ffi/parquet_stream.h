#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Include the core Arrow C Data Interface header
#include "parquet_ffi/arrow_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * == Enhanced Configuration Structures ==
 * ==========================================================================
 */

/**
 * Compression codecs supported by Parquet
 */
typedef enum {
  PARQUET_STREAM_COMPRESSION_UNCOMPRESSED = 0,
  PARQUET_STREAM_COMPRESSION_SNAPPY = 1,
  PARQUET_STREAM_COMPRESSION_GZIP = 2,
  PARQUET_STREAM_COMPRESSION_LZO = 3,
  PARQUET_STREAM_COMPRESSION_BROTLI = 4,
  PARQUET_STREAM_COMPRESSION_ZSTD = 5,
  PARQUET_STREAM_COMPRESSION_LZ4 = 6,
  PARQUET_STREAM_COMPRESSION_LZ4_RAW = 7
} ParquetStreamCompression;

/**
 * Encoding types for Parquet columns
 */
typedef enum {
  PARQUET_STREAM_ENCODING_PLAIN = 0,
  PARQUET_STREAM_ENCODING_DICTIONARY = 1,
  PARQUET_STREAM_ENCODING_RLE = 2,
  PARQUET_STREAM_ENCODING_BIT_PACKED = 3,
  PARQUET_STREAM_ENCODING_DELTA_BINARY_PACKED = 4,
  PARQUET_STREAM_ENCODING_DELTA_LENGTH_BYTE_ARRAY = 5,
  PARQUET_STREAM_ENCODING_DELTA_BYTE_ARRAY = 6,
  PARQUET_STREAM_ENCODING_RLE_DICTIONARY = 7,
  PARQUET_STREAM_ENCODING_BYTE_STREAM_SPLIT = 8
} ParquetStreamEncoding;

/**
 * Compression levels for codecs that support them
 */
typedef struct {
  int32_t gzip_level;   // 1-9, default 6
  int32_t brotli_level; // 1-11, default 1
  int32_t zstd_level;   // 1-22, default 3
} ParquetStreamCompressionLevels;

/**
 * Column-specific configuration
 */
typedef struct {
  const char *name;                     // Column name
  ParquetStreamEncoding encoding;       // Per-column encoding
  ParquetStreamCompression compression; // Per-column compression (optional)
  bool use_dictionary;                  // Per-column dictionary enable/disable
  bool enable_bloom_filter;             // Per-column bloom filter
  bool enable_statistics;               // Per-column statistics
} ParquetStreamColumnDef;

/**
 * Writer configuration options
 */
typedef struct {
  ParquetStreamCompression compression; // Global compression codec
  ParquetStreamCompressionLevels
      compression_levels;      // Compression level settings
  uint32_t row_group_size;     // Rows per row group
  bool enable_dictionary;      // Global dictionary setting
  bool enable_statistics;      // Global statistics setting
  bool enable_bloom_filter;    // Global bloom filter setting
  uint32_t max_row_group_size; // Max row group size in bytes
  uint32_t data_page_size;     // Data page size
  uint32_t dict_page_size;     // Dictionary page size
  bool enable_page_index;      // Page index for filtering
  bool enable_column_index;    // Column index for filtering
} ParquetStreamWriterOptions;

/* ==========================================================================
 * == FFI Functions for Arrow C Stream Interface based Parquet Interaction ==
 * ==========================================================================
 */

/**
 * Initialize Rust tracing/logging system for debugging
 *
 * This function initializes the Rust tracing system which enables debug output
 * from the Rust backend. It's safe to call multiple times - initialization
 * will only happen once.
 *
 * Call this function early in your application if you want to see debug output
 * from the parquet_ffi library. The logging level can be controlled with the
 * RUST_LOG environment variable (e.g., RUST_LOG=debug).
 *
 * @return 0 on success, non-zero on error (currently always returns 0)
 */
int parquet_ffi_init_tracing(void);

/**
 * Export a Parquet file to an Arrow C Stream Interface
 * This allows for efficient reading of Parquet data using the Arrow C Data
 * Interface
 *
 * @param path Path to the Parquet file
 * @param out_stream Output parameter to receive the Arrow array stream
 * @return 0 on success, non-zero on error (specific error codes TBD)
 */
int export_parquet_file_to_stream(const char *path,
                                  struct ArrowArrayStream *out_stream);

/**
 * Export a Parquet file to an Arrow C Stream Interface with custom batch size
 * This allows for efficient reading of Parquet data using the Arrow C Data
 * Interface with control over the batch size for performance tuning
 *
 * @param path Path to the Parquet file
 * @param out_stream Output parameter to receive the Arrow array stream
 * @param batch_size Number of rows per batch (must be positive)
 * @return 0 on success, non-zero on error (specific error codes TBD)
 */
int export_parquet_file_to_stream_with_batch_size(
    const char *path, struct ArrowArrayStream *out_stream, int batch_size);

/**
 * Initialize a streaming Parquet writer using data provided via an Arrow C
 * Stream.
 *
 * Takes ownership of the stream pointer ONLY if initialization fails early
 * (e.g., invalid path) or if reading the first batch fails. If initialization
 * succeeds (returning a non-null handle), the caller retains ownership of the
 * stream for subsequent calls to write_batch.
 *
 * @param stream_ptr A pointer to an FFI_ArrowArrayStream providing the data.
 *                   The first call to get_next() on this stream is used to
 *                   get the schema and the first batch.
 * @param path_ptr A C string representing the path to the output Parquet file.
 * @return An opaque pointer (void*) to the writer state if successful,
 *         otherwise NULL. The caller is responsible for calling
 *         parquet_stream_writer_close() on the returned handle.
 */
void *parquet_stream_writer_init(struct ArrowArrayStream *stream_ptr,
                                 const char *path_ptr);

/**
 * Initialize a streaming Parquet writer with advanced configuration options.
 *
 * @param stream_ptr A pointer to an FFI_ArrowArrayStream providing the data.
 * @param path_ptr A C string representing the path to the output Parquet file.
 * @param options Writer configuration options
 * @param column_defs Array of column-specific configurations (optional, can be
 * NULL)
 * @param num_columns Number of columns in column_defs array
 * @return An opaque pointer (void*) to the writer state if successful,
 *         otherwise NULL.
 */
void *parquet_stream_writer_init_with_options(
    struct ArrowArrayStream *stream_ptr, const char *path_ptr,
    const ParquetStreamWriterOptions *options,
    const ParquetStreamColumnDef *column_defs, size_t num_columns);

/**
 * Create default writer options with sensible defaults
 */
ParquetStreamWriterOptions parquet_stream_create_default_options(void);

/**
 * Write a subsequent batch of data from an Arrow C Stream to an initialized
 * Parquet writer.
 *
 * Takes ownership of the stream pointer ONLY if reading the batch fails.
 * If writing succeeds, the caller retains ownership of the stream.
 *
 * @param state_ptr_void The opaque pointer returned by
 * parquet_stream_writer_init.
 * @param stream_ptr A pointer to an FFI_ArrowArrayStream providing the next
 * batch. get_next() will be called on this stream.
 * @return 0 on success (or if the stream ends), -1 on error.
 */
int parquet_stream_writer_write_batch(void *state_ptr_void,
                                      struct ArrowArrayStream *stream_ptr);

/**
 * Close the streaming Parquet writer and release associated resources.
 *
 * Takes ownership of the state pointer and frees it.
 *
 * @param state_ptr_void The opaque pointer returned by
 * parquet_stream_writer_init.
 * @return 0 on success, -1 on error during closing.
 */
int parquet_stream_writer_close(void *state_ptr_void);

#ifdef __cplusplus
} // extern "C"
#endif