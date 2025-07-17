#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace q_spsc;

int main() {
    // Create a small queue to ensure messages are processed immediately
    const size_t capacity = 64 * 1024; // 64KB
    BoundedSPSCQueue<size_t> queue(capacity);
    
    // Simple 64-byte message
    struct Message {
        uint64_t timestamp;
        char data[56];
    };
    
    const size_t num_iterations = 100000;
    std::vector<double> latencies;
    latencies.reserve(num_iterations);
    
    std::cout << "Single-threaded latency measurement (no thread switching)\n";
    std::cout << "=========================================================\n\n";
    
    // Warm up
    for (int i = 0; i < 1000; ++i) {
        Message msg{};
        auto* write_pos = queue.prepare_write(sizeof(Message));
        if (write_pos) {
            std::memcpy(write_pos, &msg, sizeof(Message));
            queue.finish_and_commit_write(sizeof(Message));
        }
        
        auto* read_pos = queue.prepare_read();
        if (read_pos) {
            queue.finish_read(sizeof(Message));
            queue.commit_read();
        }
    }
    
    // Measure round-trip latency
    for (size_t i = 0; i < num_iterations; ++i) {
        Message msg{};
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Write
        auto* write_pos = queue.prepare_write(sizeof(Message));
        std::memcpy(write_pos, &msg, sizeof(Message));
        queue.finish_and_commit_write(sizeof(Message));
        
        // Read
        auto* read_pos = queue.prepare_read();
        Message received;
        std::memcpy(&received, read_pos, sizeof(Message));
        queue.finish_read(sizeof(Message));
        queue.commit_read();
        
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        latencies.push_back(duration.count());
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();
    
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(latencies.size() * p / 100.0);
        return latencies[std::min(idx, latencies.size() - 1)];
    };
    
    std::cout << "Round-trip latency (write + read in same thread):\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Min:    " << latencies.front() << " ns\n";
    std::cout << "  Avg:    " << avg << " ns\n";
    std::cout << "  50th:   " << percentile(50) << " ns\n";
    std::cout << "  90th:   " << percentile(90) << " ns\n";
    std::cout << "  99th:   " << percentile(99) << " ns\n";
    std::cout << "  99.9th: " << percentile(99.9) << " ns\n";
    std::cout << "  Max:    " << latencies.back() << " ns\n";
    
    // Show per-operation estimates
    std::cout << "\nEstimated per-operation latency:\n";
    std::cout << "  Write (prepare + finish + commit): ~" << avg / 2 << " ns\n";
    std::cout << "  Read (prepare + finish + commit):  ~" << avg / 2 << " ns\n";
    
    return 0;
}