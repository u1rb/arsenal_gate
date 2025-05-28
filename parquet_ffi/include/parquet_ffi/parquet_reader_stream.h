#ifndef PARQUET_READER_STREAM_H
#define PARQUET_READER_STREAM_H

// =============================================================================
// Parquet Reader Stream - Simplified Header-Only Library with Zero-Copy
// Optimizations
// =============================================================================
//
// This library provides a high-performance, zero-copy interface for reading
// Parquet files with support for:
// - Single file reading with batch processing
// - Multi-file merging with min-heap sorting
// - Unified data access for fixed-size types
// - Zero-copy access for variable-length types (strings, binary)
// - Memory-efficient streaming with automatic cleanup
//
// Usage:
//   #include "parquet_reader_stream.h"
//
//   // Single file
//   PacketStream *stream = parquet_reader_init_stream("file.parquet");
//
//   // Multiple files (sorted merge)
//   const char *files[] = {"file1.parquet", "file2.parquet"};
//   PacketStream *stream = parquet_merger_init_stream(files, 2, 0);
//
//   // Read data with unified and zero-copy access
//   while (packet_stream_next(stream)) {
//     // Fixed-size types (unified getter)
//     int32_t id = packet_stream_get_value_int32(stream, 0);
//     double value = packet_stream_get_value_double(stream, 1);
//
//     // Variable-length types (zero-copy only)
//     const char *name;
//     size_t name_len;
//     packet_stream_get_string_zerocopy(stream, 2, &name, &name_len);
//
//     const uint8_t *data;
//     size_t data_len;
//     packet_stream_get_binary_zerocopy(stream, 3, &data, &data_len);
//   }
//
//   // Cleanup
//   packet_stream_release(stream);
//
// =============================================================================

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

// Include the Parquet FFI library
#include <parquet_ffi/parquet_stream.h>

// =============================================================================
// Type Definitions and Structures
// =============================================================================

typedef struct PacketStream PacketStream; // Forward declaration

// Define stream types for dispatch
typedef enum {
  PS_PARQUET_SINGLE, // Single parquet file reader
  PS_PARQUET_MERGER  // Merger of multiple parquet readers
} PsKind;

// Forward declarations of concrete state structures
typedef struct ParquetReader ParquetReader;
typedef struct ParquetMerger ParquetMerger;

// Single Parquet File Reader Implementation
struct ParquetReader {
  struct ArrowArrayStream arrow_stream; // The underlying Arrow stream
  struct ArrowSchema schema;            // Schema from the stream
  struct ArrowArray current_batch;      // Current record batch
  int64_t next_row_in_batch;            // Index of next row to read
  int64_t batch_number;                 // Current batch number
  bool stream_ended;                    // Flag indicating end of stream
  uint64_t total_rows_processed;        // Counter for processed rows
};

// Entry in our merger heap
typedef struct {
  int64_t key;           // Current key value (for sorting)
  uint32_t reader_index; // Which reader it belongs to
} HeapEntry;

// ParquetMerger struct to manage multiple readers
struct ParquetMerger {
  uint32_t K;                    // Number of input readers
  ParquetReader **readers;       // Array of reader pointers
  HeapEntry *heap;               // Min-heap of entries
  uint32_t heap_size;            // Current size of heap
  int key_column_index;          // Index of column to use as key
  ParquetReader *current_reader; // Current active reader
  bool is_int32_key; // Whether the key column is int32 (otherwise int64)
  uint64_t total_rows_processed; // Counter for total rows processed
  bool need_advance;             // Whether we need to advance current reader
};

// The stream structure with embedded union for concrete implementations
struct PacketStream {
  PsKind kind; // Type tag for dispatch
  union {
    ParquetReader *parquet_reader;
    ParquetMerger *parquet_merger;
  } impl; // Direct pointers to implementations
};

// =============================================================================
// Function Declarations
// =============================================================================

// Core stream operations
static bool parquet_reader_next(ParquetReader *pr);
static void parquet_reader_cleanup(ParquetReader *pr);
static bool parquet_merger_next(ParquetMerger *pm);
static void parquet_merger_cleanup(ParquetMerger *pm);

// Utility functions
static bool validate_column_index(ParquetReader *pr, int col_index);
static bool is_value_null(const uint8_t *validity, int64_t row_index);
static ParquetReader *get_current_reader(PacketStream *stream);

// Min-heap operations
static inline void swap_entries(HeapEntry *a, HeapEntry *b);
static void heapify_down(HeapEntry *heap, uint32_t heap_size, uint32_t index);
static void build_heap(HeapEntry *heap, uint32_t heap_size);
static int64_t get_reader_key(ParquetReader *pr,
                              int col_index,
                              bool is_int32_key);

// =============================================================================
// Optimized Stream Operations
// =============================================================================

// Optimized next function using dispatch
#define packet_stream_next(stream)                                             \
  ((stream)->kind == PS_PARQUET_SINGLE                                         \
       ? parquet_reader_next((stream)->impl.parquet_reader)                    \
       : parquet_merger_next((stream)->impl.parquet_merger))

// Helper function to release stream resources
static inline void packet_stream_release(PacketStream *stream) {
  if (stream) {
    switch (stream->kind) {
      case PS_PARQUET_SINGLE:
        parquet_reader_cleanup(stream->impl.parquet_reader);
        free(stream->impl.parquet_reader);
        break;
      case PS_PARQUET_MERGER:
        parquet_merger_cleanup(stream->impl.parquet_merger);
        free(stream->impl.parquet_merger);
        break;
    }
    free(stream);
  }
}

// =============================================================================
// Public API Functions
// =============================================================================

// Initialize a single Parquet file reader
static PacketStream *parquet_reader_init_stream(const char *file_path);

// Initialize a merger of multiple Parquet files
static PacketStream *parquet_merger_init_stream(const char **file_paths,
                                                uint32_t num_files,
                                                int key_column_index);

// Schema access functions
static const char *packet_stream_get_column_name(PacketStream *stream,
                                                 int col_index);
static const char *packet_stream_get_column_format(PacketStream *stream,
                                                   int col_index);
static int packet_stream_get_column_count(PacketStream *stream);

// Unified data access for fixed-size types
static bool packet_stream_is_null(PacketStream *stream, int col_index);

// Type-specific getters for fixed-size types (unified implementation)
static int32_t packet_stream_get_value_int32(PacketStream *stream,
                                             int col_index);
static int64_t packet_stream_get_value_int64(PacketStream *stream,
                                             int col_index);
static double packet_stream_get_value_double(PacketStream *stream,
                                             int col_index);
static float packet_stream_get_value_float(PacketStream *stream, int col_index);
static bool packet_stream_get_value_bool(PacketStream *stream, int col_index);
static int8_t packet_stream_get_value_int8(PacketStream *stream, int col_index);
static uint8_t packet_stream_get_value_uint8(PacketStream *stream,
                                             int col_index);
static int16_t packet_stream_get_value_int16(PacketStream *stream,
                                             int col_index);
static uint16_t packet_stream_get_value_uint16(PacketStream *stream,
                                               int col_index);
static uint32_t packet_stream_get_value_uint32(PacketStream *stream,
                                               int col_index);
static uint64_t packet_stream_get_value_uint64(PacketStream *stream,
                                               int col_index);

// Zero-copy data access for variable-length types
static bool packet_stream_get_string_zerocopy(PacketStream *stream,
                                              int col_index,
                                              const char **data,
                                              size_t *length);
static bool packet_stream_get_binary_zerocopy(PacketStream *stream,
                                              int col_index,
                                              const uint8_t **data,
                                              size_t *length);

// =============================================================================
// Implementation
// =============================================================================

// Get current reader (helper function to reduce code duplication)
static ParquetReader *get_current_reader(PacketStream *stream) {
  if (stream->kind == PS_PARQUET_SINGLE) {
    return stream->impl.parquet_reader;
  } else {
    ParquetMerger *pm = stream->impl.parquet_merger;
    return pm->current_reader;
  }
}

// Initialize a single Parquet file reader
static PacketStream *parquet_reader_init_stream(const char *file_path) {
  ParquetReader *pr = (ParquetReader *) malloc(sizeof(ParquetReader));
  if (!pr) {
    perror("Failed to allocate ParquetReader");
    return NULL;
  }

  // Initialize all fields
  memset(pr, 0, sizeof(ParquetReader));

  // Export the parquet file to the stream
  int ret = export_parquet_file_to_stream(file_path, &pr->arrow_stream);
  if (ret != 0) {
    fprintf(stderr,
            "Failed to export parquet file to stream: error code %d\n",
            ret);
    free(pr);
    return NULL;
  }

  // Get the schema
  ret = pr->arrow_stream.get_schema(&pr->arrow_stream, &pr->schema);
  if (ret != 0) {
    fprintf(stderr, "Failed to get schema: error code %d\n", ret);
    if (pr->arrow_stream.get_last_error) {
      fprintf(stderr,
              "Error: %s\n",
              pr->arrow_stream.get_last_error(&pr->arrow_stream));
    }
    pr->arrow_stream.release(&pr->arrow_stream);
    free(pr);
    return NULL;
  }

  // Get first batch
  ret = pr->arrow_stream.get_next(&pr->arrow_stream, &pr->current_batch);
  if (ret != 0) {
    fprintf(stderr, "Failed to get first batch: error code %d\n", ret);
    if (pr->arrow_stream.get_last_error) {
      fprintf(stderr,
              "Error: %s\n",
              pr->arrow_stream.get_last_error(&pr->arrow_stream));
    }
    pr->schema.release(&pr->schema);
    pr->arrow_stream.release(&pr->arrow_stream);
    free(pr);
    return NULL;
  }

  // Check if we got an empty batch (end of stream)
  if (pr->current_batch.release == NULL) {
    pr->stream_ended = true;
  } else {
    pr->batch_number = 1;
  }

  // Initialize counters
  pr->next_row_in_batch = 0;
  pr->total_rows_processed = 0;

  // Create the packet stream wrapper
  PacketStream *stream = (PacketStream *) malloc(sizeof(PacketStream));
  if (!stream) {
    perror("Failed to allocate PacketStream");
    if (!pr->stream_ended && pr->current_batch.release) {
      pr->current_batch.release(&pr->current_batch);
    }
    pr->schema.release(&pr->schema);
    pr->arrow_stream.release(&pr->arrow_stream);
    free(pr);
    return NULL;
  }

  stream->kind = PS_PARQUET_SINGLE;
  stream->impl.parquet_reader = pr;

  return stream;
}

// Advance to the next row
static bool parquet_reader_next(ParquetReader *pr) {
  // If stream already ended, no more rows
  if (pr->stream_ended) {
    return false;
  }

  // Check if we've reached the end of the current batch
  if (pr->next_row_in_batch >= pr->current_batch.length) {
    // Release current batch
    pr->current_batch.release(&pr->current_batch);

    // Get next batch
    int ret = pr->arrow_stream.get_next(&pr->arrow_stream, &pr->current_batch);
    if (ret != 0) {
      fprintf(stderr, "Failed to get next batch: error code %d\n", ret);
      if (pr->arrow_stream.get_last_error) {
        fprintf(stderr,
                "Error: %s\n",
                pr->arrow_stream.get_last_error(&pr->arrow_stream));
      }
      pr->stream_ended = true;
      return false;
    }

    // Check if we've reached the end of the stream
    if (pr->current_batch.release == NULL) {
      pr->stream_ended = true;
      return false;
    }

    // Reset row index for new batch
    pr->next_row_in_batch = 0;
    pr->batch_number++;
  }

  // Advance to next row
  pr->next_row_in_batch++;
  pr->total_rows_processed++;

  return true;
}

// Cleanup resources
static void parquet_reader_cleanup(ParquetReader *pr) {
  if (pr) {
    // Release current batch if active
    if (!pr->stream_ended && pr->current_batch.release) {
      pr->current_batch.release(&pr->current_batch);
    }

    // Release schema and stream
    if (pr->schema.release) {
      pr->schema.release(&pr->schema);
    }

    pr->arrow_stream.release(&pr->arrow_stream);
  }
}

// =============================================================================
// Utility Functions
// =============================================================================

// Helper function to check if column index is valid
static bool validate_column_index(ParquetReader *pr, int col_index) {
  if (col_index < 0 || col_index >= pr->schema.n_children) {
    fprintf(stderr,
            "Invalid column index %d (schema has %ld columns)\n",
            col_index,
            pr->schema.n_children);
    return false;
  }
  return true;
}

// Helper to check if a value is null
static bool is_value_null(const uint8_t *validity, int64_t row_index) {
  if (!validity)
    return false; // No validity buffer means all values are valid
  return !(validity[row_index / 8] & (1 << (row_index % 8)));
}

// =============================================================================
// Schema Access Functions
// =============================================================================

// Get column name from stream
static const char *packet_stream_get_column_name(PacketStream *stream,
                                                 int col_index) {
  if (stream->kind == PS_PARQUET_SINGLE) {
    ParquetReader *pr = stream->impl.parquet_reader;
    if (!validate_column_index(pr, col_index)) {
      return NULL;
    }
    return pr->schema.children[col_index]->name;
  } else {
    ParquetMerger *pm = stream->impl.parquet_merger;
    // For merger, use the schema from the first stream
    ParquetReader *pr = pm->readers[0];
    if (!validate_column_index(pr, col_index)) {
      return NULL;
    }
    return pr->schema.children[col_index]->name;
  }
}

// Get column format from stream
static const char *packet_stream_get_column_format(PacketStream *stream,
                                                   int col_index) {
  if (stream->kind == PS_PARQUET_SINGLE) {
    ParquetReader *pr = stream->impl.parquet_reader;
    if (!validate_column_index(pr, col_index)) {
      return NULL;
    }
    return pr->schema.children[col_index]->format;
  } else {
    ParquetMerger *pm = stream->impl.parquet_merger;
    // For merger, use the schema from the first stream
    ParquetReader *pr = pm->readers[0];
    if (!validate_column_index(pr, col_index)) {
      return NULL;
    }
    return pr->schema.children[col_index]->format;
  }
}

// Get number of columns in the stream
static int packet_stream_get_column_count(PacketStream *stream) {
  if (stream->kind == PS_PARQUET_SINGLE) {
    ParquetReader *pr = stream->impl.parquet_reader;
    return pr->schema.n_children;
  } else {
    ParquetMerger *pm = stream->impl.parquet_merger;
    // For merger, use the schema from the first stream
    ParquetReader *pr = pm->readers[0];
    if (!validate_column_index(pr, 0)) { // Just check if any columns exist
      return 0;
    }
    return pr->schema.n_children;
  }
}

// =============================================================================
// Unified Data Access Functions for Fixed-Size Types
// =============================================================================

// Check if the current row has a null value for the given column
static bool packet_stream_is_null(PacketStream *stream, int col_index) {
  ParquetReader *pr = get_current_reader(stream);
  if (!pr || !validate_column_index(pr, col_index)) {
    return true; // Return true (null) on error
  }

  struct ArrowArray *col_array = pr->current_batch.children[col_index];
  int64_t row_idx = pr->next_row_in_batch - 1;
  const uint8_t *validity = (const uint8_t *) col_array->buffers[0];
  return is_value_null(validity, row_idx);
}

// Unified getter macro for fixed-size types
#define DEFINE_FIXED_TYPE_GETTER(type_name, c_type, format_char)               \
  static c_type packet_stream_get_value_##type_name(PacketStream *stream,      \
                                                    int col_index) {           \
    ParquetReader *pr = get_current_reader(stream);                            \
    if (!pr || !validate_column_index(pr, col_index)) {                        \
      return (c_type) 0;                                                       \
    }                                                                          \
                                                                               \
    struct ArrowArray *col_array = pr->current_batch.children[col_index];      \
    struct ArrowSchema *col_schema = pr->schema.children[col_index];           \
                                                                               \
    if (strcmp(col_schema->format, format_char) != 0) {                        \
      fprintf(stderr,                                                          \
              "Column %d is not " #type_name " (format: %s)\n",                \
              col_index,                                                       \
              col_schema->format);                                             \
      return (c_type) 0;                                                       \
    }                                                                          \
                                                                               \
    int64_t row_idx = pr->next_row_in_batch - 1;                               \
    const uint8_t *validity = (const uint8_t *) col_array->buffers[0];         \
    if (is_value_null(validity, row_idx)) {                                    \
      return (c_type) 0;                                                       \
    }                                                                          \
                                                                               \
    const c_type *values = (const c_type *) col_array->buffers[1];             \
    return values[row_idx];                                                    \
  }

// Define all fixed-size type getters
DEFINE_FIXED_TYPE_GETTER(int32, int32_t, "i")
DEFINE_FIXED_TYPE_GETTER(int64, int64_t, "l")
DEFINE_FIXED_TYPE_GETTER(uint32, uint32_t, "I")
DEFINE_FIXED_TYPE_GETTER(uint64, uint64_t, "L")
DEFINE_FIXED_TYPE_GETTER(double, double, "g")
DEFINE_FIXED_TYPE_GETTER(float, float, "f")
DEFINE_FIXED_TYPE_GETTER(bool, bool, "b")
DEFINE_FIXED_TYPE_GETTER(int8, int8_t, "c")
DEFINE_FIXED_TYPE_GETTER(uint8, uint8_t, "C")
DEFINE_FIXED_TYPE_GETTER(int16, int16_t, "s")
DEFINE_FIXED_TYPE_GETTER(uint16, uint16_t, "S")

// =============================================================================
// Zero-Copy Data Access Functions for Variable-Length Types
// =============================================================================

// Get string value with zero-copy (returns pointer directly into Arrow buffer)
static bool packet_stream_get_string_zerocopy(PacketStream *stream,
                                              int col_index,
                                              const char **data,
                                              size_t *length) {
  ParquetReader *pr = get_current_reader(stream);
  if (!pr || !validate_column_index(pr, col_index)) {
    *data = NULL;
    *length = 0;
    return false;
  }

  struct ArrowArray *col_array = pr->current_batch.children[col_index];
  struct ArrowSchema *col_schema = pr->schema.children[col_index];

  // Check format is string (support both "u" and "U")
  if (strcmp(col_schema->format, "u") != 0 &&
      strcmp(col_schema->format, "U") != 0) {
    fprintf(stderr,
            "Column %d is not a string (format: %s)\n",
            col_index,
            col_schema->format);
    *data = NULL;
    *length = 0;
    return false;
  }

  int64_t row_idx = pr->next_row_in_batch - 1;
  const uint8_t *validity = (const uint8_t *) col_array->buffers[0];
  if (is_value_null(validity, row_idx)) {
    *data = NULL;
    *length = 0;
    return true; // Successfully retrieved null value
  }

  // Get the string directly from Arrow buffer (zero-copy)
  const int32_t *offsets = (const int32_t *) col_array->buffers[1];
  const uint8_t *buffer_data = (const uint8_t *) col_array->buffers[2];

  int32_t offset = offsets[row_idx];
  int32_t string_length = offsets[row_idx + 1] - offset;

  // Return pointer directly into Arrow buffer - NO MEMORY ALLOCATION
  *data = (const char *) (buffer_data + offset);
  *length = (size_t) string_length;

  return true;
}

// Get binary value with zero-copy (returns pointer directly into Arrow buffer)
static bool packet_stream_get_binary_zerocopy(PacketStream *stream,
                                              int col_index,
                                              const uint8_t **data,
                                              size_t *length) {
  ParquetReader *pr = get_current_reader(stream);
  if (!pr || !validate_column_index(pr, col_index)) {
    *data = NULL;
    *length = 0;
    return false;
  }

  struct ArrowArray *col_array = pr->current_batch.children[col_index];
  struct ArrowSchema *col_schema = pr->schema.children[col_index];

  // Check format is binary (support both "z" and "Z")
  if (strcmp(col_schema->format, "z") != 0 &&
      strcmp(col_schema->format, "Z") != 0) {
    fprintf(stderr,
            "Column %d is not binary (format: %s)\n",
            col_index,
            col_schema->format);
    *data = NULL;
    *length = 0;
    return false;
  }

  int64_t row_idx = pr->next_row_in_batch - 1;
  const uint8_t *validity = (const uint8_t *) col_array->buffers[0];
  if (is_value_null(validity, row_idx)) {
    *data = NULL;
    *length = 0;
    return true; // Successfully retrieved null value
  }

  // Get the binary data directly from Arrow buffer (zero-copy)
  const int32_t *offsets = (const int32_t *) col_array->buffers[1];
  const uint8_t *buffer_data = (const uint8_t *) col_array->buffers[2];

  int32_t offset = offsets[row_idx];
  int32_t binary_length = offsets[row_idx + 1] - offset;

  // Return pointer directly into Arrow buffer - NO MEMORY ALLOCATION
  *data = buffer_data + offset;
  *length = (size_t) binary_length;

  return true;
}

// =============================================================================
// Min-Heap Operations for Multi-File Merger
// =============================================================================

static inline void swap_entries(HeapEntry *a, HeapEntry *b) {
  HeapEntry temp = *a;
  *a = *b;
  *b = temp;
}

// Restore heap property (smaller key at root)
static void heapify_down(HeapEntry *heap, uint32_t heap_size, uint32_t index) {
  uint32_t smallest = index;
  uint32_t left = 2 * index + 1;
  uint32_t right = 2 * index + 2;

  if (left < heap_size && heap[left].key < heap[smallest].key) {
    smallest = left;
  }

  if (right < heap_size && heap[right].key < heap[smallest].key) {
    smallest = right;
  }

  if (smallest != index) {
    swap_entries(&heap[index], &heap[smallest]);
    heapify_down(heap, heap_size, smallest);
  }
}

// Build heap from unordered array
static void build_heap(HeapEntry *heap, uint32_t heap_size) {
  if (heap_size <= 1)
    return;

  for (int i = (heap_size - 2) / 2; i >= 0; i--) {
    heapify_down(heap, heap_size, i);
  }
}

// Get the key value for a reader
static int64_t get_reader_key(ParquetReader *pr,
                              int col_index,
                              bool is_int32_key) {
  if (is_int32_key) {
    const int32_t *values =
        (const int32_t *) pr->current_batch.children[col_index]->buffers[1];
    return (int64_t) values[pr->next_row_in_batch - 1];
  } else {
    const int64_t *values =
        (const int64_t *) pr->current_batch.children[col_index]->buffers[1];
    return values[pr->next_row_in_batch - 1];
  }
}

// =============================================================================
// Multi-File Merger Implementation
// =============================================================================

// Initialize a merger of multiple Parquet files
static PacketStream *parquet_merger_init_stream(const char **file_paths,
                                                uint32_t num_files,
                                                int key_column_index) {
  if (num_files == 0 || !file_paths) {
    fprintf(stderr, "No files provided for merger\n");
    return NULL;
  }

  // Allocate the merger
  ParquetMerger *pm = (ParquetMerger *) malloc(sizeof(ParquetMerger));
  if (!pm) {
    perror("Failed to allocate ParquetMerger");
    return NULL;
  }

  // Initialize base fields
  pm->K = num_files;
  pm->key_column_index = key_column_index;
  pm->heap_size = 0;
  pm->total_rows_processed = 0;
  pm->current_reader = NULL;
  pm->need_advance = false;

  // Allocate readers array
  pm->readers = (ParquetReader **) malloc(sizeof(ParquetReader *) * num_files);
  if (!pm->readers) {
    perror("Failed to allocate readers array");
    free(pm);
    return NULL;
  }

  // Allocate heap
  pm->heap = (HeapEntry *) malloc(sizeof(HeapEntry) * num_files);
  if (!pm->heap) {
    perror("Failed to allocate heap");
    free(pm->readers);
    free(pm);
    return NULL;
  }

  // Initialize all readers
  uint32_t active_readers = 0;
  bool key_type_determined = false;

  for (uint32_t i = 0; i < num_files; i++) {
    // Initialize each reader
    PacketStream *stream = parquet_reader_init_stream(file_paths[i]);
    if (!stream) {
      fprintf(stderr,
              "Failed to initialize reader for file %s\n",
              file_paths[i]);
      // Skip this file and continue
      pm->readers[i] = NULL;
      continue;
    }

    ParquetReader *pr = stream->impl.parquet_reader;
    pm->readers[i] = pr;

    // Validate key column exists and get its type
    if (!validate_column_index(pr, key_column_index)) {
      fprintf(stderr,
              "Key column index %d is invalid in file %s\n",
              key_column_index,
              file_paths[i]);
      packet_stream_release(stream);
      pm->readers[i] = NULL;
      continue;
    }

    const char *key_format = pr->schema.children[key_column_index]->format;

    // Check key type is compatible (either int32 or int64)
    if (strcmp(key_format, "i") != 0 && strcmp(key_format, "l") != 0) {
      fprintf(stderr,
              "Key column must be int32 or int64, but got %s in file %s\n",
              key_format,
              file_paths[i]);
      packet_stream_release(stream);
      pm->readers[i] = NULL;
      continue;
    }

    // Determine key type if this is the first valid reader
    if (!key_type_determined) {
      pm->is_int32_key = (strcmp(key_format, "i") == 0);
      key_type_determined = true;
    } else {
      // Ensure consistent key type across files
      bool is_current_int32 = (strcmp(key_format, "i") == 0);
      if (is_current_int32 != pm->is_int32_key) {
        fprintf(stderr,
                "Inconsistent key types: expected %s but got %s in file %s\n",
                pm->is_int32_key ? "int32" : "int64",
                is_current_int32 ? "int32" : "int64",
                file_paths[i]);
        packet_stream_release(stream);
        pm->readers[i] = NULL;
        continue;
      }
    }

    // Check if reader has data - it should be positioned at the first batch
    // with next_row_in_batch = 0
    if (!pr->stream_ended && pr->current_batch.release != NULL) {
      // The reader is positioned at the first batch with next_row_in_batch = 0,
      // just like single reader. We need to advance it to position at first row
      // and read the key for proper heap ordering.
      if (parquet_reader_next(pr)) {
        HeapEntry entry;
        entry.reader_index = i;
        entry.key = get_reader_key(pr, key_column_index, pm->is_int32_key);

        pm->heap[pm->heap_size++] = entry;
        active_readers++;
      }
    }

    // We've taken ownership of the ParquetReader from the stream
    // Free only the stream wrapper, not the reader itself
    free(stream);
  }

  // Check if we have any active readers
  if (active_readers == 0) {
    fprintf(stderr, "No valid readers were initialized\n");
    for (uint32_t i = 0; i < num_files; i++) {
      if (pm->readers[i]) {
        parquet_reader_cleanup(pm->readers[i]);
        free(pm->readers[i]);
      }
    }
    free(pm->readers);
    free(pm->heap);
    free(pm);
    return NULL;
  }

  // Build the initial heap
  build_heap(pm->heap, pm->heap_size);

  // Create the packet stream wrapper
  PacketStream *stream = (PacketStream *) malloc(sizeof(PacketStream));
  if (!stream) {
    perror("Failed to allocate PacketStream for merger");
    parquet_merger_cleanup(pm);
    free(pm);
    return NULL;
  }

  stream->kind = PS_PARQUET_MERGER;
  stream->impl.parquet_merger = pm;

  return stream;
}

// Advance to next row (reading from appropriate reader based on min-heap)
static bool parquet_merger_next(ParquetMerger *pm) {
  // If heap is empty, no more rows
  if (pm->heap_size == 0) {
    return false;
  }

  // If we need to advance from the previous call, do it now
  if (pm->need_advance && pm->current_reader) {
    bool has_next = parquet_reader_next(pm->current_reader);

    if (has_next) {
      // Update the key value in the heap for the current reader
      // Find the heap entry for this reader
      for (uint32_t i = 0; i < pm->heap_size; i++) {
        if (pm->readers[pm->heap[i].reader_index] == pm->current_reader) {
          pm->heap[i].key = get_reader_key(pm->current_reader,
                                           pm->key_column_index,
                                           pm->is_int32_key);
          break;
        }
      }
      // Rebuild heap to maintain order
      build_heap(pm->heap, pm->heap_size);
    } else {
      // Reader is exhausted, remove from heap
      for (uint32_t i = 0; i < pm->heap_size; i++) {
        if (pm->readers[pm->heap[i].reader_index] == pm->current_reader) {
          pm->heap[i] = pm->heap[pm->heap_size - 1];
          pm->heap_size--;
          if (pm->heap_size > 0) {
            build_heap(pm->heap, pm->heap_size);
          }
          break;
        }
      }
    }
    pm->need_advance = false;
  }

  // Check again if heap is empty after potential removal
  if (pm->heap_size == 0) {
    return false;
  }

  // Get the reader with the minimum key (at root of heap)
  uint32_t min_reader_idx = pm->heap[0].reader_index;
  ParquetReader *min_reader = pm->readers[min_reader_idx];

  // Set the current active reader - this reader is positioned at the current
  // row
  pm->current_reader = min_reader;

  // Increment total rows
  pm->total_rows_processed++;

  // Mark that we need to advance this reader on the next call
  pm->need_advance = true;

  return true;
}

// Cleanup merger and all readers
static void parquet_merger_cleanup(ParquetMerger *pm) {
  if (pm) {
    // Cleanup all readers
    for (uint32_t i = 0; i < pm->K; i++) {
      if (pm->readers[i]) {
        parquet_reader_cleanup(pm->readers[i]);
        free(pm->readers[i]);
      }
    }

    // Free arrays
    free(pm->readers);
    free(pm->heap);
  }
}

#endif // PARQUET_READER_STREAM_H