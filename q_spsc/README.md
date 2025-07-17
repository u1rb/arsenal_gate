# Q SPSC Queue - Header-Only Library for HFT

A high-performance, header-only Single Producer Single Consumer (SPSC) queue library extracted from the [Quill logging library](https://github.com/odygrd/quill). Optimized for ultra-low latency in High-Frequency Trading (HFT) applications.

## Quick Start

```bash
# Build and run quick performance test
./run.sh --quick

# Build and run full benchmark suite
./run.sh

# Build only
./run.sh --build-only

# Clean build and run all tests
./run.sh --clean --tests

# See all options
./run.sh --help
```

## Features

- **Ultra-low latency**: Sub-100ns message passing between threads
- **Lock-free**: Wait-free operations for both producer and consumer
- **Cache-optimized**: Separate cache lines for producer/consumer state
- **x86 optimizations**: Cache prefetching and flushing for optimal performance
- **Zero allocations**: Fixed memory footprint after initialization
- **Huge pages support**: Reduced TLB misses for large buffers (Linux)
- **Header-only**: Easy integration with no compilation required
- **HFT-focused**: No dynamic allocations, predictable latency

## Performance Characteristics

Based on benchmarks with 64-byte messages on modern x86_64 hardware:

- **Latency**: 50-90ns median, <150ns 99th percentile
- **Throughput**: 15-25 million messages/second
- **Memory**: Fixed footprint, no runtime allocations

## Requirements

- C++17 or later
- CMake 3.14+ (for building examples/benchmarks)
- x86_64 architecture recommended for best performance
- Linux or Windows

## Installation

### Header-Only Usage

Simply copy the `include/q_spsc` directory to your project:

```bash
cp -r include/q_spsc /path/to/your/project/include/
```

### CMake Integration with FetchContent

Add to your CMakeLists.txt:

```cmake
include(FetchContent)

FetchContent_Declare(
    q_spsc
    SOURCE_DIR /path/to/q_spsc  # Use absolute path to the library
)

FetchContent_MakeAvailable(q_spsc)

target_link_libraries(your_target PRIVATE q_spsc::q_spsc)
```

Example with relative path:

```cmake
FetchContent_Declare(
    q_spsc
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/q_spsc
)
```

## Usage Example

```cpp
#include <q_spsc/BoundedSPSCQueue.h>
#include <thread>

struct Message {
    uint64_t timestamp;
    uint64_t data;
    char padding[48];  // Total 64 bytes (one cache line)
};

int main() {
    // Create queue with capacity for 1024 messages
    q_spsc::BoundedSPSCQueue<size_t> queue(1024 * sizeof(Message));
    
    // Producer thread
    std::thread producer([&queue]() {
        Message msg{};
        msg.timestamp = 12345;
        msg.data = 42;
        
        // Write message
        if (auto* write_pos = queue.prepare_write(sizeof(Message))) {
            std::memcpy(write_pos, &msg, sizeof(Message));
            queue.finish_and_commit_write(sizeof(Message));
        }
    });
    
    // Consumer thread
    std::thread consumer([&queue]() {
        while (queue.empty()) {
            std::this_thread::yield();
        }
        
        // Read message
        if (auto* read_pos = queue.prepare_read()) {
            auto* msg = reinterpret_cast<Message*>(read_pos);
            // Process message...
            queue.finish_read(sizeof(Message));
            queue.commit_read();
        }
    });
    
    producer.join();
    consumer.join();
    
    return 0;
}
```

## Queue Type

### BoundedSPSCQueue

Fixed-capacity queue optimized for lowest latency:

```cpp
// Basic usage
BoundedSPSCQueue<size_t> queue(capacity_bytes);

// With huge pages (Linux)
BoundedSPSCQueue<size_t> queue(capacity_bytes, HugePagesPolicy::Try);

// Custom reader batch size (default 5% of capacity)
BoundedSPSCQueue<size_t> queue(capacity_bytes, HugePagesPolicy::Never, 10);
```

## Building Examples and Benchmarks

Using the run.sh script (recommended):
```bash
# Build everything
./run.sh --build-only

# Clean build
./run.sh --clean --build-only

# Build and run tests
./run.sh --tests

# Build and run examples
./run.sh --examples

# Install the library
./run.sh --install
```

Manual build:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DQ_SPSC_BUILD_EXAMPLES=ON \
         -DQ_SPSC_BUILD_BENCHMARKS=ON \
         -DQ_SPSC_BUILD_TESTS=ON
make -j

# Run examples
./examples/basic_example
./examples/hft_example

# Run benchmarks
./benchmarks/latency_benchmark --messages 10000000
./benchmarks/throughput_benchmark --capacity 32
```

## Benchmark Options

### Latency Benchmark
```bash
./benchmarks/latency_benchmark [options]
  --messages N       Number of messages (default: 1000000)
  --capacity N       Queue capacity in messages (default: 4096)
  --huge-pages       Enable huge pages
  --producer-cpu N   CPU for producer thread (default: 1)
  --consumer-cpu N   CPU for consumer thread (default: 2)
```

### Throughput Benchmark
```bash
./benchmarks/throughput_benchmark [options]
  --messages N       Number of messages (default: 100000000)
  --capacity N       Queue capacity in MB (default: 16)
  --producer-cpu N   CPU for producer thread (default: 1)
  --consumer-cpu N   CPU for consumer thread (default: 2)
```

## HFT Best Practices

1. **CPU Affinity**: Pin producer and consumer threads to separate CPU cores
2. **Message Size**: Keep messages small and cache-line aligned (64 bytes)
3. **Queue Capacity**: Use power-of-2 sizes, minimum 1024 for x86 optimizations
4. **Huge Pages**: Enable on Linux for reduced TLB misses
5. **Batch Operations**: Use separate prepare/finish/commit calls for batching

## Implementation Details

- **Ring Buffer**: Power-of-2 sized circular buffer with masking
- **Cache Layout**: Producer and consumer state on separate cache lines
- **Memory Ordering**: Careful use of memory_order_acquire/release
- **x86 Optimizations**: `_mm_prefetch` and `_mm_clflushopt` instructions
- **Huge Pages**: mmap with MAP_HUGETLB flag on Linux

## License

This library is derived from the Quill logging library and is distributed under the MIT License. See the original [Quill repository](https://github.com/odygrd/quill) for full license details.

## Acknowledgments

This SPSC queue implementation was extracted from the excellent [Quill](https://github.com/odygrd/quill) logging library by Odysseas Georgoudis.