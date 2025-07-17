#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cstring>

using namespace q_spsc;

// Use RDTSC for more accurate timing on x86
#if defined(__x86_64__) || defined(__i386__)
inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

// Calibrate RDTSC frequency
double calibrate_rdtsc() {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t start_tsc = rdtsc();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t end_tsc = rdtsc();
    
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(end_tsc - start_tsc) / duration_ns;  // TSC ticks per nanosecond
}

// Minimal message for latency testing
struct LatencyMessage {
    uint64_t timestamp;
    uint64_t sequence;
    char padding[48];  // Total 64 bytes (one cache line)
};

// Test configuration
struct TestConfig {
    size_t num_messages = 1'000'000;
    size_t queue_capacity = 4096;  // messages
    size_t warmup_messages = 10'000;
    bool use_huge_pages = false;
};

void run_latency_test(const TestConfig& config) {
    // Create queue
    HugePagesPolicy huge_pages = config.use_huge_pages ? HugePagesPolicy::Try : HugePagesPolicy::Never;
    BoundedSPSCQueue<size_t> queue(config.queue_capacity * sizeof(LatencyMessage), huge_pages);
    
    // Calibrate RDTSC
    std::cout << "Calibrating RDTSC...\n";
    double tsc_freq = calibrate_rdtsc();
    std::cout << "TSC frequency: " << tsc_freq << " ticks/ns\n\n";
    
    // Results storage
    std::vector<uint64_t> latencies;
    latencies.reserve(config.num_messages);
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    
    // Consumer thread
    std::thread consumer([&]() {
        // Wait for producer to be ready
        while (!ready.load()) {
            __builtin_ia32_pause();
        }
        
        uint64_t messages_read = 0;
        
        while (!done.load() || !queue.empty()) {
            auto* read_pos = queue.prepare_read();
            if (read_pos) {
                uint64_t receive_tsc = rdtsc();
                auto* msg = reinterpret_cast<LatencyMessage*>(read_pos);
                
                // Calculate latency in TSC ticks
                uint64_t latency_ticks = receive_tsc - msg->timestamp;
                
                // Skip warmup messages
                if (messages_read >= config.warmup_messages) {
                    latencies.push_back(latency_ticks);
                }
                
                queue.finish_read(sizeof(LatencyMessage));
                queue.commit_read();
                messages_read++;
            } else {
                __builtin_ia32_pause();
            }
        }
    });
    
    // Producer thread
    std::thread producer([&]() {
        // Pre-allocate message
        LatencyMessage msg;
        std::memset(&msg, 0, sizeof(msg));
        
        // Signal ready
        ready = true;
        
        // Send messages
        for (size_t i = 0; i < config.num_messages; ++i) {
            msg.sequence = i;
            msg.timestamp = rdtsc();
            
            // Busy wait if queue is full
            while (true) {
                auto* write_pos = queue.prepare_write(sizeof(LatencyMessage));
                if (write_pos) {
                    std::memcpy(write_pos, &msg, sizeof(LatencyMessage));
                    queue.finish_and_commit_write(sizeof(LatencyMessage));
                    break;
                }
                __builtin_ia32_pause();
            }
        }
        
        done = true;
    });
    
    // Wait for completion
    producer.join();
    consumer.join();
    
    // Convert latencies to nanoseconds and calculate statistics
    std::vector<double> latencies_ns;
    latencies_ns.reserve(latencies.size());
    for (auto ticks : latencies) {
        latencies_ns.push_back(static_cast<double>(ticks) / tsc_freq);
    }
    
    // Sort for percentiles
    std::sort(latencies_ns.begin(), latencies_ns.end());
    
    // Calculate statistics
    double min_lat = latencies_ns.front();
    double max_lat = latencies_ns.back();
    double sum = 0;
    for (auto lat : latencies_ns) {
        sum += lat;
    }
    double avg_lat = sum / latencies_ns.size();
    
    // Percentiles
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(latencies_ns.size() * p / 100.0);
        return latencies_ns[std::min(idx, latencies_ns.size() - 1)];
    };
    
    // Display results
    std::cout << "Latency Benchmark Results\n";
    std::cout << "========================\n";
    std::cout << "Messages sent: " << config.num_messages << "\n";
    std::cout << "Messages measured: " << latencies_ns.size() << "\n";
    std::cout << "Queue capacity: " << config.queue_capacity << " messages\n";
    std::cout << "Huge pages: " << (config.use_huge_pages ? "enabled" : "disabled") << "\n";
    
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Latency Statistics (nanoseconds):\n";
    std::cout << "Min:     " << min_lat << " ns\n";
    std::cout << "Avg:     " << avg_lat << " ns\n";
    std::cout << "Max:     " << max_lat << " ns\n";
    std::cout << "50th:    " << percentile(50) << " ns\n";
    std::cout << "90th:    " << percentile(90) << " ns\n";
    std::cout << "99th:    " << percentile(99) << " ns\n";
    std::cout << "99.9th:  " << percentile(99.9) << " ns\n";
    std::cout << "99.99th: " << percentile(99.99) << " ns\n";
    
    // Latency distribution
    std::cout << "\nLatency Distribution:\n";
    const double buckets[] = {50, 100, 150, 200, 300, 500, 1000, 2000, 5000};
    size_t bucket_counts[sizeof(buckets)/sizeof(buckets[0]) + 1] = {0};
    
    for (auto lat : latencies_ns) {
        size_t bucket = 0;
        while (bucket < sizeof(buckets)/sizeof(buckets[0]) && lat > buckets[bucket]) {
            bucket++;
        }
        bucket_counts[bucket]++;
    }
    
    for (size_t i = 0; i < sizeof(buckets)/sizeof(buckets[0]); ++i) {
        double percent = 100.0 * bucket_counts[i] / latencies_ns.size();
        std::cout << "  < " << std::setw(5) << buckets[i] << " ns: " 
                  << std::setw(6) << std::setprecision(2) << percent << "%\n";
    }
    double percent = 100.0 * bucket_counts[sizeof(buckets)/sizeof(buckets[0])] / latencies_ns.size();
    std::cout << "  >= " << std::setw(4) << buckets[sizeof(buckets)/sizeof(buckets[0])-1] 
              << " ns: " << std::setw(6) << std::setprecision(2) << percent << "%\n";
}

int main(int argc, char* argv[]) {
    TestConfig config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--messages" && i + 1 < argc) {
            config.num_messages = std::stoul(argv[++i]);
        } else if (arg == "--capacity" && i + 1 < argc) {
            config.queue_capacity = std::stoul(argv[++i]);
        } else if (arg == "--huge-pages") {
            config.use_huge_pages = true;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --messages N       Number of messages to send (default: 1000000)\n";
            std::cout << "  --capacity N       Queue capacity in messages (default: 4096)\n";
            std::cout << "  --huge-pages       Enable huge pages\n";
            return 0;
        }
    }
    
    std::cout << "SPSC Queue Latency Benchmark\n";
    std::cout << "============================\n\n";
    
    run_latency_test(config);
    
    return 0;
}