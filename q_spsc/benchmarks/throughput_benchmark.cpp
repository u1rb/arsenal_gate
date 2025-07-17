#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <iomanip>

using namespace q_spsc;

// Test different message sizes
struct Message16 { char data[16]; };
struct Message64 { char data[64]; };
struct Message256 { char data[256]; };
struct Message1024 { char data[1024]; };

template<typename MessageType>
void run_throughput_test(const std::string& name, size_t num_messages, 
                        size_t queue_capacity_bytes) {
    BoundedSPSCQueue<size_t> queue(queue_capacity_bytes);
    
    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = std::chrono::steady_clock::now();
    
    // Consumer thread
    std::thread consumer([&]() {
        // Wait for start signal
        while (!start.load()) {
            std::this_thread::yield();
        }
        
        MessageType msg;
        uint64_t local_received = 0;
        
        while (!done.load() || !queue.empty()) {
            auto* read_pos = queue.prepare_read();
            if (read_pos) {
                // Just copy the data to ensure it's read
                std::memcpy(&msg, read_pos, sizeof(MessageType));
                queue.finish_read(sizeof(MessageType));
                queue.commit_read();
                local_received++;
                
                // Periodically update atomic counter
                if (local_received % 10000 == 0) {
                    messages_received.store(local_received);
                }
            } else {
                __builtin_ia32_pause();
            }
        }
        
        messages_received.store(local_received);
        end_time = std::chrono::steady_clock::now();
    });
    
    // Producer thread
    std::thread producer([&]() {
        MessageType msg;
        std::memset(&msg, 0x42, sizeof(MessageType));
        
        // Signal start
        start = true;
        start_time = std::chrono::steady_clock::now();
        
        for (size_t i = 0; i < num_messages; ++i) {
            // Busy wait if queue is full
            while (true) {
                auto* write_pos = queue.prepare_write(sizeof(MessageType));
                if (write_pos) {
                    std::memcpy(write_pos, &msg, sizeof(MessageType));
                    queue.finish_and_commit_write(sizeof(MessageType));
                    break;
                }
                __builtin_ia32_pause();
            }
            
            // Periodically update counter
            if ((i + 1) % 10000 == 0) {
                messages_sent.store(i + 1);
            }
        }
        
        messages_sent.store(num_messages);
        done = true;
    });
    
    // Monitor thread - prints progress
    std::thread monitor([&]() {
        while (!done.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            uint64_t sent = messages_sent.load();
            uint64_t received = messages_received.load();
            
            if (sent > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count();
                if (elapsed > 0) {
                    double rate = static_cast<double>(sent) / elapsed * 1000.0 / 1e6;
                    std::cout << "\rProgress: " << sent << " sent, " << received 
                              << " received (" << std::fixed << std::setprecision(2) 
                              << rate << " M msg/s)" << std::flush;
                }
            }
        }
    });
    
    // Wait for completion
    producer.join();
    consumer.join();
    monitor.join();
    
    // Calculate results
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    double seconds = duration.count() / 1e9;
    uint64_t final_received = messages_received.load();
    
    double messages_per_second = final_received / seconds;
    double mbytes_per_second = (final_received * sizeof(MessageType)) / seconds / (1024.0 * 1024.0);
    double ns_per_message = duration.count() / static_cast<double>(final_received);
    
    // Clear progress line and print results
    std::cout << "\r" << std::string(80, ' ') << "\r";
    std::cout << name << " Results:\n";
    std::cout << "  Messages: " << final_received << "\n";
    std::cout << "  Duration: " << std::fixed << std::setprecision(3) << seconds << " seconds\n";
    std::cout << "  Throughput: " << std::setprecision(2) << messages_per_second / 1e6 
              << " million msgs/sec\n";
    std::cout << "  Bandwidth: " << std::setprecision(2) << mbytes_per_second << " MB/s\n";
    std::cout << "  Latency: " << std::setprecision(1) << ns_per_message << " ns/msg\n\n";
}

int main(int argc, char* argv[]) {
    // Default configuration
    size_t num_messages = 100'000'000;  // 100M messages
    size_t queue_capacity_mb = 16;       // 16MB queue
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--messages" && i + 1 < argc) {
            num_messages = std::stoul(argv[++i]);
        } else if (arg == "--capacity" && i + 1 < argc) {
            queue_capacity_mb = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --messages N       Number of messages (default: 100000000)\n";
            std::cout << "  --capacity N       Queue capacity in MB (default: 16)\n";
            return 0;
        }
    }
    
    size_t queue_capacity_bytes = queue_capacity_mb * 1024 * 1024;
    
    std::cout << "SPSC Queue Throughput Benchmark\n";
    std::cout << "================================\n";
    std::cout << "Configuration:\n";
    std::cout << "  Messages: " << num_messages << "\n";
    std::cout << "  Queue capacity: " << queue_capacity_mb << " MB\n";
    
    // Run tests with different message sizes
    run_throughput_test<Message16>("16-byte messages", num_messages, queue_capacity_bytes);
    run_throughput_test<Message64>("64-byte messages", num_messages, queue_capacity_bytes);
    run_throughput_test<Message256>("256-byte messages", num_messages / 4, queue_capacity_bytes);
    run_throughput_test<Message1024>("1024-byte messages", num_messages / 16, queue_capacity_bytes);
    
    return 0;
}