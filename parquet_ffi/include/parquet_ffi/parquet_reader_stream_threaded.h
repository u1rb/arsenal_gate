#ifndef PARQUET_READER_STREAM_THREADED_H
#define PARQUET_READER_STREAM_THREADED_H

// =============================================================================
// Parquet Reader Stream - Multi-threaded Batch Prefetching Extension
// =============================================================================
//
// This header extends the base parquet_reader_stream.h with multi-threaded
// batch prefetching capabilities for improved performance.
//
// Usage:
//   #include "parquet_reader_stream_threaded.h"
//
//   // Single file with threading for better performance
//   PacketStream *stream = parquet_reader_init_stream_threaded("file.parquet");
//
//   // Use the same API as the base library
//   while (packet_stream_next(stream)) {
//     // ... process data ...
//   }
//
//   packet_stream_release(stream);
//
// =============================================================================

#include "parquet_reader_stream.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Threading Support Structures
// =============================================================================

#define MAX_PREFETCH_BATCHES 3 // Number of batches to prefetch ahead

// Prefetch queue entry
typedef struct {
  struct ArrowArray batch;
  bool valid;
  bool is_end_of_stream;
} PrefetchEntry;

// Threading state for batch prefetching with queue
typedef struct {
  pthread_t thread;               // Background thread handle
  pthread_mutex_t mutex;          // Mutex for synchronization
  pthread_cond_t space_available; // Condition variable for queue space
  pthread_cond_t data_available;  // Condition variable for queue data
  bool thread_active;             // Whether the thread is running
  bool shutdown_requested;        // Signal to shutdown the thread
  bool fetch_error;               // Whether an error occurred during fetch
  int error_code;                 // Error code from failed fetch

  // Circular buffer for prefetched batches
  PrefetchEntry queue[MAX_PREFETCH_BATCHES];
  int queue_head; // Next position to write (producer)
  int queue_tail; // Next position to read (consumer)
  int queue_size; // Current number of items in queue

  struct ArrowArrayStream *arrow_stream; // Reference to the arrow stream
} ThreadState;

// Extended ParquetReader with threading support
typedef struct {
  ParquetReader base;        // Base reader functionality
  bool use_threading;        // Whether to use background threading
  ThreadState *thread_state; // Threading state (NULL if not using threading)
} ThreadedParquetReader;

// =============================================================================
// Threading Function Declarations
// =============================================================================

// Background thread function for batch prefetching
static void *background_batch_thread(void *arg);

// Initialize threading state
static ThreadState *init_thread_state(struct ArrowArrayStream *arrow_stream);

// Cleanup threading state
static void cleanup_thread_state(ThreadState *ts);

// Request next batch from background thread
static bool request_next_batch_threaded(ThreadedParquetReader *tpr);

// Get next batch using threading or fallback to sync
static bool get_next_batch_threaded(ThreadedParquetReader *tpr,
                                    struct ArrowArray *batch);

// Check if a ParquetReader is actually a ThreadedParquetReader
static bool is_threaded_reader(ParquetReader *pr);

// =============================================================================
// Public API Functions
// =============================================================================

// Initialize a single Parquet file reader with threading (default batch size)
static PacketStream *parquet_reader_init_stream_threaded(const char *file_path);

// Initialize a single Parquet file reader with threading and custom batch size
static PacketStream *
parquet_reader_init_stream_threaded_with_batch_size(const char *file_path,
                                                    int batch_size);

// Enhanced next function for threaded reader
static bool threaded_parquet_reader_next(ThreadedParquetReader *tpr);

// Enhanced cleanup function for threaded reader
static void threaded_parquet_reader_cleanup(ThreadedParquetReader *tpr);

// Override for parquet_reader_next to handle threaded readers
static bool parquet_reader_next_with_threading_support(ParquetReader *pr);

// Override for parquet_reader_cleanup to handle threaded readers
static void parquet_reader_cleanup_with_threading_support(ParquetReader *pr);

// =============================================================================
// Threading Detection and Dispatch
// =============================================================================

// Check if a ParquetReader is actually a ThreadedParquetReader
// We do this by checking if the memory layout suggests extended structure
static bool is_threaded_reader(ParquetReader *pr) {
  // Cast to ThreadedParquetReader and check if use_threading flag is set
  // This is safe because ThreadedParquetReader starts with ParquetReader
  ThreadedParquetReader *tpr = (ThreadedParquetReader *)pr;

  // Simple heuristic: if the memory after the base struct looks like our
  // threading fields In practice, we could use a magic number or other
  // identifier
  return (tpr->use_threading == true && tpr->thread_state != NULL);
}

// Override for parquet_reader_next to handle threaded readers
static bool parquet_reader_next_with_threading_support(ParquetReader *pr) {
  if (is_threaded_reader(pr)) {
    return threaded_parquet_reader_next((ThreadedParquetReader *)pr);
  } else {
    // Use original implementation for non-threaded readers
    // If stream already ended, no more rows
    if (pr->stream_ended) {
      return false;
    }

    // Check if we've reached the end of the current batch
    if (pr->next_row_in_batch >= pr->current_batch.length) {
      // Release current batch
      pr->current_batch.release(&pr->current_batch);

      // Get next batch
      int ret =
          pr->arrow_stream.get_next(&pr->arrow_stream, &pr->current_batch);
      if (ret != 0) {
        fprintf(stderr, "Failed to get next batch: error code %d\n", ret);
        if (pr->arrow_stream.get_last_error) {
          fprintf(stderr, "Error: %s\n",
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
}

// Override for parquet_reader_cleanup to handle threaded readers
static void parquet_reader_cleanup_with_threading_support(ParquetReader *pr) {
  if (is_threaded_reader(pr)) {
    threaded_parquet_reader_cleanup((ThreadedParquetReader *)pr);
  } else {
    // Use original cleanup for non-threaded readers
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
}

// Macro to override the packet_stream_next behavior for threading support
#undef packet_stream_next
#define packet_stream_next(stream)                                             \
  ((stream)->kind == PS_PARQUET_SINGLE                                         \
       ? parquet_reader_next_with_threading_support(                           \
             (stream)->impl.parquet_reader)                                    \
       : parquet_merger_next((stream)->impl.parquet_merger))

// Helper function to release stream resources with threading support
static inline void packet_stream_release_with_threading(PacketStream *stream) {
  if (stream) {
    switch (stream->kind) {
    case PS_PARQUET_SINGLE:
      parquet_reader_cleanup_with_threading_support(
          stream->impl.parquet_reader);
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

// Override packet_stream_release to use threading-aware cleanup
#define packet_stream_release(stream)                                          \
  packet_stream_release_with_threading(stream)

// =============================================================================
// Implementation
// =============================================================================

// Background thread function for batch prefetching
static void *background_batch_thread(void *arg) {
  ThreadState *ts = (ThreadState *)arg;

  while (true) {
    pthread_mutex_lock(&ts->mutex);

    // Wait for space in queue or shutdown signal
    while (ts->queue_size >= MAX_PREFETCH_BATCHES && !ts->shutdown_requested) {
      pthread_cond_wait(&ts->space_available, &ts->mutex);
    }

    if (ts->shutdown_requested) {
      pthread_mutex_unlock(&ts->mutex);
      break;
    }

    // Unlock mutex during I/O operation (key optimization!)
    pthread_mutex_unlock(&ts->mutex);

    // Perform I/O and decompression without holding the lock
    struct ArrowArray new_batch;
    memset(&new_batch, 0, sizeof(struct ArrowArray));

    int ret = ts->arrow_stream->get_next(ts->arrow_stream, &new_batch);

    // Lock again to update queue
    pthread_mutex_lock(&ts->mutex);

    if (ts->shutdown_requested) {
      // If shutdown was requested while we were doing I/O, cleanup and exit
      if (ret == 0 && new_batch.release) {
        new_batch.release(&new_batch);
      }
      pthread_mutex_unlock(&ts->mutex);
      break;
    }

    // Add batch to queue
    PrefetchEntry *entry = &ts->queue[ts->queue_head];

    if (ret == 0) {
      // Success - check if this is end of stream
      if (new_batch.release == NULL) {
        // End of stream
        entry->valid = false;
        entry->is_end_of_stream = true;
        memset(&entry->batch, 0, sizeof(struct ArrowArray));
      } else {
        // Valid batch
        entry->batch = new_batch;
        entry->valid = true;
        entry->is_end_of_stream = false;
      }
    } else {
      // Error occurred
      ts->fetch_error = true;
      ts->error_code = ret;
      entry->valid = false;
      entry->is_end_of_stream = true;
      memset(&entry->batch, 0, sizeof(struct ArrowArray));
    }

    ts->queue_head = (ts->queue_head + 1) % MAX_PREFETCH_BATCHES;
    ts->queue_size++;

    // Signal that data is available
    pthread_cond_signal(&ts->data_available);

    pthread_mutex_unlock(&ts->mutex);

    // If we hit end of stream or error, stop prefetching
    if (entry->is_end_of_stream) {
      break;
    }
  }

  return NULL;
}

// Initialize threading state
static ThreadState *init_thread_state(struct ArrowArrayStream *arrow_stream) {
  ThreadState *ts = (ThreadState *)malloc(sizeof(ThreadState));
  if (!ts) {
    perror("Failed to allocate ThreadState");
    return NULL;
  }

  memset(ts, 0, sizeof(ThreadState));
  ts->arrow_stream = arrow_stream;

  // Initialize synchronization primitives
  if (pthread_mutex_init(&ts->mutex, NULL) != 0) {
    perror("Failed to initialize mutex");
    free(ts);
    return NULL;
  }

  if (pthread_cond_init(&ts->space_available, NULL) != 0) {
    perror("Failed to initialize space available condition variable");
    pthread_mutex_destroy(&ts->mutex);
    free(ts);
    return NULL;
  }

  if (pthread_cond_init(&ts->data_available, NULL) != 0) {
    perror("Failed to initialize data available condition variable");
    pthread_cond_destroy(&ts->space_available);
    pthread_mutex_destroy(&ts->mutex);
    free(ts);
    return NULL;
  }

  // Start background thread
  if (pthread_create(&ts->thread, NULL, background_batch_thread, ts) != 0) {
    perror("Failed to create background thread");
    pthread_cond_destroy(&ts->data_available);
    pthread_cond_destroy(&ts->space_available);
    pthread_mutex_destroy(&ts->mutex);
    free(ts);
    return NULL;
  }

  ts->thread_active = true;
  return ts;
}

// Cleanup threading state
static void cleanup_thread_state(ThreadState *ts) {
  if (!ts)
    return;

  if (ts->thread_active) {
    // Signal shutdown
    pthread_mutex_lock(&ts->mutex);
    ts->shutdown_requested = true;
    pthread_cond_signal(&ts->data_available);
    pthread_mutex_unlock(&ts->mutex);

    // Wait for thread to finish
    pthread_join(ts->thread, NULL);
    ts->thread_active = false;
  }

  // Release prefetched batches if any
  for (int i = 0; i < MAX_PREFETCH_BATCHES; i++) {
    if (ts->queue[i].valid && ts->queue[i].batch.release) {
      ts->queue[i].batch.release(&ts->queue[i].batch);
    }
  }

  // Cleanup synchronization primitives
  pthread_cond_destroy(&ts->data_available);
  pthread_cond_destroy(&ts->space_available);
  pthread_mutex_destroy(&ts->mutex);

  free(ts);
}

// Request next batch from background thread (now just checks if data is
// available)
static bool request_next_batch_threaded(ThreadedParquetReader *tpr) {
  ThreadState *ts = tpr->thread_state;
  if (!ts || !ts->thread_active) {
    return false;
  }

  // With the new queue system, we don't need to request - just check if data is
  // available
  pthread_mutex_lock(&ts->mutex);
  bool has_data = (ts->queue_size > 0);
  pthread_mutex_unlock(&ts->mutex);

  return has_data;
}

// Get next batch using threading or fallback to sync
static bool get_next_batch_threaded(ThreadedParquetReader *tpr,
                                    struct ArrowArray *batch) {
  if (!tpr->use_threading || !tpr->thread_state) {
    // Fallback to synchronous fetch
    return tpr->base.arrow_stream.get_next(&tpr->base.arrow_stream, batch) == 0;
  }

  ThreadState *ts = tpr->thread_state;

  pthread_mutex_lock(&ts->mutex);

  // Wait for data to be available
  while (ts->queue_size == 0 && !ts->shutdown_requested && !ts->fetch_error) {
    pthread_cond_wait(&ts->data_available, &ts->mutex);
  }

  if (ts->shutdown_requested) {
    pthread_mutex_unlock(&ts->mutex);
    return false;
  }

  if (ts->queue_size == 0) {
    // No data available and we're not shutting down - must be an error or end
    // of stream
    pthread_mutex_unlock(&ts->mutex);
    return false;
  }

  // Get next batch from queue
  PrefetchEntry *entry = &ts->queue[ts->queue_tail];

  if (entry->is_end_of_stream) {
    // End of stream or error
    if (ts->fetch_error) {
      fprintf(stderr, "Failed to get next batch: error code %d\n",
              ts->error_code);
      if (tpr->base.arrow_stream.get_last_error) {
        fprintf(stderr, "Error: %s\n",
                tpr->base.arrow_stream.get_last_error(&tpr->base.arrow_stream));
      }
    }
    ts->queue_tail = (ts->queue_tail + 1) % MAX_PREFETCH_BATCHES;
    ts->queue_size--;
    pthread_cond_signal(&ts->space_available);
    pthread_mutex_unlock(&ts->mutex);
    return false;
  }

  // Move the batch to output
  *batch = entry->batch;
  memset(&entry->batch, 0, sizeof(struct ArrowArray));
  entry->valid = false;

  ts->queue_tail = (ts->queue_tail + 1) % MAX_PREFETCH_BATCHES;
  ts->queue_size--;

  // Signal that space is available for more prefetching
  pthread_cond_signal(&ts->space_available);

  pthread_mutex_unlock(&ts->mutex);

  return true;
}

// Enhanced next function for threaded reader
static bool threaded_parquet_reader_next(ThreadedParquetReader *tpr) {
  // If stream already ended, no more rows
  if (tpr->base.stream_ended) {
    return false;
  }

  // Check if we need to get a new batch (either first call or reached end of
  // current batch)
  if (tpr->base.current_batch.release == NULL ||
      tpr->base.next_row_in_batch >= tpr->base.current_batch.length) {

    // Release current batch if it exists
    if (tpr->base.current_batch.release) {
      tpr->base.current_batch.release(&tpr->base.current_batch);
    }

    // Get next batch (threaded or sync)
    if (!get_next_batch_threaded(tpr, &tpr->base.current_batch)) {
      tpr->base.stream_ended = true;
      return false;
    }

    // Check if we've reached the end of the stream
    if (tpr->base.current_batch.release == NULL) {
      tpr->base.stream_ended = true;
      return false;
    }

    // Reset row index for new batch
    tpr->base.next_row_in_batch = 0;
    tpr->base.batch_number++;

    // No need to explicitly request next batch - background thread runs
    // continuously
  }

  // Advance to next row
  tpr->base.next_row_in_batch++;
  tpr->base.total_rows_processed++;

  return true;
}

// Enhanced cleanup function for threaded reader
static void threaded_parquet_reader_cleanup(ThreadedParquetReader *tpr) {
  if (tpr) {
    // Cleanup threading state first
    if (tpr->thread_state) {
      cleanup_thread_state(tpr->thread_state);
      tpr->thread_state = NULL;
    }

    // Cleanup base reader
    if (!tpr->base.stream_ended && tpr->base.current_batch.release) {
      tpr->base.current_batch.release(&tpr->base.current_batch);
    }

    if (tpr->base.schema.release) {
      tpr->base.schema.release(&tpr->base.schema);
    }

    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
  }
}

// Initialize a single Parquet file reader with threading (default batch size)
static PacketStream *
parquet_reader_init_stream_threaded(const char *file_path) {
  ThreadedParquetReader *tpr =
      (ThreadedParquetReader *)malloc(sizeof(ThreadedParquetReader));
  if (!tpr) {
    perror("Failed to allocate ThreadedParquetReader");
    return NULL;
  }

  // Initialize all fields
  memset(tpr, 0, sizeof(ThreadedParquetReader));
  tpr->use_threading = true;

  // Export the parquet file to the stream
  int ret = export_parquet_file_to_stream(file_path, &tpr->base.arrow_stream);
  if (ret != 0) {
    fprintf(stderr, "Failed to export parquet file to stream: error code %d\n",
            ret);
    free(tpr);
    return NULL;
  }

  // Get the schema
  ret = tpr->base.arrow_stream.get_schema(&tpr->base.arrow_stream,
                                          &tpr->base.schema);
  if (ret != 0) {
    fprintf(stderr, "Failed to get schema: error code %d\n", ret);
    if (tpr->base.arrow_stream.get_last_error) {
      fprintf(stderr, "Error: %s\n",
              tpr->base.arrow_stream.get_last_error(&tpr->base.arrow_stream));
    }
    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
    free(tpr);
    return NULL;
  }

  // Initialize threading state
  tpr->thread_state = init_thread_state(&tpr->base.arrow_stream);
  if (!tpr->thread_state) {
    fprintf(stderr, "Failed to initialize threading state\n");
    tpr->base.schema.release(&tpr->base.schema);
    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
    free(tpr);
    return NULL;
  }

  // Let the background thread handle all batch fetching, including the first
  // batch Initialize with empty current batch - will be filled by first call to
  // next()
  memset(&tpr->base.current_batch, 0, sizeof(struct ArrowArray));
  tpr->base.batch_number = 0;
  tpr->base.next_row_in_batch =
      0; // This will trigger batch fetch on first next() call
  tpr->base.total_rows_processed = 0;

  // Create the packet stream wrapper
  PacketStream *stream = (PacketStream *)malloc(sizeof(PacketStream));
  if (!stream) {
    perror("Failed to allocate PacketStream");
    if (!tpr->base.stream_ended && tpr->base.current_batch.release) {
      tpr->base.current_batch.release(&tpr->base.current_batch);
    }
    cleanup_thread_state(tpr->thread_state);
    tpr->base.schema.release(&tpr->base.schema);
    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
    free(tpr);
    return NULL;
  }

  stream->kind = PS_PARQUET_SINGLE;
  stream->impl.parquet_reader = (ParquetReader *)tpr; // Cast to base type

  return stream;
}

// Initialize a single Parquet file reader with threading and custom batch size
static PacketStream *
parquet_reader_init_stream_threaded_with_batch_size(const char *file_path,
                                                    int batch_size) {
  ThreadedParquetReader *tpr =
      (ThreadedParquetReader *)malloc(sizeof(ThreadedParquetReader));
  if (!tpr) {
    perror("Failed to allocate ThreadedParquetReader");
    return NULL;
  }

  // Initialize all fields
  memset(tpr, 0, sizeof(ThreadedParquetReader));
  tpr->use_threading = true;

  // Export the parquet file to the stream with custom batch size
  int ret = export_parquet_file_to_stream_with_batch_size(
      file_path, &tpr->base.arrow_stream, batch_size);
  if (ret != 0) {
    fprintf(stderr, "Failed to export parquet file to stream: error code %d\n",
            ret);
    free(tpr);
    return NULL;
  }

  // Get the schema
  ret = tpr->base.arrow_stream.get_schema(&tpr->base.arrow_stream,
                                          &tpr->base.schema);
  if (ret != 0) {
    fprintf(stderr, "Failed to get schema: error code %d\n", ret);
    if (tpr->base.arrow_stream.get_last_error) {
      fprintf(stderr, "Error: %s\n",
              tpr->base.arrow_stream.get_last_error(&tpr->base.arrow_stream));
    }
    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
    free(tpr);
    return NULL;
  }

  // Initialize threading state
  tpr->thread_state = init_thread_state(&tpr->base.arrow_stream);
  if (!tpr->thread_state) {
    fprintf(stderr, "Failed to initialize threading state\n");
    tpr->base.schema.release(&tpr->base.schema);
    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
    free(tpr);
    return NULL;
  }

  // Let the background thread handle all batch fetching, including the first
  // batch Initialize with empty current batch - will be filled by first call to
  // next()
  memset(&tpr->base.current_batch, 0, sizeof(struct ArrowArray));
  tpr->base.batch_number = 0;
  tpr->base.next_row_in_batch =
      0; // This will trigger batch fetch on first next() call
  tpr->base.total_rows_processed = 0;

  // Create the packet stream wrapper
  PacketStream *stream = (PacketStream *)malloc(sizeof(PacketStream));
  if (!stream) {
    perror("Failed to allocate PacketStream");
    if (!tpr->base.stream_ended && tpr->base.current_batch.release) {
      tpr->base.current_batch.release(&tpr->base.current_batch);
    }
    cleanup_thread_state(tpr->thread_state);
    tpr->base.schema.release(&tpr->base.schema);
    tpr->base.arrow_stream.release(&tpr->base.arrow_stream);
    free(tpr);
    return NULL;
  }

  stream->kind = PS_PARQUET_SINGLE;
  stream->impl.parquet_reader = (ParquetReader *)tpr; // Cast to base type

  return stream;
}

#ifdef __cplusplus
}
#endif

#endif // PARQUET_READER_STREAM_THREADED_H