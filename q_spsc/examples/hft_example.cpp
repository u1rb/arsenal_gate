#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <iomanip>
#include <cstring>
#include <algorithm>

using namespace q_spsc;

// Market data message types for HFT
enum class MessageType : uint8_t {
    ORDER_ADD = 1,
    ORDER_CANCEL = 2,
    ORDER_EXECUTE = 3,
    TRADE = 4
};

#pragma pack(push, 1)
// Compact order message for minimal cache footprint
struct OrderMessage {
    uint64_t timestamp_ns;   // 8 bytes
    uint64_t order_id;       // 8 bytes
    uint32_t price;          // 4 bytes (price in cents)
    uint32_t quantity;       // 4 bytes
    MessageType type;        // 1 byte
    char symbol[7];          // 7 bytes (6 chars + null)
    // Total: 32 bytes (half cache line)
};
#pragma pack(pop)

static_assert(sizeof(OrderMessage) == 32, "OrderMessage must be 32 bytes");

// Performance statistics
struct PerfStats {
    uint64_t total_messages = 0;
    uint64_t total_latency_ns = 0;
    uint64_t min_latency_ns = UINT64_MAX;
    uint64_t max_latency_ns = 0;
    std::vector<uint64_t> latencies;
};

// Get current time in nanoseconds
inline uint64_t get_time_ns() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

int main() {
    // Create queue with 64KB capacity (2048 messages)
    // Using power of 2 for efficient masking
    const size_t capacity = 2048 * sizeof(OrderMessage);
    
    // Try to use huge pages for better TLB performance
    BoundedSPSCQueue<size_t> queue(capacity, HugePagesPolicy::Try);
    
    std::cout << "HFT SPSC Queue Performance Test\n";
    std::cout << "Queue capacity: " << queue.capacity() << " bytes ("
              << queue.capacity() / sizeof(OrderMessage) << " messages)\n";
    std::cout << "Message size: " << sizeof(OrderMessage) << " bytes\n";
    std::cout << "Cache line size: " << CACHE_LINE_SIZE << " bytes\n\n";
    
    // Shared state
    std::atomic<bool> running{true};
    PerfStats stats;
    
    // Market data producer (simulates exchange feed handler)
    std::thread producer([&]() {
        const uint64_t num_messages = 1'000'000;
        const char* symbols[] = {"AAPL", "GOOGL", "MSFT", "AMZN", "META"};
        
        std::cout << "Producer starting, sending " << num_messages << " messages...\n";
        
        for (uint64_t i = 0; i < num_messages; ++i) {
            // Prepare message
            OrderMessage msg;
            msg.timestamp_ns = get_time_ns();
            msg.order_id = i;
            msg.price = 10000 + (i % 1000);  // $100.00 to $109.99
            msg.quantity = 100 * (1 + i % 10);
            msg.type = static_cast<MessageType>(1 + (i % 4));
            std::strncpy(msg.symbol, symbols[i % 5], sizeof(msg.symbol) - 1);
            msg.symbol[sizeof(msg.symbol) - 1] = '\0';
            
            // Write to queue with retry on full
            while (true) {
                auto* write_pos = queue.prepare_write(sizeof(OrderMessage));
                if (write_pos) {
                    std::memcpy(write_pos, &msg, sizeof(OrderMessage));
                    queue.finish_and_commit_write(sizeof(OrderMessage));
                    break;
                }
                // Queue full, spin wait (in real HFT, might process other tasks)
                __builtin_ia32_pause();  // CPU pause instruction
            }
        }
        
        running = false;
        std::cout << "Producer finished\n";
    });
    
    // Trading strategy consumer (simulates order book processor)
    std::thread consumer([&]() {
        stats.latencies.reserve(1'000'000);
        
        while (running || !queue.empty()) {
            auto* read_pos = queue.prepare_read();
            if (read_pos) {
                uint64_t receive_time = get_time_ns();
                
                // Process message
                auto* msg = reinterpret_cast<OrderMessage*>(read_pos);
                uint64_t latency = receive_time - msg->timestamp_ns;
                
                // Update statistics
                stats.total_messages++;
                stats.total_latency_ns += latency;
                stats.min_latency_ns = std::min(stats.min_latency_ns, latency);
                stats.max_latency_ns = std::max(stats.max_latency_ns, latency);
                stats.latencies.push_back(latency);
                
                // Finish read
                queue.finish_read(sizeof(OrderMessage));
                queue.commit_read();
            } else {
                // Queue empty, use pause instruction instead of yield
                __builtin_ia32_pause();
            }
        }
    });
    
    // Wait for completion
    producer.join();
    consumer.join();
    
    // Calculate and display statistics
    if (stats.total_messages > 0) {
        std::cout << "\nPerformance Statistics:\n";
        std::cout << "Total messages: " << stats.total_messages << "\n";
        std::cout << "Min latency: " << stats.min_latency_ns << " ns\n";
        std::cout << "Max latency: " << stats.max_latency_ns << " ns\n";
        std::cout << "Avg latency: " << stats.total_latency_ns / stats.total_messages << " ns\n";
        
        // Calculate percentiles
        std::sort(stats.latencies.begin(), stats.latencies.end());
        auto percentile = [&](double p) {
            size_t idx = static_cast<size_t>(stats.latencies.size() * p / 100.0);
            return stats.latencies[std::min(idx, stats.latencies.size() - 1)];
        };
        
        std::cout << "50th percentile: " << percentile(50) << " ns\n";
        std::cout << "90th percentile: " << percentile(90) << " ns\n";
        std::cout << "99th percentile: " << percentile(99) << " ns\n";
        std::cout << "99.9th percentile: " << percentile(99.9) << " ns\n";
        
        // Throughput
        double duration_sec = static_cast<double>(stats.max_latency_ns) / 1e9;
        double throughput = stats.total_messages / duration_sec;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput / 1e6 << " million msgs/sec\n";
    }
    
    return 0;
}