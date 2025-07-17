#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

using namespace q_spsc;

// Simple message structure
struct Message {
    uint64_t timestamp;
    uint64_t sequence;
    char data[48];  // Pad to 64 bytes total
};

int main() {
    // Create a bounded SPSC queue with capacity for 1024 messages
    const size_t capacity = 1024 * sizeof(Message);
    BoundedSPSCQueue<size_t> queue(capacity);
    
    std::cout << "Queue capacity: " << queue.capacity() << " bytes\n";
    std::cout << "Message size: " << sizeof(Message) << " bytes\n";
    
    // Producer thread
    std::thread producer([&queue]() {
        for (uint64_t i = 0; i < 100; ++i) {
            // Prepare write
            auto* write_pos = queue.prepare_write(sizeof(Message));
            if (write_pos) {
                // Cast to message pointer and fill data
                auto* msg = reinterpret_cast<Message*>(write_pos);
                msg->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
                msg->sequence = i;
                std::snprintf(msg->data, sizeof(msg->data), "Message %lu", i);
                
                // Finish and commit the write
                queue.finish_and_commit_write(sizeof(Message));
            } else {
                std::cerr << "Queue full at message " << i << "\n";
                break;
            }
            
            // Small delay to simulate real workload
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        
        std::cout << "Producer finished\n";
    });
    
    // Consumer thread
    std::thread consumer([&queue]() {
        uint64_t messages_read = 0;
        
        while (messages_read < 100) {
            // Prepare read
            auto* read_pos = queue.prepare_read();
            if (read_pos) {
                // Cast to message pointer and read data
                auto* msg = reinterpret_cast<Message*>(read_pos);
                
                // Process the message (just print for demo)
                if (messages_read < 10 || messages_read >= 90) {
                    std::cout << "Received: seq=" << msg->sequence 
                              << ", data=" << msg->data << "\n";
                }
                
                // Finish read
                queue.finish_read(sizeof(Message));
                queue.commit_read();
                
                messages_read++;
            } else {
                // Queue empty, yield CPU
                std::this_thread::yield();
            }
        }
        
        std::cout << "Consumer finished, read " << messages_read << " messages\n";
    });
    
    // Wait for both threads
    producer.join();
    consumer.join();
    
    return 0;
}