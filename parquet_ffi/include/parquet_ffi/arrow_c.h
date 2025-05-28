#pragma once

#define ARROW_FLAG_DICTIONARY_ORDERED 1
#define ARROW_FLAG_NULLABLE 2
#define ARROW_FLAG_MAP_KEYS_SORTED 4

#include <stdint.h> // for uint8_t, int64_t, etc.

#ifdef __cplusplus
extern "C" {
#endif

// Arrow C Data Interface for Schema
struct ArrowSchema {
  // Array type description
  const char *format;
  const char *name;
  const char *metadata;
  int64_t flags;
  int64_t n_children;
  struct ArrowSchema **children;
  struct ArrowSchema *dictionary;

  // Release callback
  void (*release)(struct ArrowSchema *);
  // Private data for release callback
  void *private_data;
};

// Arrow C Data Interface for Array
struct ArrowArray {
  // Array data description
  int64_t length;
  int64_t null_count;
  int64_t offset;
  int64_t n_buffers;
  int64_t n_children;
  const void **buffers;
  struct ArrowArray **children;
  struct ArrowArray *dictionary;

  // Release callback
  void (*release)(struct ArrowArray *);
  // Private data for release callback
  void *private_data;
};

// Arrow C Stream Interface
struct ArrowArrayStream {
  // Callback to get the stream schema
  int (*get_schema)(struct ArrowArrayStream *, struct ArrowSchema *);
  // Callback to get the next array
  int (*get_next)(struct ArrowArrayStream *, struct ArrowArray *);
  // Callback to get the last error
  const char *(*get_last_error)(struct ArrowArrayStream *);
  // Release callback
  void (*release)(struct ArrowArrayStream *);
  // Private data
  void *private_data;
};

#ifdef __cplusplus
}
#endif
