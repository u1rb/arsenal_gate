#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "freq_timer.h"

void test_consistency_csv() {
    printf("Generating consistency test data...\n");
    
    FILE* fp = fopen("consistency_data.csv", "w");
    if (!fp) {
        perror("Failed to open consistency_data.csv");
        return;
    }
    
    // CSV header
    fprintf(fp, "sample_id,freq_timer_ns,std_time_ns,freq_cycles,time_diff_ns\n");
    
    const int samples = 10000;
    
    for (int i = 0; i < samples; i++) {
        struct timespec ts_start, ts_end;
        
        // Synchronized measurements
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        uint64_t freq_start_cycles = freq_timer_now_cycles();
        uint64_t freq_start_ns = freq_timer_now_ns();
        
        // Small work unit
        volatile int dummy = 0;
        for (int j = 0; j < 100; j++) {
            dummy += j;
        }
        
        uint64_t freq_end_ns = freq_timer_now_ns();
        uint64_t freq_end_cycles = freq_timer_now_cycles();
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        
        // Calculate durations
        uint64_t freq_duration_ns = freq_end_ns - freq_start_ns;
        uint64_t freq_duration_cycles = freq_end_cycles - freq_start_cycles;
        uint64_t std_duration_ns = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000UL + 
                                   (ts_end.tv_nsec - ts_start.tv_nsec);
        
        int64_t time_diff = (int64_t)freq_duration_ns - (int64_t)std_duration_ns;
        
        fprintf(fp, "%d,%lu,%lu,%lu,%ld\n", 
                i, freq_duration_ns, std_duration_ns, freq_duration_cycles, time_diff);
        
        if (i % 1000 == 0) {
            printf("  Processed %d/%d samples\n", i, samples);
        }
        
        // Small delay to avoid overwhelming the system
        if (i % 100 == 0) {
            usleep(1000); // 1ms every 100 samples
        }
    }
    
    fclose(fp);
    printf("Consistency data saved to consistency_data.csv\n");
}

void test_overhead_csv() {
    printf("Generating overhead test data...\n");
    
    FILE* fp = fopen("overhead_data.csv", "w");
    if (!fp) {
        perror("Failed to open overhead_data.csv");
        return;
    }
    
    // CSV header
    fprintf(fp, "method,call_number,duration_ns\n");
    
    const int iterations = 50000;
    
    // Test freq_timer_now_ns overhead
    printf("  Testing freq_timer_now_ns overhead...\n");
    for (int i = 0; i < iterations; i++) {
        struct timespec start, end;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        volatile uint64_t result = freq_timer_now_ns();
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000UL + 
                           (end.tv_nsec - start.tv_nsec);
        
        fprintf(fp, "freq_timer_now_ns,%d,%lu\n", i, duration);
    }
    
    // Test freq_timer_now_cycles overhead
    printf("  Testing freq_timer_now_cycles overhead...\n");
    for (int i = 0; i < iterations; i++) {
        struct timespec start, end;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        volatile uint64_t result = freq_timer_now_cycles();
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000UL + 
                           (end.tv_nsec - start.tv_nsec);
        
        fprintf(fp, "freq_timer_now_cycles,%d,%lu\n", i, duration);
    }
    
    // Test clock_gettime baseline
    printf("  Testing clock_gettime baseline...\n");
    for (int i = 0; i < iterations; i++) {
        struct timespec start, end, dummy;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        volatile int result = clock_gettime(CLOCK_MONOTONIC, &dummy);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000UL + 
                           (end.tv_nsec - start.tv_nsec);
        
        fprintf(fp, "clock_gettime,%d,%lu\n", i, duration);
    }
    
    fclose(fp);
    printf("Overhead data saved to overhead_data.csv\n");
}

void test_monotonicity_csv() {
    printf("Generating monotonicity test data...\n");
    
    FILE* fp = fopen("monotonicity_data.csv", "w");
    if (!fp) {
        perror("Failed to open monotonicity_data.csv");
        return;
    }
    
    // CSV header
    fprintf(fp, "call_number,timestamp_ns,timestamp_cycles,delta_ns,delta_cycles,violation\n");
    
    const int samples = 100000;
    uint64_t prev_ns = freq_timer_now_ns();
    uint64_t prev_cycles = freq_timer_now_cycles();
    
    for (int i = 1; i < samples; i++) {
        uint64_t curr_ns = freq_timer_now_ns();
        uint64_t curr_cycles = freq_timer_now_cycles();
        
        int64_t delta_ns = (int64_t)curr_ns - (int64_t)prev_ns;
        int64_t delta_cycles = (int64_t)curr_cycles - (int64_t)prev_cycles;
        
        int violation = (delta_ns < 0) ? 1 : 0;
        
        fprintf(fp, "%d,%lu,%lu,%ld,%ld,%d\n", 
                i, curr_ns, curr_cycles, delta_ns, delta_cycles, violation);
        
        if (i % 10000 == 0) {
            printf("  Processed %d/%d samples\n", i, samples);
        }
        
        prev_ns = curr_ns;
        prev_cycles = curr_cycles;
    }
    
    fclose(fp);
    printf("Monotonicity data saved to monotonicity_data.csv\n");
}

void test_resolution_csv() {
    printf("Generating resolution test data...\n");
    
    FILE* fp = fopen("resolution_data.csv", "w");
    if (!fp) {
        perror("Failed to open resolution_data.csv");
        return;
    }
    
    // CSV header
    fprintf(fp, "measurement_id,time_diff_ns,cycles_diff,zero_diff\n");
    
    const int samples = 20000;
    
    for (int i = 0; i < samples; i++) {
        uint64_t t1_ns = freq_timer_now_ns();
        uint64_t c1 = freq_timer_now_cycles();
        uint64_t t2_ns = freq_timer_now_ns();
        uint64_t c2 = freq_timer_now_cycles();
        
        uint64_t time_diff = t2_ns - t1_ns;
        uint64_t cycles_diff = c2 - c1;
        int zero_diff = (time_diff == 0) ? 1 : 0;
        
        fprintf(fp, "%d,%lu,%lu,%d\n", i, time_diff, cycles_diff, zero_diff);
        
        if (i % 2000 == 0) {
            printf("  Processed %d/%d samples\n", i, samples);
        }
    }
    
    fclose(fp);
    printf("Resolution data saved to resolution_data.csv\n");
}

int main() {
    printf("=== freq_timer CSV Data Generation ===\n\n");
    
    // Initialize
    if (freq_timer_init() != 0) {
        fprintf(stderr, "Failed to initialize freq_timer\n");
        return 1;
    }
    
    // Wait for calibration to settle
    printf("Waiting for calibration to settle...\n");
    sleep(1);
    
    // Generate all test data
    test_consistency_csv();
    printf("\n");
    
    test_overhead_csv();
    printf("\n");
    
    test_monotonicity_csv();
    printf("\n");
    
    test_resolution_csv();
    printf("\n");
    
    // Cleanup
    freq_timer_cleanup();
    
    printf("All CSV data files generated successfully!\n");
    printf("Files created:\n");
    printf("  - consistency_data.csv\n");
    printf("  - overhead_data.csv\n");
    printf("  - monotonicity_data.csv\n");
    printf("  - resolution_data.csv\n");
    
    return 0;
}