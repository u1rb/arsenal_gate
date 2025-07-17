#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/q_spsc.h"

typedef struct {
    uint32_t sequence;
    char message[60];
} Message;

typedef struct {
    QSPSCQueue queue;
    int num_messages;
} ThreadData;

void* producer_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    for (int i = 0; i < data->num_messages; i++) {
        Message msg;
        msg.sequence = i;
        snprintf(msg.message, sizeof(msg.message), "Hello from producer: %d", i);
        
        // Try to get write space
        uint8_t* write_ptr = NULL;
        while ((write_ptr = qspsc_prepare_write(data->queue, sizeof(Message))) == NULL) {
            // Queue is full, wait a bit
            usleep(10);
        }
        
        // Copy data
        memcpy(write_ptr, &msg, sizeof(Message));
        
        // Commit the write
        qspsc_finish_and_commit_write(data->queue, sizeof(Message));
        
        if (i % 100 == 0) {
            printf("Producer: sent message %d\n", i);
        }
    }
    
    printf("Producer: finished sending %d messages\n", data->num_messages);
    return NULL;
}

void* consumer_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int received = 0;
    
    while (received < data->num_messages) {
        uint8_t* read_ptr = qspsc_prepare_read(data->queue);
        if (read_ptr == NULL) {
            // Queue is empty, wait a bit
            usleep(10);
            continue;
        }
        
        // Read the message
        Message msg;
        memcpy(&msg, read_ptr, sizeof(Message));
        
        // Finish the read
        qspsc_finish_read(data->queue, sizeof(Message));
        qspsc_commit_read(data->queue);
        
        if (received % 100 == 0) {
            printf("Consumer: received message %d: '%s'\n", msg.sequence, msg.message);
        }
        
        received++;
    }
    
    printf("Consumer: finished receiving %d messages\n", received);
    return NULL;
}

int main() {
    // Create queue with capacity for 1024 messages
    size_t capacity = 1024 * sizeof(Message);
    printf("Creating queue with capacity %zu bytes (message size: %zu)\n", capacity, sizeof(Message));
    QSPSCQueue queue = qspsc_new(capacity, QSPSC_HUGE_PAGES_TRY, 50);
    
    if (!queue) {
        fprintf(stderr, "Failed to create queue\n");
        return 1;
    }
    
    printf("Created SPSC queue with capacity: %zu bytes\n", qspsc_capacity(queue));
    printf("Huge pages policy: %d\n", qspsc_huge_pages_policy(queue));
    
    ThreadData data = {
        .queue = queue,
        .num_messages = 1000
    };
    
    pthread_t producer, consumer;
    
    // Start threads
    pthread_create(&producer, NULL, producer_thread, &data);
    pthread_create(&consumer, NULL, consumer_thread, &data);
    
    // Wait for threads to finish
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    // Cleanup
    qspsc_destroy(queue);
    
    printf("Test completed successfully!\n");
    return 0;
}