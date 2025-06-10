#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include "freq_timer.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void test_monotonicity_stress() {
    printf("Testing monotonicity under stress...\n");
    
    const int iterations = 100000;
    uint64_t* timestamps = malloc(iterations * sizeof(uint64_t));
    assert(timestamps != NULL);
    
    // Rapid fire timestamp collection
    for (int i = 0; i < iterations; i++) {
        timestamps[i] = freq_timer_now_ns();
    }
    
    // Check monotonicity
    int violations = 0;
    for (int i = 1; i < iterations; i++) {
        if (timestamps[i] < timestamps[i-1]) {
            violations++;
            if (violations <= 5) { // Only print first few violations
                printf("  Monotonicity violation at %d: %lu -> %lu (diff: %ld)\n", 
                       i, timestamps[i-1], timestamps[i], (int64_t)timestamps[i] - (int64_t)timestamps[i-1]);
            }
        }
    }
    
    printf("  Collected %d timestamps, violations: %d (%.3f%%)\n", 
           iterations, violations, (violations * 100.0) / iterations);
    
    assert(violations == 0);
    free(timestamps);
}

void test_accuracy_vs_clock_gettime() {
    printf("Testing accuracy vs clock_gettime...\n");
    
    const int iterations = 50;
    double total_error = 0.0;
    double max_error = 0.0;
    
    for (int i = 0; i < iterations; i++) {
        struct timespec ts_start, ts_end;
        
        // Get start times
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        uint64_t freq_start = freq_timer_now_ns();
        
        // Do some work
        usleep(5000); // 5ms
        
        // Get end times
        uint64_t freq_end = freq_timer_now_ns();
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        
        // Calculate durations
        uint64_t freq_duration = freq_end - freq_start;
        uint64_t clock_duration = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000UL + 
                                  (ts_end.tv_nsec - ts_start.tv_nsec);
        
        // Calculate relative error
        double error = fabs((double)freq_duration - (double)clock_duration) / (double)clock_duration;
        total_error += error;
        if (error > max_error) max_error = error;
        
        if (i < 5) { // Print first few measurements
            printf("  Sample %d: freq=%lu ns, clock=%lu ns, error=%.3f%%\n", 
                   i, freq_duration, clock_duration, error * 100.0);
        }
    }
    
    double avg_error = total_error / iterations;
    printf("  Average error: %.3f%%, Max error: %.3f%%\n", avg_error * 100.0, max_error * 100.0);
    
    // Should be within 2% on average
    assert(avg_error < 0.02);
    assert(max_error < 0.05);
}

void test_timer_handle_accuracy() {
    printf("Testing timer handle accuracy...\n");
    
    struct freq_timer_handle timer;
    assert(freq_timer_start(&timer) == 0);
    
    struct timespec ts_start;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    
    // Wait
    usleep(10000); // 10ms
    
    struct timespec ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    
    uint64_t freq_elapsed = freq_timer_elapsed_ns(&timer);
    uint64_t clock_elapsed = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000UL + 
                             (ts_end.tv_nsec - ts_start.tv_nsec);
    
    double error = fabs((double)freq_elapsed - (double)clock_elapsed) / (double)clock_elapsed;
    
    printf("  Timer handle: freq=%lu ns, clock=%lu ns, error=%.3f%%\n", 
           freq_elapsed, clock_elapsed, error * 100.0);
    
    assert(error < 0.02); // Within 2%
}

void test_batch_consistency() {
    printf("Testing batch timing consistency...\n");
    
    struct freq_timer_batch batch;
    const size_t capacity = 1000;
    
    assert(freq_timer_batch_init(&batch, capacity) == 0);
    
    // Capture timestamps with known delays
    for (size_t i = 0; i < capacity; i++) {
        assert(freq_timer_batch_capture(&batch) == 0);
        if (i % 100 == 0) {
            usleep(100); // 100us every 100 samples
        }
    }
    
    // Analyze the timestamps
    uint64_t min_diff = UINT64_MAX;
    uint64_t max_diff = 0;
    int zero_diffs = 0;
    
    for (size_t i = 1; i < batch.count; i++) {
        if (batch.timestamps[i] == batch.timestamps[i-1]) {
            zero_diffs++;
        } else {
            uint64_t diff = batch.timestamps[i] - batch.timestamps[i-1];
            if (diff < min_diff) min_diff = diff;
            if (diff > max_diff) max_diff = diff;
        }
    }
    
    printf("  Captured %zu timestamps\n", batch.count);
    printf("  Min difference: %lu ns, Max difference: %lu ns\n", min_diff, max_diff);
    printf("  Zero differences: %d/%zu (%.1f%%)\n", zero_diffs, batch.count-1, 
           (zero_diffs * 100.0) / (batch.count-1));
    
    // Should have reasonable resolution
    assert(min_diff < 10000); // Less than 10us minimum
    assert(zero_diffs < (int)(batch.count / 4)); // Less than 25% zero diffs
    
    freq_timer_batch_cleanup(&batch);
}

void test_resolution() {
    printf("Testing timer resolution...\n");
    
    const int samples = 10000;
    uint64_t min_diff = UINT64_MAX;
    int zero_count = 0;
    
    uint64_t prev = freq_timer_now_ns();
    for (int i = 0; i < samples; i++) {
        uint64_t curr = freq_timer_now_ns();
        if (curr == prev) {
            zero_count++;
        } else if (curr > prev) {
            uint64_t diff = curr - prev;
            if (diff < min_diff) {
                min_diff = diff;
            }
        }
        prev = curr;
    }
    
    printf("  Minimum resolution: %lu ns\n", min_diff == UINT64_MAX ? 0 : min_diff);
    printf("  Zero differences: %d/%d (%.1f%%)\n", zero_count, samples, 
           (zero_count * 100.0) / samples);
    
    // Should have sub-microsecond resolution
    if (min_diff != UINT64_MAX) {
        assert(min_diff < 1000);
    }
}

// Thread function for concurrent testing
void* thread_timing_test(void* arg) {
    int thread_id = *(int*)arg;
    const int iterations = 1000;
    uint64_t* timestamps = malloc(iterations * sizeof(uint64_t));
    
    for (int i = 0; i < iterations; i++) {
        timestamps[i] = freq_timer_now_ns();
        usleep(10); // Small delay
    }
    
    // Check monotonicity within thread
    for (int i = 1; i < iterations; i++) {
        if (timestamps[i] < timestamps[i-1]) {
            printf("Thread %d: Monotonicity violation at %d\n", thread_id, i);
            free(timestamps);
            return (void*)-1;
        }
    }
    
    free(timestamps);
    return (void*)0;
}

void test_concurrent_safety() {
    printf("Testing concurrent access safety...\n");
    
    const int num_threads = 4;
    pthread_t threads[num_threads];
    int thread_ids[num_threads];
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        assert(pthread_create(&threads[i], NULL, thread_timing_test, &thread_ids[i]) == 0);
    }
    
    // Wait for threads
    for (int i = 0; i < num_threads; i++) {
        void* result;
        assert(pthread_join(threads[i], &result) == 0);
        assert(result == (void*)0); // Check thread succeeded
    }
    
    printf("  Concurrent access test passed with %d threads\n", num_threads);
}

int main() {
    printf("=== freq_timer C Consistency Tests ===\n\n");
    
    // Initialize
    assert(freq_timer_init() == 0);
    
    // Run tests
    test_monotonicity_stress();
    test_accuracy_vs_clock_gettime();
    test_timer_handle_accuracy();
    test_batch_consistency();
    test_resolution();
    test_concurrent_safety();
    
    // Cleanup
    freq_timer_cleanup();
    
    printf("\nAll consistency tests PASSED!\n");
    return 0;
}