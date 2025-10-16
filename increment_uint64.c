#include <stdio.h>
#include <stdint.h>
#include <time.h>

int main() {
    uint64_t counter = 0;
    const uint64_t target = 1000000000;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (counter < target) {
        counter++;
        // Inline assembly that uses counter as input/output
        // This creates a data dependency forcing the loop to execute
        // The empty assembly with "+r" means: read and write counter
        // but don't generate any actual instructions
        __asm__ __volatile__("" : "+r"(counter));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Counter reached: %lu\n", counter);
    printf("Elapsed time: %.9f seconds\n", elapsed);

    // Calculate time to saturate uint64_t (2^64 - 1 = 18,446,744,073,709,551,615)
    const uint64_t uint64_max = UINT64_MAX;
    double rate = (double)counter / elapsed;  // increments per second
    double time_to_saturate = (double)uint64_max / rate;  // seconds

    double years = time_to_saturate / (365.25 * 24 * 3600);

    printf("Time to saturate uint64_t: %.3f seconds (%.6f years)\n",
           time_to_saturate, years);

    return 0;
}
