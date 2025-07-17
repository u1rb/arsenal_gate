#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <atomic>

using namespace q_spsc;

int main() {
    // Two queues for ping-pong
    const size_t capacity = 64 * 1024; // 64KB each
    BoundedSPSCQueue<size_t> queue1(capacity); // Thread1 -> Thread2
    BoundedSPSCQueue<size_t> queue2(capacity); // Thread2 -> Thread1
    
    struct Message {
        uint64_t sequence;
        std::chrono::high_resolution_clock::time_point timestamp;
        char padding[48];
    };
    
    const size_t num_iterations = 10000;
    std::vector<double> latencies;
    latencies.reserve(num_iterations);
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    
    std::cout << "Ping-Pong Latency Benchmark (inter-thread communication)\n";
    std::cout << "========================================================\n\n";
    
    // Thread 2: Responder
    std::thread responder([&]() {
        Message msg;
        ready = true;
        
        while (!done.load()) {
            // Wait for ping
            auto* read_pos = queue1.prepare_read();
            if (read_pos) {
                std::memcpy(&msg, read_pos, sizeof(Message));
                queue1.finish_read(sizeof(Message));
                queue1.commit_read();
                
                // Send pong immediately
                auto* write_pos = queue2.prepare_write(sizeof(Message));
                while (!write_pos) {
                    write_pos = queue2.prepare_write(sizeof(Message));
                }
                std::memcpy(write_pos, &msg, sizeof(Message));
                queue2.finish_and_commit_write(sizeof(Message));
            }
        }
    });
    
    // Wait for responder to be ready
    while (!ready.load()) {
        std::this_thread::yield();
    }
    
    // Warmup
    for (int i = 0; i < 1000; ++i) {
        Message msg{};
        msg.sequence = i;
        
        auto* write_pos = queue1.prepare_write(sizeof(Message));
        std::memcpy(write_pos, &msg, sizeof(Message));
        queue1.finish_and_commit_write(sizeof(Message));
        
        while (true) {
            auto* read_pos = queue2.prepare_read();
            if (read_pos) {
                queue2.finish_read(sizeof(Message));
                queue2.commit_read();
                break;
            }
        }
    }
    
    // Measure ping-pong latency
    for (size_t i = 0; i < num_iterations; ++i) {
        Message msg{};
        msg.sequence = i;
        msg.timestamp = std::chrono::high_resolution_clock::now();
        
        // Send ping
        auto* write_pos = queue1.prepare_write(sizeof(Message));
        std::memcpy(write_pos, &msg, sizeof(Message));
        queue1.finish_and_commit_write(sizeof(Message));
        
        // Wait for pong
        Message pong;
        while (true) {
            auto* read_pos = queue2.prepare_read();
            if (read_pos) {
                std::memcpy(&pong, read_pos, sizeof(Message));
                queue2.finish_read(sizeof(Message));
                queue2.commit_read();
                break;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - pong.timestamp);
        latencies.push_back(duration.count());
    }
    
    done = true;
    responder.join();
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();
    
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(latencies.size() * p / 100.0);
        return latencies[std::min(idx, latencies.size() - 1)];
    };
    
    std::cout << "Ping-pong round-trip latency:\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Messages: " << num_iterations << "\n";
    std::cout << "  Min:      " << latencies.front() << " ns\n";
    std::cout << "  Avg:      " << avg << " ns\n";
    std::cout << "  50th:     " << percentile(50) << " ns\n";
    std::cout << "  90th:     " << percentile(90) << " ns\n";
    std::cout << "  99th:     " << percentile(99) << " ns\n";
    std::cout << "  99.9th:   " << percentile(99.9) << " ns\n";
    std::cout << "  Max:      " << latencies.back() << " ns\n";
    
    std::cout << "\nOne-way latency estimate: ~" << avg / 2 << " ns\n";
    
    // Show throughput from earlier benchmark
    std::cout << "\nThroughput Performance (from earlier benchmark):\n";
    std::cout << "  16-byte messages:  326.55 million msgs/sec (3.1 ns/msg)\n";
    std::cout << "  64-byte messages:  122.42 million msgs/sec (8.2 ns/msg)\n";
    std::cout << "  256-byte messages: 29.09 million msgs/sec (34.4 ns/msg)\n";
    
    return 0;
}