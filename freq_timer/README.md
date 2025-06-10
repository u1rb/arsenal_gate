# freq_timer - High-Performance Wall Clock Library

A high-performance wall clock library implemented in Rust with C FFI interface, providing nanosecond precision timing optimized for single-threaded usage.

## Features

- **Ultra-low latency**: ~30ns per timing call (target <20ns on bare metal)
- **High precision**: Sub-100ns resolution using CPU cycle counters (RDTSC)
- **Single-threaded optimization**: Zero synchronization overhead in hot path
- **Background calibration**: Automatic frequency scaling compensation
- **Monotonic guarantees**: Time never goes backwards
- **C/C++ compatible**: Full FFI interface with automatic header generation
- **Cross-platform**: Linux x86_64 and aarch64 support
- **Production ready**: Comprehensive testing and error handling

## Quick Start

### Building

```bash
# Build the library
make build

# Show all available targets
make help
```

### Basic Usage (C)

```c
#include "freq_timer.h"

int main() {
    // Initialize the timer library
    freq_timer_init();
    
    // Get current timestamp
    uint64_t now = freq_timer_now_ns();
    
    // Measure elapsed time
    struct freq_timer_handle timer;
    freq_timer_start(&timer);
    
    // ... do some work ...
    
    uint64_t elapsed = freq_timer_elapsed_ns(&timer);
    printf("Elapsed: %lu ns\n", elapsed);
    
    // Cleanup
    freq_timer_cleanup();
    return 0;
}
```

### Basic Usage (Rust)

```rust
use freq_timer::timer;

fn main() {
    freq_timer::initialize().unwrap();
    
    let start = timer::now_nanoseconds();
    
    // ... do some work ...
    
    let elapsed = timer::now_nanoseconds() - start;
    println!("Elapsed: {} ns", elapsed);
    
    freq_timer::cleanup();
}
```

## Testing

### Basic Tests

Run the standard test suite including unit tests and basic C interface validation:

```bash
make test
```

This will:
- Run Rust unit tests
- Build and execute C interface tests
- Validate basic functionality and monotonicity

### Consistency Tests

Run comprehensive consistency and accuracy validation:

```bash
make test-consistency
```

**Rust Consistency Tests:**
- ✅ Basic monotonicity (10,000 consecutive calls)
- ✅ Resolution and precision validation
- ✅ Timer handle consistency
- ✅ Long duration accuracy
- ✅ Concurrent access safety
- ✅ Calibration stability over time

**C Interface Consistency Tests:**
- ✅ Monotonicity stress test (100,000 timestamps)
- ✅ Accuracy vs `clock_gettime()` 
- ✅ Timer handle accuracy validation
- ✅ Batch timing consistency
- ✅ Resolution measurement
- ✅ Multi-threaded safety verification

### All Tests

Run both basic and consistency tests:

```bash
make test-all
```

### Expected Results

**On VMware VM (this environment):**
```
Monotonicity: 100,000 timestamps, 0 violations (0.000%)
Accuracy vs system clock: Average error 0.007%, max 0.013%
Resolution: ~50-70ns minimum detectable difference
Performance: ~30ns per timing call
```

**On bare metal hardware:**
- Performance: Expected <20ns per timing call
- Accuracy: <0.01% vs system clocks
- Resolution: <50ns minimum difference

## Performance Benchmarking

### Quick Benchmark

```bash
make bench
```

This runs comprehensive Criterion benchmarks measuring:

- `freq_timer_now_ns()` - Main timing function performance
- `freq_timer_now_cycles()` - Raw CPU cycle counter access
- `std::time::Instant::now()` - Comparison baseline
- Timer handle operations
- FFI overhead analysis
- Batch operations (10, 100, 1000 timestamps)

### Benchmark Output

The benchmark generates detailed statistical analysis including:
- **Mean execution time** with confidence intervals
- **Outlier detection** and analysis
- **Performance regression** detection
- **HTML reports** with graphs (saved to `target/criterion/`)

### Sample Results (VMware VM)

```
freq_timer_now_ns       time:   [29.786 ns 30.033 ns 30.300 ns]
freq_timer_now_cycles   time:   [12.149 ns 12.251 ns 12.351 ns]
std_time_instant_now    time:   [21.284 ns 21.430 ns 21.579 ns]
```

**Key Insights:**
- Raw cycle counter: ~12ns (shows true potential)
- Full timing function: ~30ns (including calibration lookup)
- Competitive with std::time on VM, expected to beat it on bare metal

### Viewing Detailed Results

After running benchmarks:

```bash
# View HTML reports
open target/criterion/reports/index.html

# Or browse the generated files
ls target/criterion/
```

### Custom Performance Testing

Create your own performance test:

```c
#include "freq_timer.h"
#include <time.h>

void measure_overhead() {
    freq_timer_init();
    
    const int iterations = 1000000;
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        volatile uint64_t t = freq_timer_now_ns();
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    uint64_t total_ns = (end.tv_sec - start.tv_sec) * 1000000000UL + 
                        (end.tv_nsec - start.tv_nsec);
    
    printf("Average overhead: %.2f ns per call\n", 
           (double)total_ns / iterations);
    
    freq_timer_cleanup();
}
```

## Performance Analysis

### Understanding the Numbers

**Target Performance:** <20ns per call
- **Achieved on VM:** ~30ns (limited by virtualization overhead)
- **Expected on bare metal:** <20ns based on 12ns raw cycle counter access

**Why 30ns on VM?**
1. **Virtualization overhead**: VMware adds latency to RDTSC instruction
2. **CPU scaling**: Virtual CPUs have different timing characteristics
3. **Cache effects**: VM memory layout affects cache performance

**Raw cycle counter (12ns) shows true potential:**
- Direct RDTSC instruction execution
- Minimal overhead for basic time measurement
- Indicates bare metal would easily achieve <20ns target

### Optimization Notes

The library is designed for **single-threaded usage** to achieve maximum performance:

- **Zero synchronization** in hot path
- **Lock-free calibration** updates via double-buffering
- **Cache-aligned** data structures
- **Background calibration** thread handles all syscalls

## Architecture

### Single-Threaded Design

```
User Thread (Hot Path)          Background Thread
┌─────────────────┐            ┌──────────────────┐
│ freq_timer_now_ns() │        │  Calibration     │
│                 │            │                  │
│ 1. Read cycles  │            │ 1. Sample cycles │
│ 2. Apply factor │◄───────────┤ 2. Sample OS time│
│ 3. Return ns    │            │ 3. Update factors│
└─────────────────┘            └──────────────────┘
     ~12-30ns                       Every 1 second
```

### Key Components

- **Platform layer**: RDTSC/CNTVCT access
- **Timer module**: High-level timing API
- **Calibration**: Background frequency tracking
- **FFI layer**: C interface with safety guarantees

## Platform Support

| Platform | Architecture | Status | Performance |
|----------|--------------|---------|-------------|
| Linux    | x86_64      | ✅ Full | ~30ns (VM) / <20ns (bare metal) |
| Linux    | aarch64     | ✅ Full | ~40ns (estimated) |
| Windows  | x86_64      | ⚠️ Planned | TBD |
| macOS    | x86_64/ARM  | ⚠️ Planned | TBD |

## Installation

### System-wide Installation

```bash
make install
```

This installs:
- `/usr/local/lib/libfreq_timer.{a,so}` - Static and dynamic libraries
- `/usr/local/include/freq_timer.h` - C header file

### Package Managers

```bash
# Add to Cargo.toml for Rust projects
[dependencies]
freq_timer = { path = "path/to/freq_timer" }
```

```cmake
# CMake integration
find_library(FREQ_TIMER freq_timer PATHS /usr/local/lib)
target_link_libraries(your_target ${FREQ_TIMER})
```

## Examples

### High-Frequency Measurement

```c
// Batch timing for minimal per-measurement overhead
struct freq_timer_batch batch;
freq_timer_batch_init(&batch, 1000);

for (int i = 0; i < 1000; i++) {
    freq_timer_batch_capture(&batch);
    // Your code here
}

// Process timestamps
for (size_t i = 1; i < batch.count; i++) {
    uint64_t diff = batch.timestamps[i] - batch.timestamps[i-1];
    printf("Iteration %zu: %lu ns\n", i, diff);
}

freq_timer_batch_cleanup(&batch);
```

### Rust Integration

```rust
use freq_timer::timer;

fn benchmark_function<F>(f: F) -> u64 
where F: FnOnce() {
    let start = timer::now_nanoseconds();
    f();
    timer::now_nanoseconds() - start
}

let duration = benchmark_function(|| {
    // Code to benchmark
    expensive_computation();
});

println!("Function took {} ns", duration);
```

## Troubleshooting

### Common Issues

**"Performance target exceeded"**: Normal on virtualized environments
```bash
# Check if running in VM
systemd-detect-virt
# Expected: higher latency on VMs
```

**"Calibration unstable"**: CPU frequency scaling active
```bash
# Check CPU governor
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# Consider setting to 'performance' for consistent timing
```

**"Build failures"**: Missing dependencies
```bash
# Install required packages
sudo apt install build-essential pkg-config
```

### Performance Tips

1. **Pin CPU affinity** for consistent results
2. **Disable CPU frequency scaling** during measurements
3. **Use performance CPU governor**
4. **Minimize system load** during benchmarking
5. **Run on bare metal** for best performance

## Contributing

1. Fork the repository
2. Create a feature branch
3. Run all tests: `make test-all`
4. Ensure clippy passes: `cargo clippy`
5. Submit a pull request

## License

This project is licensed under MIT OR Apache-2.0 - see the LICENSE files for details.