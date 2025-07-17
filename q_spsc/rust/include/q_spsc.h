#ifndef Q_SPSC_H
#define Q_SPSC_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QSPSC_HUGE_PAGES_NEVER = 0,
    QSPSC_HUGE_PAGES_ALWAYS = 1,
    QSPSC_HUGE_PAGES_TRY = 2
} QSPSCHugePagesPolicy;

typedef void* QSPSCQueue;

/**
 * Create a new bounded single-producer single-consumer queue.
 *
 * @param capacity The desired capacity (will be rounded up to next power of 2)
 * @param huge_pages_policy Policy for using huge pages (Linux only)
 * @param reader_store_percent Percentage of capacity for batching reader position updates
 * @return Handle to the queue, or NULL on failure
 */
QSPSCQueue qspsc_new(size_t capacity, QSPSCHugePagesPolicy huge_pages_policy, size_t reader_store_percent);

/**
 * Destroy a queue and free all associated memory.
 *
 * @param queue Handle to the queue
 */
void qspsc_destroy(QSPSCQueue queue);

/**
 * Prepare to write n bytes to the queue.
 *
 * @param queue Handle to the queue
 * @param n Number of bytes to write
 * @return Pointer to write location, or NULL if insufficient space
 */
uint8_t* qspsc_prepare_write(QSPSCQueue queue, size_t n);

/**
 * Finish writing n bytes to the queue (update internal position).
 *
 * @param queue Handle to the queue
 * @param n Number of bytes written
 */
void qspsc_finish_write(QSPSCQueue queue, size_t n);

/**
 * Commit all written data to make it visible to the reader.
 *
 * @param queue Handle to the queue
 */
void qspsc_commit_write(QSPSCQueue queue);

/**
 * Finish writing n bytes and commit in one operation.
 *
 * @param queue Handle to the queue
 * @param n Number of bytes written
 */
void qspsc_finish_and_commit_write(QSPSCQueue queue, size_t n);

/**
 * Prepare to read from the queue.
 *
 * @param queue Handle to the queue
 * @return Pointer to read location, or NULL if queue is empty
 */
uint8_t* qspsc_prepare_read(QSPSCQueue queue);

/**
 * Finish reading n bytes from the queue (update internal position).
 *
 * @param queue Handle to the queue
 * @param n Number of bytes read
 */
void qspsc_finish_read(QSPSCQueue queue, size_t n);

/**
 * Commit the read operation (may batch position updates).
 *
 * @param queue Handle to the queue
 */
void qspsc_commit_read(QSPSCQueue queue);

/**
 * Check if the queue is empty.
 *
 * @param queue Handle to the queue
 * @return true if empty, false otherwise
 */
bool qspsc_empty(QSPSCQueue queue);

/**
 * Get the capacity of the queue.
 *
 * @param queue Handle to the queue
 * @return Capacity in bytes
 */
size_t qspsc_capacity(QSPSCQueue queue);

/**
 * Get the huge pages policy of the queue.
 *
 * @param queue Handle to the queue
 * @return The huge pages policy
 */
QSPSCHugePagesPolicy qspsc_huge_pages_policy(QSPSCQueue queue);

#ifdef __cplusplus
}
#endif

#endif /* Q_SPSC_H */