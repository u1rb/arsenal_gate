#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <memory>

using namespace q_spsc;

// Example of an opaque data structure
// This simulates passing data between threads where the consumer doesn't know
// the exact type - just receives a void* and size
struct OpaqueDataHeader {
    uint32_t type_id;
    uint32_t data_size;
    uint64_t timestamp;
    // Followed by actual data
};

// Different types of messages that can be sent
enum MessageType : uint32_t {
    TEXT_MESSAGE = 1,
    BINARY_DATA = 2,
    CUSTOM_STRUCT = 3
};

// Example custom struct
struct CustomData {
    double price;
    uint64_t volume;
    char symbol[8];
};

// Producer function that sends various types of opaque data
void producer_thread(BoundedSPSCQueue<size_t>& queue) {
    std::cout << "Producer starting...\n";
    
    // Send different types of messages
    for (int i = 0; i < 30; ++i) {
        if (i % 3 == 0) {
            // Send text message
            std::string text = "Hello from message " + std::to_string(i);
            size_t total_size = sizeof(OpaqueDataHeader) + text.size() + 1;
            
            auto* write_pos = queue.prepare_write(total_size);
            if (write_pos) {
                auto* header = reinterpret_cast<OpaqueDataHeader*>(write_pos);
                header->type_id = TEXT_MESSAGE;
                header->data_size = text.size() + 1;
                header->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
                
                // Copy text data after header
                std::memcpy(write_pos + sizeof(OpaqueDataHeader), text.c_str(), text.size() + 1);
                
                queue.finish_and_commit_write(total_size);
            }
        } else if (i % 3 == 1) {
            // Send binary data
            std::vector<uint8_t> binary_data(64);
            for (size_t j = 0; j < binary_data.size(); ++j) {
                binary_data[j] = (i + j) % 256;
            }
            
            size_t total_size = sizeof(OpaqueDataHeader) + binary_data.size();
            
            auto* write_pos = queue.prepare_write(total_size);
            if (write_pos) {
                auto* header = reinterpret_cast<OpaqueDataHeader*>(write_pos);
                header->type_id = BINARY_DATA;
                header->data_size = binary_data.size();
                header->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
                
                // Copy binary data after header
                std::memcpy(write_pos + sizeof(OpaqueDataHeader), binary_data.data(), binary_data.size());
                
                queue.finish_and_commit_write(total_size);
            }
        } else {
            // Send custom struct
            CustomData data;
            data.price = 100.0 + i * 0.5;
            data.volume = 1000 + i * 100;
            std::snprintf(data.symbol, sizeof(data.symbol), "SYM%d", i);
            
            size_t total_size = sizeof(OpaqueDataHeader) + sizeof(CustomData);
            
            auto* write_pos = queue.prepare_write(total_size);
            if (write_pos) {
                auto* header = reinterpret_cast<OpaqueDataHeader*>(write_pos);
                header->type_id = CUSTOM_STRUCT;
                header->data_size = sizeof(CustomData);
                header->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
                
                // Copy struct data after header
                std::memcpy(write_pos + sizeof(OpaqueDataHeader), &data, sizeof(CustomData));
                
                queue.finish_and_commit_write(total_size);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    std::cout << "Producer finished\n";
}

// Consumer function that receives opaque data
void consumer_thread(BoundedSPSCQueue<size_t>& queue) {
    std::cout << "Consumer starting...\n";
    
    int messages_received = 0;
    int text_count = 0, binary_count = 0, struct_count = 0;
    
    while (messages_received < 30) {
        auto* read_pos = queue.prepare_read();
        if (read_pos) {
            // First, read the header to understand what type of data follows
            auto* header = reinterpret_cast<const OpaqueDataHeader*>(read_pos);
            
            // Get opaque pointer to actual data (void*, size)
            void* opaque_data = read_pos + sizeof(OpaqueDataHeader);
            size_t data_size = header->data_size;
            
            // Process based on type
            switch (header->type_id) {
                case TEXT_MESSAGE: {
                    // Interpret opaque data as text
                    const char* text = static_cast<const char*>(opaque_data);
                    if (text_count < 3) {
                        std::cout << "Received TEXT: " << text << " (size: " << data_size << ")\n";
                    }
                    text_count++;
                    break;
                }
                
                case BINARY_DATA: {
                    // Interpret opaque data as binary
                    const uint8_t* binary = static_cast<const uint8_t*>(opaque_data);
                    if (binary_count < 3) {
                        std::cout << "Received BINARY data (size: " << data_size << "), first 8 bytes: ";
                        for (size_t i = 0; i < std::min(size_t(8), data_size); ++i) {
                            std::cout << std::hex << (int)binary[i] << " ";
                        }
                        std::cout << std::dec << "\n";
                    }
                    binary_count++;
                    break;
                }
                
                case CUSTOM_STRUCT: {
                    // Interpret opaque data as custom struct
                    const CustomData* custom = static_cast<const CustomData*>(opaque_data);
                    if (struct_count < 3) {
                        std::cout << "Received CUSTOM: symbol=" << custom->symbol 
                                  << ", price=" << custom->price 
                                  << ", volume=" << custom->volume 
                                  << " (size: " << data_size << ")\n";
                    }
                    struct_count++;
                    break;
                }
                
                default:
                    std::cout << "Unknown message type: " << header->type_id << "\n";
                    break;
            }
            
            // Finish reading - total size is header + data
            size_t total_size = sizeof(OpaqueDataHeader) + header->data_size;
            queue.finish_read(total_size);
            queue.commit_read();
            
            messages_received++;
        } else {
            std::this_thread::yield();
        }
    }
    
    std::cout << "\nConsumer finished\n";
    std::cout << "Summary: " << text_count << " text, " 
              << binary_count << " binary, " 
              << struct_count << " struct messages\n";
}

int main() {
    std::cout << "=== Opaque Data Structure Example ===\n\n";
    std::cout << "This example demonstrates passing opaque data (void*, size) through the queue.\n";
    std::cout << "The producer sends different types of data with headers,\n";
    std::cout << "and the consumer interprets them based on type information.\n\n";
    
    // Create queue with sufficient capacity
    const size_t capacity = 64 * 1024; // 64KB
    BoundedSPSCQueue<size_t> queue(capacity);
    
    std::cout << "Queue capacity: " << queue.capacity() << " bytes\n\n";
    
    // Create producer and consumer threads
    std::thread producer(producer_thread, std::ref(queue));
    std::thread consumer(consumer_thread, std::ref(queue));
    
    // Wait for completion
    producer.join();
    consumer.join();
    
    std::cout << "\n=== Example complete ===\n";
    
    return 0;
}