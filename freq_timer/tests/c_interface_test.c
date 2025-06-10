#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "freq_timer.h"

void test_basic_functionality() {
    printf("Testing basic functionality...\n");
    
    // Initialize
    assert(freq_timer_init() == 0);
    
    // Get timestamps
    uint64_t t1 = freq_timer_now_ns();
    uint64_t t2 = freq_timer_now_ns();
    
    // Monotonic guarantee
    assert(t2 >= t1);
    printf("  Monotonic check: PASSED (t1=%lu, t2=%lu)\n", t1, t2);
    
    // Sleep and measure
    usleep(1000); // 1ms
    uint64_t t3 = freq_timer_now_ns();
    uint64_t elapsed = t3 - t2;
    
    // Should be at least 1ms
    assert(elapsed >= 1000000);
    // But not more than 10ms (accounting for scheduling)
    assert(elapsed < 10000000);
    printf("  Sleep measurement: PASSED (elapsed=%lu ns)\n", elapsed);
}

void test_timer_handle() {
    printf("Testing timer handle...\n");
    
    struct freq_timer_handle timer;
    assert(freq_timer_start(&timer) == 0);
    
    // Do some work
    volatile int sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += i;
    }
    
    uint64_t elapsed_ns = freq_timer_elapsed_ns(&timer);
    uint64_t elapsed_cycles = freq_timer_elapsed_cycles(&timer);
    
    assert(elapsed_ns > 0);
    assert(elapsed_cycles > 0);
    
    printf("  Timer handle: PASSED (elapsed=%lu ns, %lu cycles)\n", 
           elapsed_ns, elapsed_cycles);
}

void test_batch_timing() {
    printf("Testing batch timing...\n");
    
    struct freq_timer_batch batch;
    const size_t capacity = 10;
    
    assert(freq_timer_batch_init(&batch, capacity) == 0);
    
    // Capture timestamps
    for (size_t i = 0; i < capacity; i++) {
        assert(freq_timer_batch_capture(&batch) == 0);
        usleep(100); // 100us between captures
    }
    
    // Should fail when full
    assert(freq_timer_batch_capture(&batch) == -1);
    
    // Verify monotonicity
    for (size_t i = 1; i < batch.count; i++) {
        assert(batch.timestamps[i] >= batch.timestamps[i-1]);
    }
    
    printf("  Batch timing: PASSED (captured %zu timestamps)\n", batch.count);
    
    // Cleanup
    freq_timer_batch_cleanup(&batch);
}

void test_performance() {
    printf("Testing performance...\n");
    
    const int iterations = 1000000;
    
    // Measure freq_timer_now_ns performance
    struct freq_timer_handle timer;
    freq_timer_start(&timer);
    
    for (int i = 0; i < iterations; i++) {
        volatile uint64_t t = freq_timer_now_ns();
        (void)t;
    }
    
    uint64_t elapsed = freq_timer_elapsed_ns(&timer);
    double ns_per_call = (double)elapsed / iterations;
    
    printf("  Performance: %.2f ns per call (target: < 20ns)\n", ns_per_call);
    
    // Check if we meet the performance target
    if (ns_per_call < 20.0) {
        printf("  Performance target: PASSED\n");
    } else {
        printf("  Performance target: WARNING (exceeds 20ns)\n");
    }
}

void test_calibration() {
    printf("Testing calibration...\n");
    
    // Get initial cycles per ns
    double cpn1 = freq_timer_cycles_per_ns();
    assert(cpn1 > 0.0);
    
    // Force recalibration
    assert(freq_timer_calibrate() == 0);
    
    // Get new cycles per ns
    double cpn2 = freq_timer_cycles_per_ns();
    assert(cpn2 > 0.0);
    
    printf("  Calibration: PASSED (cycles/ns: %.3f)\n", cpn2);
}

int main() {
    printf("=== freq_timer C Interface Tests ===\n\n");
    
    test_basic_functionality();
    test_timer_handle();
    test_batch_timing();
    test_performance();
    test_calibration();
    
    // Cleanup
    freq_timer_cleanup();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}