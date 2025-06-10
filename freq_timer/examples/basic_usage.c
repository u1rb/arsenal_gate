#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "freq_timer.h"

int main() {
    printf("=== freq_timer Basic Usage Example ===\n\n");
    
    // Initialize the timer library
    if (freq_timer_init() != 0) {
        fprintf(stderr, "Failed to initialize freq_timer\n");
        return 1;
    }
    
    // Example 1: Simple timestamp
    printf("1. Getting current timestamp:\n");
    uint64_t now = freq_timer_now_ns();
    printf("   Current time: %lu ns\n\n", now);
    
    // Example 2: Measuring elapsed time
    printf("2. Measuring elapsed time:\n");
    struct freq_timer_handle timer;
    freq_timer_start(&timer);
    
    // Simulate some work
    printf("   Doing some work...\n");
    usleep(10000); // 10ms
    
    uint64_t elapsed = freq_timer_elapsed_ns(&timer);
    printf("   Elapsed time: %.3f ms\n\n", elapsed / 1e6);
    
    // Example 3: High-frequency measurements
    printf("3. High-frequency measurements:\n");
    struct freq_timer_batch batch;
    freq_timer_batch_init(&batch, 100);
    
    for (int i = 0; i < 10; i++) {
        freq_timer_batch_capture(&batch);
        // Very short work
        volatile int x = 0;
        for (int j = 0; j < 1000; j++) x++;
    }
    
    printf("   Captured %zu timestamps\n", batch.count);
    printf("   Time differences:\n");
    for (size_t i = 1; i < batch.count && i < 5; i++) {
        uint64_t diff = batch.timestamps[i] - batch.timestamps[i-1];
        printf("     [%zu-%zu]: %lu ns\n", i-1, i, diff);
    }
    
    freq_timer_batch_cleanup(&batch);
    
    // Example 4: CPU cycles measurement
    printf("\n4. CPU cycles measurement:\n");
    uint64_t cycles1 = freq_timer_now_cycles();
    usleep(1000); // 1ms
    uint64_t cycles2 = freq_timer_now_cycles();
    
    printf("   Cycles elapsed: %lu\n", cycles2 - cycles1);
    printf("   Cycles per ns: %.3f\n", freq_timer_cycles_per_ns());
    
    // Cleanup
    freq_timer_cleanup();
    
    printf("\nExample completed!\n");
    return 0;
}