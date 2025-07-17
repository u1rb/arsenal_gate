#include <stdio.h>
#include <stdlib.h>
#include "../include/q_spsc.h"

int main() {
    printf("Creating queue...\n");
    
    // Try with smaller capacity
    size_t capacity = 1024;
    QSPSCQueue queue = qspsc_new(capacity, QSPSC_HUGE_PAGES_NEVER, 50);
    
    if (!queue) {
        fprintf(stderr, "Failed to create queue\n");
        return 1;
    }
    
    printf("Queue created successfully!\n");
    printf("Capacity: %zu\n", qspsc_capacity(queue));
    
    printf("Destroying queue...\n");
    qspsc_destroy(queue);
    printf("Done!\n");
    
    return 0;
}