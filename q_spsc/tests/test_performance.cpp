#include <q_spsc/BoundedSPSCQueue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstring>

using namespace q_spsc;

void test_single_thread_latency() {
    std::cout << "Testing single-thread operation latency...\n";
    
    BoundedSPSCQueue<size_t> queue(64 * 1024);
    
    struct Message {
        uint64_t data[8]; // 64 bytes
    };
    
    const int warmup = 10000;
    const int iterations = 100000;
    
    // Warmup
    for (int i = 0; i < warmup; ++i) {
        Message msg{};
        auto* write_pos = queue.prepare_write(sizeof(Message));
        std::memcpy(write_pos, &msg, sizeof(Message));
        queue.finish_and_commit_write(sizeof(Message));
        
        auto* read_pos = queue.prepare_read();
        (void)read_pos; // Suppress unused variable warning
        queue.finish_read(sizeof(Message));
        queue.commit_read();
    }
    
    // Measure
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        Message msg{};
        msg.data[0] = i;
        
        auto* write_pos = queue.prepare_write(sizeof(Message));
        std::memcpy(write_pos, &msg, sizeof(Message));
        queue.finish_and_commit_write(sizeof(Message));
        
        auto* read_pos = queue.prepare_read();
        Message read_msg;
        std::memcpy(&read_msg, read_pos, sizeof(Message));
        queue.finish_read(sizeof(Message));
        queue.commit_read();
        
        if (read_msg.data[0] != static_cast<uint64_t>(i)) {
            throw std::runtime_error("Data mismatch");
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    double ns_per_op = static_cast<double>(duration.count()) / iterations;
    
    std::cout << "  Average round-trip time: " << std::fixed << std::setprecision(1) 
              << ns_per_op << " ns\n";
    std::cout << "  Per operation: ~" << ns_per_op / 2 << " ns\n";
    
    if (ns_per_op > 100) {
        throw std::runtime_error("Performance regression: single-thread latency too high");
    }
}

void test_throughput() {
    std::cout << "\nTesting throughput performance...\n";
    
    const size_t queue_size = 16 * 1024 * 1024; // 16MB
    BoundedSPSCQueue<size_t> queue(queue_size);
    
    struct Message {
        uint64_t data[8]; // 64 bytes
    };
    
    const size_t num_messages = 10'000'000;
    std::atomic<bool> done{false};
    
    auto start = std::chrono::steady_clock::now();
    
    // Producer
    std::thread producer([&]() {
        for (size_t i = 0; i < num_messages; ++i) {
            Message msg{};
            msg.data[0] = i;
            
            while (true) {
                auto* write_pos = queue.prepare_write(sizeof(Message));
                if (write_pos) {
                    std::memcpy(write_pos, &msg, sizeof(Message));
                    queue.finish_and_commit_write(sizeof(Message));
                    break;
                }
                __builtin_ia32_pause();
            }
        }
        done = true;
    });
    
    // Consumer
    size_t received = 0;
    while (received < num_messages) {
        auto* read_pos = queue.prepare_read();
        if (read_pos) {
            Message msg;
            std::memcpy(&msg, read_pos, sizeof(Message));
            if (msg.data[0] != received) {
                throw std::runtime_error("Sequence error");
            }
            
            queue.finish_read(sizeof(Message));
            queue.commit_read();
            received++;
        } else if (done.load() && queue.empty()) {
            break;
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    producer.join();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double seconds = duration.count() / 1000.0;
    double rate = num_messages / seconds / 1e6;
    
    std::cout << "  Messages sent: " << num_messages << "\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(3) << seconds << " seconds\n";
    std::cout << "  Throughput: " << std::setprecision(2) << rate << " million msgs/sec\n";
    
    if (rate < 50.0) {
        throw std::runtime_error("Performance regression: throughput too low");
    }
}

void test_latency_percentiles() {
    std::cout << "\nTesting latency percentiles...\n";
    
    BoundedSPSCQueue<size_t> queue(256 * sizeof(uint64_t));
    
    const int iterations = 10000;
    std::vector<double> latencies;
    latencies.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        uint64_t value = i;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        auto* write_pos = queue.prepare_write(sizeof(value));
        if (write_pos) {
            std::memcpy(write_pos, &value, sizeof(value));
            queue.finish_and_commit_write(sizeof(value));
        }
        
        auto* read_pos = queue.prepare_read();
        if (read_pos) {
            queue.finish_read(sizeof(value));
            queue.commit_read();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        latencies.push_back(duration.count());
    }
    
    std::sort(latencies.begin(), latencies.end());
    
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(latencies.size() * p / 100.0);
        return latencies[std::min(idx, latencies.size() - 1)];
    };
    
    std::cout << "  50th percentile: " << percentile(50) << " ns\n";
    std::cout << "  90th percentile: " << percentile(90) << " ns\n";
    std::cout << "  99th percentile: " << percentile(99) << " ns\n";
    
    if (percentile(99) > 1000) {
        throw std::runtime_error("Performance regression: 99th percentile too high");
    }
}

int main() {
    std::cout << "SPSC Queue Performance Tests\n";
    std::cout << "============================\n\n";
    
    try {
        test_single_thread_latency();
        test_throughput();
        test_latency_percentiles();
        
        std::cout << "\nAll performance tests PASSED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest FAILED: " << e.what() << "\n";
        return 1;
    }
}