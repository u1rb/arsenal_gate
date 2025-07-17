# High-Performance SPSC Queue in Rust

A low-latency, wait-free Single Producer Single Consumer (SPSC) queue implementation in Rust, optimized for HFT applications.

## Features

- **Ultra-low latency**: ~20-32ns median write latency
- **Cache-line aligned**: Producer and consumer data on separate cache lines
- **x86 optimizations**: Cache prefetching and flushing on x86_64
- **Zero allocations**: All memory allocated upfront
- **Wait-free**: No locks or blocking operations
- **Power-of-2 sizing**: Efficient masking operations
- **Huge pages support**: Linux huge pages for reduced TLB misses
- **Batched commits**: Reader batching reduces cache coherence traffic

## Performance Characteristics

Based on the C++ implementation benchmarks:
- Hot path write latency: 22ns (32-byte), 32ns (128-byte), 20ns (512-byte) median
- Inter-thread latency: ~215ns one-way
- Throughput: Up to 300M+ messages/second for small messages

## Usage

```rust
use q_spsc::{BoundedSPSCQueue, HugePagesPolicy};

// Create queue with 256 capacity
let queue = BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5);

// Producer
if let Some(write_pos) = queue.prepare_write(data.len()) {
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), write_pos, data.len());
    }
    queue.finish_and_commit_write(data.len());
}

// Consumer
if let Some(read_pos) = queue.prepare_read() {
    // Process data at read_pos
    queue.finish_read(bytes_read);
    queue.commit_read();
}
```

## Implementation Details

- **Memory layout**: Cache-line aligned producer/consumer structures
- **Memory ordering**: Careful use of acquire/release semantics
- **x86 optimizations**: `_mm_prefetch` and `_mm_clflushopt` for cache management
- **Batching**: Reader updates atomic position every N% of capacity
- **Power-of-2**: Capacity rounded up for efficient index masking

## Building

```bash
cargo build --release
cargo test
cargo bench
```

## Benchmarks

Run benchmarks with:
```bash
cargo bench
```

Benchmarks include:
- Single-thread round-trip latency
- Producer hot-path latency
- Ping-pong inter-thread latency
- Throughput tests