#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <cstring>

using namespace q_spsc;

// RDTSC for more accurate timing on x86
#if defined(__x86_64__) || defined(__i386__)
inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

struct Message32 { 
    uint64_t timestamp;
    char data[24];
};

struct Message128 { 
    uint64_t timestamp;
    char data[120];
};

struct Message512 { 
    uint64_t timestamp;
    char data[504];
};

static_assert(sizeof(Message32) == 32);
static_assert(sizeof(Message128) == 128);
static_assert(sizeof(Message512) == 512);

template<typename MessageType>
void run_hot_path_test(const std::string& name, size_t num_messages) {
    // Queue sized for low latency - small to keep in L2 cache
    const size_t queue_capacity = 256 * sizeof(MessageType);
    BoundedSPSCQueue<size_t> queue(queue_capacity);
    
    std::vector<uint64_t> producer_latencies;
    producer_latencies.reserve(num_messages);
    
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    
    // Consumer thread - just drains the queue
    std::thread consumer([&]() {
        ready = true;
        
        MessageType msg;
        while (!done.load() || !queue.empty()) {
            auto* read_pos = queue.prepare_read();
            if (read_pos) {
                std::memcpy(&msg, read_pos, sizeof(MessageType));
                queue.finish_read(sizeof(MessageType));
                queue.commit_read();
            } else {
                __builtin_ia32_pause();
            }
        }
    });
    
    // Wait for consumer ready
    while (!ready.load()) {
        std::this_thread::yield();
    }
    
    // Producer thread - measure hot path latency
    // Warmup
    MessageType msg;
    std::memset(&msg, 0x42, sizeof(MessageType));
    
    for (int i = 0; i < 10000; ++i) {
        msg.timestamp = i;
        while (true) {
            auto* write_pos = queue.prepare_write(sizeof(MessageType));
            if (write_pos) {
                std::memcpy(write_pos, &msg, sizeof(MessageType));
                queue.finish_and_commit_write(sizeof(MessageType));
                break;
            }
            __builtin_ia32_pause();
        }
    }
    
    // Measure producer hot path
    for (size_t i = 0; i < num_messages; ++i) {
        msg.timestamp = i;
        
        uint64_t start = rdtsc();
        
        // Hot path: prepare, write, commit
        auto* write_pos = queue.prepare_write(sizeof(MessageType));
        if (write_pos) {
            std::memcpy(write_pos, &msg, sizeof(MessageType));
            queue.finish_and_commit_write(sizeof(MessageType));
            uint64_t end = rdtsc();
            producer_latencies.push_back(end - start);
        } else {
            // Queue full - retry with measurement
            while (true) {
                write_pos = queue.prepare_write(sizeof(MessageType));
                if (write_pos) {
                    std::memcpy(write_pos, &msg, sizeof(MessageType));
                    queue.finish_and_commit_write(sizeof(MessageType));
                    uint64_t end = rdtsc();
                    producer_latencies.push_back(end - start);
                    break;
                }
                __builtin_ia32_pause();
            }
        }
    }
    
    done = true;
    consumer.join();
    
    // Calibrate TSC frequency
    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t start_tsc = rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto end_time = std::chrono::high_resolution_clock::now();
    uint64_t end_tsc = rdtsc();
    
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    double tsc_freq = static_cast<double>(end_tsc - start_tsc) / duration_ns;
    
    // Convert to nanoseconds and calculate statistics
    std::vector<double> latencies_ns;
    latencies_ns.reserve(producer_latencies.size());
    for (auto cycles : producer_latencies) {
        latencies_ns.push_back(static_cast<double>(cycles) / tsc_freq);
    }
    
    std::sort(latencies_ns.begin(), latencies_ns.end());
    double sum = std::accumulate(latencies_ns.begin(), latencies_ns.end(), 0.0);
    double avg = sum / latencies_ns.size();
    
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(latencies_ns.size() * p / 100.0);
        return latencies_ns[std::min(idx, latencies_ns.size() - 1)];
    };
    
    // Output results
    std::cout << "\n" << name << " Hot Path Latency (Producer Thread):\n";
    std::cout << "================================================\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Queue capacity: " << queue_capacity / sizeof(MessageType) << " messages\n";
    std::cout << "Messages sent: " << latencies_ns.size() << "\n";
    
    std::cout << "Latency (nanoseconds):\n";
    std::cout << "  Min:     " << latencies_ns.front() << " ns\n";
    std::cout << "  Avg:     " << avg << " ns\n";
    std::cout << "  50th:    " << percentile(50) << " ns\n";
    std::cout << "  90th:    " << percentile(90) << " ns\n";
    std::cout << "  95th:    " << percentile(95) << " ns\n";
    std::cout << "  99th:    " << percentile(99) << " ns\n";
    std::cout << "  99.9th:  " << percentile(99.9) << " ns\n";
    std::cout << "  99.99th: " << percentile(99.99) << " ns\n";
    std::cout << "  Max:     " << latencies_ns.back() << " ns\n";
    
    // Show distribution
    std::cout << "\nLatency Distribution:\n";
    const double buckets[] = {20, 30, 40, 50, 60, 80, 100, 150, 200, 500};
    for (auto bucket : buckets) {
        size_t count = std::count_if(latencies_ns.begin(), latencies_ns.end(),
                                     [bucket](double lat) { return lat < bucket; });
        double percent = 100.0 * count / latencies_ns.size();
        std::cout << "  < " << std::setw(4) << bucket << " ns: " 
                  << std::setw(6) << std::setprecision(2) << percent << "%\n";
    }
}

int main(int argc, char* argv[]) {
    // Configuration
    size_t num_messages = 1'000'000;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--messages" && i + 1 < argc) {
            num_messages = std::stoul(argv[++i]);
        }
    }
    
    std::cout << "SPSC Queue Hot Path Latency Benchmark\n";
    std::cout << "=====================================\n";
    std::cout << "Measuring producer thread write latency for different message sizes\n";
    std::cout << "Total messages per test: " << num_messages << "\n";
    
    // Run tests for each message size
    run_hot_path_test<Message32>("32-byte messages", num_messages);
    run_hot_path_test<Message128>("128-byte messages", num_messages);
    run_hot_path_test<Message512>("512-byte messages", num_messages);
    
    std::cout << "\nNote: Latency measured is for the complete write operation:\n";
    std::cout << "prepare_write() + memcpy() + finish_and_commit_write()\n";
    
    return 0;
}