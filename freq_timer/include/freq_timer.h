#ifndef FREQ_TIMER_H
#define FREQ_TIMER_H

/* Generated with cbindgen:0.26.0 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct freq_timer_handle {
  uint64_t start_cycles;
} freq_timer_handle;

typedef struct freq_timer_batch {
  uint64_t *timestamps;
  uintptr_t capacity;
  uintptr_t count;
} freq_timer_batch;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

uint64_t freq_timer_now_ns(void);

uint64_t freq_timer_now_cycles(void);

double freq_timer_cycles_per_ns(void);

/**
 * Start a timer by recording the current CPU cycle count.
 *
 * # Safety
 *
 * The caller must ensure that `timer` points to a valid `freq_timer_handle` structure.
 */
int freq_timer_start(struct freq_timer_handle *timer);

/**
 * Get elapsed time in nanoseconds since timer was started.
 *
 * # Safety
 *
 * The caller must ensure that `timer` points to a valid, initialized `freq_timer_handle`.
 */
uint64_t freq_timer_elapsed_ns(const struct freq_timer_handle *timer);

/**
 * Get elapsed CPU cycles since timer was started.
 *
 * # Safety
 *
 * The caller must ensure that `timer` points to a valid, initialized `freq_timer_handle`.
 */
uint64_t freq_timer_elapsed_cycles(const struct freq_timer_handle *timer);

int freq_timer_init(void);

int freq_timer_calibrate(void);

void freq_timer_cleanup(void);

/**
 * Initialize a batch timer with the specified capacity.
 *
 * # Safety
 *
 * The caller must ensure that `batch` points to a valid `freq_timer_batch` structure
 * that will remain valid for the lifetime of the batch.
 */
int freq_timer_batch_init(struct freq_timer_batch *batch, uintptr_t capacity);

/**
 * Capture a timestamp in the batch.
 *
 * # Safety
 *
 * The caller must ensure that `batch` points to a valid, initialized `freq_timer_batch`.
 */
int freq_timer_batch_capture(struct freq_timer_batch *batch);

/**
 * Clean up and free memory allocated for a batch timer.
 *
 * # Safety
 *
 * The caller must ensure that `batch` points to a valid `freq_timer_batch` that was
 * previously initialized with `freq_timer_batch_init`.
 */
void freq_timer_batch_cleanup(struct freq_timer_batch *batch);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif /* FREQ_TIMER_H */
