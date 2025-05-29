#ifndef PARQUET_WRITER_ZEROCOPY_H
#define PARQUET_WRITER_ZEROCOPY_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parquet_ffi/arrow_c.h"
#include <parquet_ffi/parquet_stream.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration and Constants
// =============================================================================

#define DEFAULT_BUFFER_SIZE (1024 * 1024 * 16)
#define MAX_BATCH_SIZE 1000000

#define FAIL_IF(cond, msg, ...)                                                \
  do {                                                                         \
    if (cond) {                                                                \
      fprintf(stderr, msg "\n", ##__VA_ARGS__);                                \
      return 0;                                                                \
    }                                                                          \
  } while (0)

// =============================================================================
// Compression and Encoding Types
// =============================================================================

/**
 * Compression codecs supported by Parquet
 */
typedef enum {
  PARQUET_COMPRESSION_UNCOMPRESSED = 0,
  PARQUET_COMPRESSION_SNAPPY = 1,
  PARQUET_COMPRESSION_GZIP = 2,
  PARQUET_COMPRESSION_LZO = 3,
  PARQUET_COMPRESSION_BROTLI = 4,
  PARQUET_COMPRESSION_ZSTD = 5,
  PARQUET_COMPRESSION_LZ4 = 6,
  PARQUET_COMPRESSION_LZ4_RAW = 7
} ParquetCompression;

/**
 * Encoding types for Parquet columns
 */
typedef enum {
  PARQUET_ENCODING_PLAIN = 0,
  PARQUET_ENCODING_DICTIONARY = 1,
  PARQUET_ENCODING_RLE = 2,
  PARQUET_ENCODING_BIT_PACKED = 3,
  PARQUET_ENCODING_DELTA_BINARY_PACKED = 4,
  PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY = 5,
  PARQUET_ENCODING_DELTA_BYTE_ARRAY = 6,
  PARQUET_ENCODING_RLE_DICTIONARY = 7,
  PARQUET_ENCODING_BYTE_STREAM_SPLIT = 8
} ParquetEncoding;

/**
 * Compression levels for codecs that support them
 */
typedef struct {
  int32_t gzip_level;   // 1-9, default 6
  int32_t brotli_level; // 1-11, default 1
  int32_t zstd_level;   // 1-22, default 3
} CompressionLevels;

/**
 * Writer configuration options
 */
typedef struct {
  ParquetCompression compression;
  CompressionLevels compression_levels;
  uint32_t row_group_size;
  bool enable_dictionary;
  bool enable_statistics;
  bool enable_bloom_filter;
  uint32_t max_row_group_size;
  uint32_t data_page_size;
  uint32_t dict_page_size;
  bool enable_page_index;
  bool enable_column_index;
} WriterOptions;

/**
 * Arrow format types for column definitions
 * These correspond to Arrow C Data Interface format strings
 */
typedef enum {
  ARROW_TYPE_BOOL = 'b',         // boolean
  ARROW_TYPE_INT8 = 'c',         // int8
  ARROW_TYPE_UINT8 = 'C',        // uint8
  ARROW_TYPE_INT16 = 's',        // int16
  ARROW_TYPE_UINT16 = 'S',       // uint16
  ARROW_TYPE_INT32 = 'i',        // int32
  ARROW_TYPE_UINT32 = 'I',       // uint32
  ARROW_TYPE_INT64 = 'l',        // int64
  ARROW_TYPE_UINT64 = 'L',       // uint64
  ARROW_TYPE_FLOAT = 'f',        // float32
  ARROW_TYPE_DOUBLE = 'g',       // float64
  ARROW_TYPE_STRING = 'u',       // utf8 string
  ARROW_TYPE_LARGE_STRING = 'U', // large utf8 string
  ARROW_TYPE_BINARY = 'z',       // binary
  ARROW_TYPE_LARGE_BINARY = 'Z'  // large binary
} ArrowType;

/**
 * Arrow format string constants for static initialization
 * Use these for static array initialization where constant expressions are
 * required
 */
#define ARROW_FORMAT_BOOL "b"
#define ARROW_FORMAT_INT8 "c"
#define ARROW_FORMAT_UINT8 "C"
#define ARROW_FORMAT_INT16 "s"
#define ARROW_FORMAT_UINT16 "S"
#define ARROW_FORMAT_INT32 "i"
#define ARROW_FORMAT_UINT32 "I"
#define ARROW_FORMAT_INT64 "l"
#define ARROW_FORMAT_UINT64 "L"
#define ARROW_FORMAT_FLOAT "f"
#define ARROW_FORMAT_DOUBLE "g"
#define ARROW_FORMAT_STRING "u"
#define ARROW_FORMAT_LARGE_STRING "U"
#define ARROW_FORMAT_BINARY "z"
#define ARROW_FORMAT_LARGE_BINARY "Z"

/**
 * Convert ArrowType enum to format string (for dynamic use)
 */
static inline const char *arrow_type_to_format(ArrowType type) {
  static char format[2] = {0, 0};
  format[0] = (char)type;
  return format;
}

// =============================================================================
// Schema Definition
// =============================================================================

typedef struct {
  const char *name;
  const char *format;
  bool nullable;
  ParquetEncoding encoding;       // Per-column encoding
  ParquetCompression compression; // Per-column compression (optional)
  bool use_dictionary;            // Per-column dictionary enable/disable
  bool enable_bloom_filter;       // Per-column bloom filter
  bool enable_statistics;         // Per-column statistics
} ColumnDef;

// =============================================================================
// Data Types and Structures
// =============================================================================

// Zero-copy buffer for variable-length data
typedef struct {
  uint8_t *data;          // Raw data buffer (Arrow-compatible)
  int32_t *offsets;       // Offset array (Arrow-compatible)
  size_t data_used;       // Bytes used in data buffer
  size_t data_capacity;   // Total capacity of data buffer
  size_t offset_count;    // Number of offsets stored
  size_t offset_capacity; // Capacity of offset array
} ZeroCopyBuffer;

typedef struct {
  void **column_buffers;
  bool **null_flags;
  ZeroCopyBuffer *var_buffers; // One per variable-length column
  int64_t row_count;
  int64_t capacity;
} BatchData;

typedef struct {
  const char *filename;
  BatchData batch;
  void *writer_handle;
  int64_t total_rows;
  const ColumnDef *schema;
  size_t num_columns;
  WriterOptions options;
  bool stream_consumed; // Track if stream has been consumed
} StreamWriter;

// =============================================================================
// Configuration Helper Functions
// =============================================================================

/**
 * Create default writer options with sensible defaults
 */
static inline WriterOptions create_default_writer_options() {
  WriterOptions options;
  memset(&options, 0, sizeof(WriterOptions));
  options.compression = PARQUET_COMPRESSION_SNAPPY;
  options.compression_levels.gzip_level = 6;
  options.compression_levels.brotli_level = 1;
  options.compression_levels.zstd_level = 3;
  options.row_group_size = 1048576; // 1M rows
  options.enable_dictionary = true;
  options.enable_statistics = true;
  options.enable_bloom_filter = false;
  options.max_row_group_size = 134217728; // 128MB
  options.data_page_size = 1048576;       // 1MB
  options.dict_page_size = 1048576;       // 1MB
  options.enable_page_index = false;
  options.enable_column_index = false;
  return options;
}

/**
 * Create default column definition with sensible defaults
 */
static inline ColumnDef create_column_def(const char *name, const char *format,
                                          bool nullable) {
  ColumnDef col;
  memset(&col, 0, sizeof(ColumnDef));
  col.name = name;
  col.format = format;
  col.nullable = nullable;

  // Set default encoding based on data type
  switch (format[0]) {
  case 'i': // int32
  case 'l': // int64
  case 'I': // uint32
  case 'L': // uint64
    col.encoding = PARQUET_ENCODING_DELTA_BINARY_PACKED;
    break;
  case 'u': // string
  case 'U': // large string
    col.encoding = PARQUET_ENCODING_DICTIONARY;
    col.use_dictionary = true;
    break;
  case 'z': // binary
  case 'Z': // large binary
    col.encoding = PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY;
    break;
  case 'f': // float
  case 'g': // double
    col.encoding = PARQUET_ENCODING_BYTE_STREAM_SPLIT;
    break;
  default:
    col.encoding = PARQUET_ENCODING_PLAIN;
    break;
  }

  col.compression =
      PARQUET_COMPRESSION_UNCOMPRESSED; // Use global compression by default
  col.use_dictionary =
      (format[0] == 'u' || format[0] == 'U'); // Enable for strings
  col.enable_bloom_filter = false;
  col.enable_statistics = true;

  return col;
}

/**
 * Helper macros for creating column definitions
 */
#define COLUMN_DEF(name, format, nullable)                                     \
  {.name = name,                                                               \
   .format = format,                                                           \
   .nullable = nullable,                                                       \
   .encoding = _get_default_encoding(format),                                  \
   .compression = PARQUET_COMPRESSION_UNCOMPRESSED,                            \
   .use_dictionary = _get_default_dictionary(format),                          \
   .enable_bloom_filter = false,                                               \
   .enable_statistics = true}

#define COLUMN_DEF_WITH_ENCODING(name, format, nullable, encoding)             \
  {.name = name,                                                               \
   .format = format,                                                           \
   .nullable = nullable,                                                       \
   .encoding = encoding,                                                       \
   .compression = PARQUET_COMPRESSION_UNCOMPRESSED,                            \
   .use_dictionary = false,                                                    \
   .enable_bloom_filter = false,                                               \
   .enable_statistics = true}

// Helper macros for default encoding selection
#define _get_default_encoding(format)                                          \
  ((format)[0] == 'i' || (format)[0] == 'l' || (format)[0] == 'I' ||           \
           (format)[0] == 'L'                                                  \
       ? PARQUET_ENCODING_DELTA_BINARY_PACKED                                  \
       : ((format)[0] == 'u' || (format)[0] == 'U'                             \
              ? PARQUET_ENCODING_DICTIONARY                                    \
              : ((format)[0] == 'z' || (format)[0] == 'Z'                      \
                     ? PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY                \
                     : ((format)[0] == 'f' || (format)[0] == 'g'               \
                            ? PARQUET_ENCODING_BYTE_STREAM_SPLIT               \
                            : PARQUET_ENCODING_PLAIN))))

#define _get_default_dictionary(format)                                        \
  ((format)[0] == 'u' || (format)[0] == 'U')

// =============================================================================
// Utility Functions
// =============================================================================

static inline size_t get_type_size(const char *format) {
  switch (format[0]) {
  case 'b':
    return sizeof(bool);
  case 'c':
    return sizeof(int8_t);
  case 'C':
    return sizeof(uint8_t);
  case 's':
    return sizeof(int16_t);
  case 'S':
    return sizeof(uint16_t);
  case 'i':
    return sizeof(int32_t);
  case 'I':
    return sizeof(uint32_t);
  case 'l':
    return sizeof(int64_t);
  case 'L':
    return sizeof(uint64_t);
  case 'f':
    return sizeof(float);
  case 'g':
    return sizeof(double);
  case 'u':
  case 'U':
    return sizeof(int32_t); // string offset
  case 'z':
  case 'Z':
    return 0; // Variable-length, no fixed size
  default:
    return 0;
  }
}

static inline bool is_variable_type(const char *format) {
  return format[0] == 'u' || format[0] == 'U' || format[0] == 'z' ||
         format[0] == 'Z';
}

// =============================================================================
// Zero-Copy Buffer Management
// =============================================================================

static inline int init_zerocopy_buffer(ZeroCopyBuffer *buf,
                                       size_t initial_data_capacity,
                                       size_t initial_offset_capacity) {
  memset(buf, 0, sizeof(ZeroCopyBuffer));

  buf->data = (uint8_t *)malloc(initial_data_capacity);
  buf->offsets = (int32_t *)malloc(initial_offset_capacity * sizeof(int32_t));

  if (!buf->data || !buf->offsets) {
    free(buf->data);
    free(buf->offsets);
    return 0;
  }

  buf->data_capacity = initial_data_capacity;
  buf->offset_capacity = initial_offset_capacity;
  buf->offsets[0] = 0; // First offset is always 0
  buf->offset_count = 1;

  return 1;
}

static inline void free_zerocopy_buffer(ZeroCopyBuffer *buf) {
  if (!buf)
    return;
  free(buf->data);
  free(buf->offsets);
  memset(buf, 0, sizeof(ZeroCopyBuffer));
}

static inline int ensure_zerocopy_data_capacity(ZeroCopyBuffer *buf,
                                                size_t needed) {
  if (buf->data_used + needed <= buf->data_capacity)
    return 1;

  size_t new_capacity = buf->data_capacity * 2;
  if (new_capacity < buf->data_used + needed) {
    new_capacity = buf->data_used + needed + DEFAULT_BUFFER_SIZE;
  }

  uint8_t *new_data = (uint8_t *)realloc(buf->data, new_capacity);
  if (!new_data)
    return 0;

  buf->data = new_data;
  buf->data_capacity = new_capacity;
  return 1;
}

static inline int ensure_zerocopy_offset_capacity(ZeroCopyBuffer *buf,
                                                  size_t needed_offsets) {
  if (buf->offset_count + needed_offsets <= buf->offset_capacity)
    return 1;

  size_t new_capacity = buf->offset_capacity * 2;
  if (new_capacity < buf->offset_count + needed_offsets) {
    new_capacity = buf->offset_count + needed_offsets + 1000;
  }

  int32_t *new_offsets =
      (int32_t *)realloc(buf->offsets, new_capacity * sizeof(int32_t));
  if (!new_offsets)
    return 0;

  buf->offsets = new_offsets;
  buf->offset_capacity = new_capacity;
  return 1;
}

// =============================================================================
// Batch Data Management
// =============================================================================

static inline int init_batch_data(BatchData *batch, int64_t capacity,
                                  const ColumnDef *schema, size_t num_columns) {
  memset(batch, 0, sizeof(BatchData));
  batch->capacity = capacity;

  // Allocate column buffers
  batch->column_buffers = (void **)calloc(num_columns, sizeof(void *));
  batch->null_flags = (bool **)calloc(num_columns, sizeof(bool *));
  batch->var_buffers =
      (ZeroCopyBuffer *)calloc(num_columns, sizeof(ZeroCopyBuffer));

  if (!batch->column_buffers || !batch->null_flags || !batch->var_buffers)
    return 0;

  // Allocate individual column buffers
  for (size_t i = 0; i < num_columns; i++) {
    if (is_variable_type(schema[i].format)) {
      // Initialize zero-copy buffer for variable-length data
      if (!init_zerocopy_buffer(&batch->var_buffers[i], DEFAULT_BUFFER_SIZE,
                                capacity + 1)) {
        return 0;
      }
    } else {
      // Fixed-length column
      size_t element_size = get_type_size(schema[i].format);
      batch->column_buffers[i] = malloc(capacity * element_size);
      if (!batch->column_buffers[i])
        return 0;
    }

    if (schema[i].nullable) {
      batch->null_flags[i] = (bool *)calloc(capacity, sizeof(bool));
      if (!batch->null_flags[i])
        return 0;
    }
  }

  return 1;
}

static inline void free_batch_data(BatchData *batch, const ColumnDef *schema,
                                   size_t num_columns) {
  if (!batch)
    return;

  if (batch->column_buffers) {
    for (size_t i = 0; i < num_columns; i++) {
      if (is_variable_type(schema[i].format)) {
        free_zerocopy_buffer(&batch->var_buffers[i]);
      } else {
        free(batch->column_buffers[i]);
      }
      free(batch->null_flags[i]);
    }
    free(batch->column_buffers);
    free(batch->null_flags);
    free(batch->var_buffers);
  }

  memset(batch, 0, sizeof(BatchData));
}

static inline void reset_batch_data(BatchData *batch, const ColumnDef *schema,
                                    size_t num_columns) {
  batch->row_count = 0;

  // Reset zero-copy buffers
  for (size_t i = 0; i < num_columns; i++) {
    if (is_variable_type(schema[i].format)) {
      batch->var_buffers[i].data_used = 0;
      batch->var_buffers[i].offset_count = 1; // Keep first offset at 0
      batch->var_buffers[i].offsets[0] = 0;
    }
  }
}

// =============================================================================
// Zero-Copy Data Addition Functions
// =============================================================================

static inline int add_string_value_zerocopy(BatchData *batch, size_t col_idx,
                                            const char *value) {
  ZeroCopyBuffer *buf = &batch->var_buffers[col_idx];
  size_t len = strlen(value); // No +1 for null terminator in Arrow strings

  // Ensure capacity for data and one more offset
  if (!ensure_zerocopy_data_capacity(buf, len) ||
      !ensure_zerocopy_offset_capacity(buf, 1)) {
    return 0;
  }

  // Copy data directly to Arrow buffer
  memcpy(buf->data + buf->data_used, value, len);
  buf->data_used += len;

  // Add offset for next string
  buf->offsets[buf->offset_count] = buf->data_used;
  buf->offset_count++;

  return 1;
}

static inline int add_binary_value_zerocopy(BatchData *batch, size_t col_idx,
                                            const void *data, size_t size) {
  ZeroCopyBuffer *buf = &batch->var_buffers[col_idx];

  // Ensure capacity for data and one more offset
  if (!ensure_zerocopy_data_capacity(buf, size) ||
      !ensure_zerocopy_offset_capacity(buf, 1)) {
    return 0;
  }

  // Copy data directly to Arrow buffer
  memcpy(buf->data + buf->data_used, data, size);
  buf->data_used += size;

  // Add offset for next binary value
  buf->offsets[buf->offset_count] = buf->data_used;
  buf->offset_count++;

  return 1;
}

static inline int add_fixed_value(BatchData *batch, size_t col_idx,
                                  const void *value, const ColumnDef *schema) {
  size_t type_size = get_type_size(schema[col_idx].format);
  char *buffer = (char *)batch->column_buffers[col_idx];
  memcpy(buffer + batch->row_count * type_size, value, type_size);
  return 1;
}

// =============================================================================
// Arrow Schema and Array Creation
// =============================================================================

static inline void create_arrow_schema(struct ArrowSchema *schema,
                                       const ColumnDef *column_defs,
                                       size_t num_columns) {
  memset(schema, 0, sizeof(struct ArrowSchema));
  schema->format = strdup("+s");
  schema->name = strdup("");
  schema->n_children = num_columns;
  schema->children =
      (struct ArrowSchema **)malloc(num_columns * sizeof(struct ArrowSchema *));

  for (size_t i = 0; i < num_columns; i++) {
    schema->children[i] =
        (struct ArrowSchema *)malloc(sizeof(struct ArrowSchema));
    memset(schema->children[i], 0, sizeof(struct ArrowSchema));

    schema->children[i]->format = strdup(column_defs[i].format);
    schema->children[i]->name = strdup(column_defs[i].name);
    schema->children[i]->flags =
        column_defs[i].nullable ? ARROW_FLAG_NULLABLE : 0;
    schema->children[i]->release = NULL; // Will be set by release function
  }

  schema->release = NULL; // Will be set by release function
}

static inline uint8_t *create_validity_buffer(const bool *null_flags,
                                              int64_t num_rows,
                                              int64_t *null_count) {
  *null_count = 0;
  int64_t bytes = (num_rows + 7) / 8;
  uint8_t *validity = (uint8_t *)calloc(bytes, 1);
  if (!validity)
    return NULL;

  if (!null_flags) {
    memset(validity, 0xFF, bytes);
    return validity;
  }

  for (int64_t i = 0; i < num_rows; i++) {
    if (!null_flags[i]) {
      validity[i / 8] |= (1 << (i % 8));
    } else {
      (*null_count)++;
    }
  }
  return validity;
}

static inline int create_arrow_array_child(BatchData *batch, size_t col_idx,
                                           struct ArrowArray *child,
                                           const ColumnDef *schema) {
  const ColumnDef *col = &schema[col_idx];
  bool is_var = is_variable_type(col->format);

  child->length = batch->row_count;
  child->offset = 0;
  child->n_buffers = is_var ? 3 : 2;
  child->buffers = (const void **)calloc(child->n_buffers, sizeof(void *));
  if (!child->buffers)
    return 0;

  // Validity buffer
  int64_t null_count;
  child->buffers[0] = create_validity_buffer(batch->null_flags[col_idx],
                                             batch->row_count, &null_count);
  child->null_count = null_count;

  if (is_var) {
    // Variable-length: use zero-copy buffers directly
    ZeroCopyBuffer *buf = &batch->var_buffers[col_idx];

    // Use the pre-built offset array directly (zero-copy!)
    int32_t *offsets =
        (int32_t *)malloc((batch->row_count + 1) * sizeof(int32_t));
    if (!offsets)
      return 0;

    // Copy only the needed offsets (up to row_count + 1)
    memcpy(offsets, buf->offsets, (batch->row_count + 1) * sizeof(int32_t));
    child->buffers[1] = offsets;

    // Use the data buffer directly (zero-copy!)
    uint8_t *data_buffer = (uint8_t *)malloc(buf->data_used);
    if (!data_buffer)
      return 0;

    memcpy(data_buffer, buf->data, buf->data_used);
    child->buffers[2] = data_buffer;
  } else {
    // Fixed-length: just copy the data
    size_t type_size = get_type_size(col->format);
    size_t data_size = batch->row_count * type_size;
    void *data_buffer = malloc(data_size);
    if (!data_buffer)
      return 0;

    memcpy(data_buffer, batch->column_buffers[col_idx], data_size);
    child->buffers[1] = data_buffer;
  }

  return 1;
}

// =============================================================================
// Arrow Stream Implementation
// =============================================================================

static void release_schema(struct ArrowSchema *schema);
static void release_array(struct ArrowArray *array);

static inline void release_schema(struct ArrowSchema *schema) {
  if (!schema || !schema->release)
    return;

  if (schema->children) {
    for (int64_t i = 0; i < schema->n_children; i++) {
      if (schema->children[i]) {
        if (schema->children[i]->release) {
          schema->children[i]->release(schema->children[i]);
        }
        free(schema->children[i]);
      }
    }
    free(schema->children);
  }

  free((char *)schema->format);
  free((char *)schema->name);
  schema->release = NULL;
}

static inline void release_array(struct ArrowArray *array) {
  if (!array || !array->release)
    return;

  if (array->children) {
    for (int64_t i = 0; i < array->n_children; i++) {
      if (array->children[i]) {
        if (array->children[i]->release) {
          array->children[i]->release(array->children[i]);
        }
        free(array->children[i]);
      }
    }
    free(array->children);
  }

  if (array->buffers) {
    for (int64_t i = 0; i < array->n_buffers; i++) {
      free((void *)array->buffers[i]);
    }
    free(array->buffers);
  }

  array->release = NULL;
}

static inline int get_schema(struct ArrowArrayStream *stream,
                             struct ArrowSchema *out_schema) {
  StreamWriter *writer = (StreamWriter *)stream->private_data;
  create_arrow_schema(out_schema, writer->schema, writer->num_columns);
  out_schema->release = release_schema;
  return 0;
}

static inline int get_next(struct ArrowArrayStream *stream,
                           struct ArrowArray *out_array) {
  StreamWriter *writer = (StreamWriter *)stream->private_data;

  // Check if stream has already been consumed
  if (writer->stream_consumed) {
    // Signal end-of-stream by setting release to NULL
    memset(out_array, 0, sizeof(struct ArrowArray));
    out_array->release = NULL; // This signals end-of-stream
    return 0;
  }

  // Check if there's data to return
  if (writer->batch.row_count == 0) {
    // No data, signal end-of-stream
    memset(out_array, 0, sizeof(struct ArrowArray));
    out_array->release = NULL;
    return 0;
  }

  // Create array
  memset(out_array, 0, sizeof(struct ArrowArray));
  out_array->length = writer->batch.row_count;
  out_array->n_children = writer->num_columns;
  out_array->n_buffers = 1;
  out_array->buffers = (const void **)calloc(1, sizeof(void *));
  out_array->children = (struct ArrowArray **)calloc(
      writer->num_columns, sizeof(struct ArrowArray *));

  if (!out_array->buffers || !out_array->children)
    return -1;

  // Create child arrays
  for (size_t i = 0; i < writer->num_columns; i++) {
    out_array->children[i] =
        (struct ArrowArray *)calloc(1, sizeof(struct ArrowArray));
    if (!out_array->children[i])
      return -1;

    if (!create_arrow_array_child(&writer->batch, i, out_array->children[i],
                                  writer->schema)) {
      return -1;
    }
    out_array->children[i]->release = release_array;
  }

  out_array->release = release_array;

  // Mark stream as consumed
  writer->stream_consumed = true;

  // Reset batch for next use
  reset_batch_data(&writer->batch, writer->schema, writer->num_columns);
  return 0;
}

static inline const char *get_last_error(struct ArrowArrayStream *stream) {
  return NULL;
}

static inline void release_stream(struct ArrowArrayStream *stream) {
  if (!stream->release)
    return;
  stream->private_data = NULL;
  stream->release = NULL;
}

// =============================================================================
// Writer Implementation
// =============================================================================

/**
 * Create a writer with custom options
 */
static inline StreamWriter *create_writer_with_options(const char *filename,
                                                       int64_t batch_size,
                                                       const ColumnDef *schema,
                                                       size_t num_columns,
                                                       WriterOptions options) {
  StreamWriter *writer = (StreamWriter *)calloc(1, sizeof(StreamWriter));
  if (!writer)
    return NULL;

  writer->filename = strdup(filename);
  if (!writer->filename) {
    free(writer);
    return NULL;
  }

  writer->schema = schema;
  writer->num_columns = num_columns;
  writer->options = options;

  if (!init_batch_data(&writer->batch, batch_size, schema, num_columns)) {
    free((char *)writer->filename);
    free(writer);
    return NULL;
  }

  return writer;
}

/**
 * Create a writer with default options (backward compatibility)
 */
static inline StreamWriter *create_writer(const char *filename,
                                          int64_t batch_size,
                                          const ColumnDef *schema,
                                          size_t num_columns) {
  WriterOptions default_options = create_default_writer_options();
  return create_writer_with_options(filename, batch_size, schema, num_columns,
                                    default_options);
}

/**
 * Create a writer with compression codec
 */
static inline StreamWriter *
create_writer_with_compression(const char *filename, int64_t batch_size,
                               const ColumnDef *schema, size_t num_columns,
                               ParquetCompression compression) {
  WriterOptions options = create_default_writer_options();
  options.compression = compression;
  return create_writer_with_options(filename, batch_size, schema, num_columns,
                                    options);
}

static inline void free_writer(StreamWriter *writer) {
  if (!writer)
    return;

  if (writer->writer_handle) {
    parquet_stream_writer_close(writer->writer_handle);
  }

  free_batch_data(&writer->batch, writer->schema, writer->num_columns);
  free((char *)writer->filename);
  free(writer);
}

// Forward declaration
static int flush_writer(StreamWriter *writer);

static inline int add_row(StreamWriter *writer, const void **values,
                          const bool *nulls, const size_t *sizes) {
  BatchData *batch = &writer->batch;

  // Check if adding this row would make the batch full
  // (flush before adding if we're at capacity)
  if (batch->row_count >= batch->capacity) {
    if (!flush_writer(writer))
      return 0;
    // After flush, the batch should be empty, but we need to wait
    // for the stream to be consumed. For now, we'll trust that
    // the backend consumes the stream synchronously.
  }

  // Add values to columns
  for (size_t i = 0; i < writer->num_columns; i++) {
    bool is_null = writer->schema[i].nullable && nulls[i];

    // Set null flag
    if (writer->schema[i].nullable) {
      batch->null_flags[i][batch->row_count] = is_null;
    } else if (is_null) {
      fprintf(stderr, "Cannot add NULL to non-nullable column %zu\n", i);
      return 0;
    }

    // Add data if not null
    if (!is_null) {
      const char *format = writer->schema[i].format;

      if (format[0] == 'u' || format[0] == 'U') {
        if (!add_string_value_zerocopy(batch, i, (const char *)values[i]))
          return 0;
      } else if (format[0] == 'z' || format[0] == 'Z') {
        if (!add_binary_value_zerocopy(batch, i, values[i], sizes[i]))
          return 0;
      } else {
        if (!add_fixed_value(batch, i, values[i], writer->schema))
          return 0;
      }
    } else {
      // Clear the slot for null values
      const char *format = writer->schema[i].format;
      size_t element_size = get_type_size(format);
      if (element_size > 0) {
        void *dest =
            (char *)batch->column_buffers[i] + batch->row_count * element_size;
        memset(dest, 0, element_size);
      }
    }
  }

  batch->row_count++;
  return 1;
}

// =============================================================================
// Backend Integration Functions
// =============================================================================

// Convert from enhanced library types to stream backend types
static inline ParquetStreamCompression
to_stream_compression(ParquetCompression compression) {
  switch (compression) {
  case PARQUET_COMPRESSION_UNCOMPRESSED:
    return PARQUET_STREAM_COMPRESSION_UNCOMPRESSED;
  case PARQUET_COMPRESSION_SNAPPY:
    return PARQUET_STREAM_COMPRESSION_SNAPPY;
  case PARQUET_COMPRESSION_GZIP:
    return PARQUET_STREAM_COMPRESSION_GZIP;
  case PARQUET_COMPRESSION_LZO:
    return PARQUET_STREAM_COMPRESSION_LZO;
  case PARQUET_COMPRESSION_BROTLI:
    return PARQUET_STREAM_COMPRESSION_BROTLI;
  case PARQUET_COMPRESSION_ZSTD:
    return PARQUET_STREAM_COMPRESSION_ZSTD;
  case PARQUET_COMPRESSION_LZ4:
    return PARQUET_STREAM_COMPRESSION_LZ4;
  case PARQUET_COMPRESSION_LZ4_RAW:
    return PARQUET_STREAM_COMPRESSION_LZ4_RAW;
  default:
    return PARQUET_STREAM_COMPRESSION_SNAPPY;
  }
}

static inline ParquetStreamEncoding
to_stream_encoding(ParquetEncoding encoding) {
  switch (encoding) {
  case PARQUET_ENCODING_PLAIN:
    return PARQUET_STREAM_ENCODING_PLAIN;
  case PARQUET_ENCODING_DICTIONARY:
    return PARQUET_STREAM_ENCODING_DICTIONARY;
  case PARQUET_ENCODING_RLE:
    return PARQUET_STREAM_ENCODING_RLE;
  case PARQUET_ENCODING_BIT_PACKED:
    return PARQUET_STREAM_ENCODING_BIT_PACKED;
  case PARQUET_ENCODING_DELTA_BINARY_PACKED:
    return PARQUET_STREAM_ENCODING_DELTA_BINARY_PACKED;
  case PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY:
    return PARQUET_STREAM_ENCODING_DELTA_LENGTH_BYTE_ARRAY;
  case PARQUET_ENCODING_DELTA_BYTE_ARRAY:
    return PARQUET_STREAM_ENCODING_DELTA_BYTE_ARRAY;
  case PARQUET_ENCODING_RLE_DICTIONARY:
    return PARQUET_STREAM_ENCODING_RLE_DICTIONARY;
  case PARQUET_ENCODING_BYTE_STREAM_SPLIT:
    return PARQUET_STREAM_ENCODING_BYTE_STREAM_SPLIT;
  default:
    return PARQUET_STREAM_ENCODING_PLAIN;
  }
}

static inline int flush_writer(StreamWriter *writer) {
  if (writer->batch.row_count == 0)
    return 1;

  int64_t rows_in_batch = writer->batch.row_count;

  // Reset stream consumed flag for this flush
  writer->stream_consumed = false;

  // Set up Arrow stream
  struct ArrowArrayStream stream = {.get_schema = get_schema,
                                    .get_next = get_next,
                                    .get_last_error = get_last_error,
                                    .release = release_stream,
                                    .private_data = writer};

  int result;
  if (!writer->writer_handle) {
    // First flush: initialize writer with enhanced options
    printf("Initializing writer with compression: %d, dictionary: %s, "
           "statistics: %s (%ld rows)...\n",
           writer->options.compression,
           writer->options.enable_dictionary ? "enabled" : "disabled",
           writer->options.enable_statistics ? "enabled" : "disabled",
           rows_in_batch);

    // Convert enhanced library options to stream backend options
    ParquetStreamWriterOptions stream_options;
    stream_options.compression =
        to_stream_compression(writer->options.compression);
    stream_options.compression_levels.gzip_level =
        writer->options.compression_levels.gzip_level;
    stream_options.compression_levels.brotli_level =
        writer->options.compression_levels.brotli_level;
    stream_options.compression_levels.zstd_level =
        writer->options.compression_levels.zstd_level;
    stream_options.row_group_size = writer->options.row_group_size;
    stream_options.enable_dictionary = writer->options.enable_dictionary;
    stream_options.enable_statistics = writer->options.enable_statistics;
    stream_options.enable_bloom_filter = writer->options.enable_bloom_filter;
    stream_options.max_row_group_size = writer->options.max_row_group_size;
    stream_options.data_page_size = writer->options.data_page_size;
    stream_options.dict_page_size = writer->options.dict_page_size;
    stream_options.enable_page_index = writer->options.enable_page_index;
    stream_options.enable_column_index = writer->options.enable_column_index;

    // Convert column definitions if available
    ParquetStreamColumnDef *stream_columns = NULL;
    if (writer->schema && writer->num_columns > 0) {
      stream_columns = (ParquetStreamColumnDef *)malloc(
          writer->num_columns * sizeof(ParquetStreamColumnDef));
      if (stream_columns) {
        for (size_t i = 0; i < writer->num_columns; i++) {
          stream_columns[i].name = writer->schema[i].name;
          stream_columns[i].encoding =
              to_stream_encoding(writer->schema[i].encoding);
          stream_columns[i].compression =
              to_stream_compression(writer->schema[i].compression);
          stream_columns[i].use_dictionary = writer->schema[i].use_dictionary;
          stream_columns[i].enable_bloom_filter =
              writer->schema[i].enable_bloom_filter;
          stream_columns[i].enable_statistics =
              writer->schema[i].enable_statistics;
        }
      }
    }

    // Initialize writer with enhanced options
    writer->writer_handle = parquet_stream_writer_init_with_options(
        &stream, writer->filename, &stream_options, stream_columns,
        writer->num_columns);

    // Clean up temporary column definitions
    free(stream_columns);

    result = writer->writer_handle ? 0 : -1;

    // For the first batch, we need to also write it
    if (result == 0) {
      result =
          parquet_stream_writer_write_batch(writer->writer_handle, &stream);
    }
  } else {
    // Subsequent flush
    result = parquet_stream_writer_write_batch(writer->writer_handle, &stream);
  }

  if (result == 0) {
    writer->total_rows += rows_in_batch;
  }

  return result == 0;
}

static inline int close_writer(StreamWriter *writer) {
  int flush_result = 1;

  if (writer->batch.row_count > 0) {
    printf("Flushing final batch (%ld rows)...\n", writer->batch.row_count);
    flush_result = flush_writer(writer);
  }

  int close_result = 0;
  if (writer->writer_handle) {
    printf("Closing writer...\n");
    close_result = parquet_stream_writer_close(writer->writer_handle);
    writer->writer_handle = NULL;
  }

  if (flush_result && close_result == 0) {
    printf("Successfully wrote %ld total rows to %s\n", writer->total_rows,
           writer->filename);
  }

  return flush_result && close_result == 0;
}

#ifdef __cplusplus
}
#endif

#endif // PARQUET_WRITER_ZEROCOPY_H