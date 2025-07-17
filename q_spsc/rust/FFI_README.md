# Q_SPSC C/C++ FFI Interface

This library provides a memory-safe, high-performance Single-Producer Single-Consumer (SPSC) queue with C/C++ FFI bindings.

## Features

- Lock-free SPSC queue implementation
- Support for Linux huge pages
- Cache-line aligned data structures
- Zero-copy API
- Memory safety through Rust

## Building

Build the Rust library:

```bash
cargo build --release
```

This will generate:
- Dynamic library: `target/release/libq_spsc.so` (Linux) or `libq_spsc.dylib` (macOS)
- Static library: `target/release/libq_spsc.a`
- Header file: `include/q_spsc.h`

## Usage

Include the header and link against the library:

```c
#include "q_spsc.h"

// Create a queue
QSPSCQueue queue = qspsc_new(capacity, QSPSC_HUGE_PAGES_TRY, 50);

// Producer side
uint8_t* write_ptr = qspsc_prepare_write(queue, size);
if (write_ptr) {
    // Write data directly to the buffer
    memcpy(write_ptr, data, size);
    qspsc_finish_and_commit_write(queue, size);
}

// Consumer side
uint8_t* read_ptr = qspsc_prepare_read(queue);
if (read_ptr) {
    // Read data directly from the buffer
    process_data(read_ptr);
    qspsc_finish_read(queue, size);
    qspsc_commit_read(queue);
}

// Cleanup
qspsc_destroy(queue);
```

## API Reference

### Types

- `QSPSCQueue`: Opaque handle to the queue
- `QSPSCHugePagesPolicy`: Enum for huge pages configuration
  - `QSPSC_HUGE_PAGES_NEVER`: Never use huge pages
  - `QSPSC_HUGE_PAGES_ALWAYS`: Always use huge pages (fails if not available)
  - `QSPSC_HUGE_PAGES_TRY`: Try to use huge pages, fall back to regular pages

### Functions

See `include/q_spsc.h` for detailed documentation of all functions.

## Example

See `examples/c_example.c` for a complete producer-consumer example.

Build and run:
```bash
cd examples
make run
```

## Thread Safety

- The queue is designed for single-producer, single-consumer scenarios
- Producer functions must only be called from one thread
- Consumer functions must only be called from one thread
- The queue handle can be shared between producer and consumer threads