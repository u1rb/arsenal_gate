#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <cassert>
#include <vector>

using namespace q_spsc;

#define TEST(name) std::cout << "Running: " << name << " ... "; try {
#define END_TEST std::cout << "PASSED\n"; } catch (const std::exception& e) { std::cout << "FAILED: " << e.what() << "\n"; return 1; }
#define ASSERT(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond);
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b);

int main() {
    std::cout << "SPSC Queue Unit Tests\n";
    std::cout << "====================\n\n";
    
    // Test 1: Basic bounded queue operations
    TEST("BoundedSPSCQueue basic operations")
        BoundedSPSCQueue<size_t> queue(1024);
        ASSERT_EQ(queue.capacity(), 1024);
        ASSERT(queue.empty());
        
        const char data[] = "Hello, World!";
        auto* write_pos = queue.prepare_write(sizeof(data));
        ASSERT(write_pos != nullptr);
        
        std::memcpy(write_pos, data, sizeof(data));
        queue.finish_and_commit_write(sizeof(data));
        
        ASSERT(!queue.empty());
        
        auto* read_pos = queue.prepare_read();
        ASSERT(read_pos != nullptr);
        
        char read_data[sizeof(data)];
        std::memcpy(read_data, read_pos, sizeof(data));
        queue.finish_read(sizeof(data));
        queue.commit_read();
        
        ASSERT_EQ(std::string(data), std::string(read_data));
        ASSERT(queue.empty());
    END_TEST
    
    // Test 2: Concurrent operations
    TEST("BoundedSPSCQueue concurrent operations")
        const size_t num_messages = 100000;
        BoundedSPSCQueue<size_t> queue(64 * 1024);
        std::atomic<bool> done{false};
        
        std::thread producer([&]() {
            for (size_t i = 0; i < num_messages; ++i) {
                while (true) {
                    auto* write_pos = queue.prepare_write(sizeof(uint64_t));
                    if (write_pos) {
                        uint64_t value = i;
                        std::memcpy(write_pos, &value, sizeof(value));
                        queue.finish_and_commit_write(sizeof(value));
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            done = true;
        });
        
        size_t received = 0;
        while (received < num_messages) {
            auto* read_pos = queue.prepare_read();
            if (read_pos) {
                uint64_t value;
                std::memcpy(&value, read_pos, sizeof(value));
                ASSERT_EQ(value, received);
                
                queue.finish_read(sizeof(value));
                queue.commit_read();
                received++;
            } else if (done.load() && queue.empty()) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
        
        producer.join();
        ASSERT_EQ(received, num_messages);
        ASSERT(queue.empty());
    END_TEST
    
    // Test 3: Power of 2 capacity
    TEST("BoundedSPSCQueue power of two capacity")
        BoundedSPSCQueue<size_t> queue1(1000);
        ASSERT_EQ(queue1.capacity(), 1024);
        
        BoundedSPSCQueue<size_t> queue2(2000);
        ASSERT_EQ(queue2.capacity(), 2048);
        
        BoundedSPSCQueue<size_t> queue3(4096);
        ASSERT_EQ(queue3.capacity(), 4096);
    END_TEST
    
    // Test 4: Stress test with checksums
    
    TEST("BoundedSPSCQueue stress test")
        const size_t iterations = 100000;
        BoundedSPSCQueue<size_t> queue(16 * 1024);
        
        std::atomic<uint64_t> checksum_write{0};
        std::atomic<uint64_t> checksum_read{0};
        
        std::thread producer([&]() {
            uint64_t local_checksum = 0;
            for (size_t i = 0; i < iterations; ++i) {
                uint8_t size = 8 + (i % 56); // 8-63 bytes
                
                while (true) {
                    auto* write_pos = queue.prepare_write(size);
                    if (write_pos) {
                        write_pos[0] = static_cast<std::byte>(size);
                        for (uint8_t j = 1; j < size; ++j) {
                            write_pos[j] = static_cast<std::byte>((i + j) & 0xFF);
                            local_checksum += static_cast<uint8_t>(write_pos[j]);
                        }
                        queue.finish_and_commit_write(size);
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            checksum_write = local_checksum;
        });
        
        std::thread consumer([&]() {
            uint64_t local_checksum = 0;
            size_t received = 0;
            
            while (received < iterations) {
                auto* read_pos = queue.prepare_read();
                if (read_pos) {
                    uint8_t size = static_cast<uint8_t>(read_pos[0]);
                    for (uint8_t j = 1; j < size; ++j) {
                        local_checksum += static_cast<uint8_t>(read_pos[j]);
                    }
                    
                    queue.finish_read(size);
                    queue.commit_read();
                    received++;
                } else {
                    std::this_thread::yield();
                }
            }
            checksum_read = local_checksum;
        });
        
        producer.join();
        consumer.join();
        
        ASSERT_EQ(checksum_write.load(), checksum_read.load());
        ASSERT(queue.empty());
    END_TEST
    
    std::cout << "\nAll tests PASSED!\n";
    return 0;
}