# High-Performance Wall Clock Library Project Plan

## Project Overview
Creating a high-performance wall clock library implemented in Rust with C FFI interface, providing nanosecond precision across platforms.

## Design Decisions

### Single-Threaded User API Model
- **Primary Use Case**: User timing calls (`freq_timer_now_ns()`) are made exclusively from a single thread
- **Performance Benefits**: 
  - Zero synchronization overhead in the hot path - no atomic operations or memory barriers
  - No cache line bouncing between cores for timing data
  - Predictable memory access patterns with optimal CPU cache utilization
  - Eliminates false sharing and contention on shared timing data structures
- **Design Architecture**:
  - User thread: Performs all timing calls without synchronization
  - Background calibration thread: Handles periodic calibration separately
  - Calibration updates use lock-free single-writer pattern (calibration thread writes, user thread reads)
  - Double-buffering or seqlock pattern for calibration data updates
- **Implementation Strategy**:
  - Hot path reads calibration data without atomics (single reader assumption)
  - Background thread updates calibration data with minimal coordination
  - Memory ordering handled through careful data structure design
- **Multi-threaded Safety**: Library detects and warns if timing calls come from multiple threads

### Core Technology Choices
- **Implementation Language**: Rust with C FFI interface
- **Build System**: Cargo with optional CMake wrapper for integration
- **Target Platforms**: Linux (x86_64, aarch64)
- **Primary Clock Source**: CPU cycle counter (RDTSC/RDTSCP on x86_64, CNTVCT_EL0 on ARM64)
- **Reference Clock API (Calibration Only)**: 
  - Linux: `clock_gettime(CLOCK_MONOTONIC)` 
  - **Note**: OS timing API used only for periodic calibration, NOT in hot path due to syscall overhead

### Library Architecture
- **Implementation**: Rust library compiled to static/dynamic library with C FFI exports
- **Header Generation**: Automatic C header generation using `cbindgen`
- **Thread Safety**: Rust's ownership system ensures memory safety; atomic operations for shared state
- **Memory Layout**: Zero-cost abstractions, cache-friendly structures, minimal allocation
- **Build Integration**: `capi` crate pattern for seamless C/C++ project integration

## Performance Requirements

### Target Performance: < 20ns per now() call
- **Baseline measurement**: std::chrono::high_resolution_clock::now() typically 20-40ns
- **Target**: Beat std::chrono performance with enhanced precision and features
- **Maximum overhead**: Sub-10ns for CPU cycle counter + conversion
- **Single-threaded optimization target**: < 5ns overhead leveraging lack of synchronization
- **Implementation strategies**:
  - CPU cycle counter (RDTSC/RDTSCP) as exclusive timing source in hot path (~2-5ns overhead)
  - Single multiplication operation for cycle-to-nanosecond conversion
  - Pre-computed conversion factors in static storage (no thread-local overhead)
  - **Zero syscalls in hot path**: OS timing APIs avoided completely in `freq_timer_now_ns()`
  - **Zero synchronization**: No atomic operations, memory barriers, or locks in timing path
  - Inline assembly for critical timing paths to minimize instruction count
  - Branch-free code paths in hot functions
  - Cache-line aligned calibration data structures
  - Background calibration thread updates data using lock-free patterns
  - User thread reads calibration data without any synchronization primitives

### Performance Optimization Techniques
- **Compile-time optimization**: Architecture-specific code paths using Rust `cfg` attributes
- **Runtime optimization**: CPU feature detection for x86_64 vs aarch64
- **Memory optimization**: Single cache line access per timing call
- **Instruction optimization**: Minimize syscalls, prefer user-space timers where possible

### Benchmarking Strategy
- Measure timing overhead using statistical analysis (minimum, median, 99th percentile)
- Compare against std::chrono baseline
- Test under various load conditions (single-threaded, multi-threaded, high contention)
- Validate accuracy vs system reference clocks

## Testing and Benchmarking

### Performance Benchmarking with Criterion
**Framework Choice**: Criterion.rs for statistical benchmarking
- Native Rust benchmarking with advanced statistical analysis
- Automatic warm-up, outlier detection, and regression analysis
- HTML report generation with performance graphs
- Integration with CI/CD for performance regression detection

**Benchmark Structure**:
```rust
use criterion::{criterion_group, criterion_main, Criterion};

fn bench_freq_timer(c: &mut Criterion) {
    c.bench_function("freq_timer_now_ns", |b| {
        b.iter(|| unsafe { freq_timer_now_ns() })
    });
    
    c.bench_function("std::time baseline", |b| {
        b.iter(|| std::time::Instant::now())
    });
    
    c.bench_function("freq_timer_now_cycles", |b| {
        b.iter(|| unsafe { freq_timer_now_cycles() })
    });
}

criterion_group!(benches, bench_freq_timer);
criterion_main!(benches);
```

**Performance Test Categories**:
- **Latency tests**: Single call overhead measurement (targeting < 5ns for single-threaded)
- **Throughput tests**: Sustained call rate under load
- **Single-threaded performance**: Verify optimizations when used from single thread
- **Multi-threaded degradation**: Measure overhead if accidentally used from multiple threads
- **Calibration tests**: Calibration update impact on timing calls
- **Accuracy tests**: Drift measurement over extended periods

### Benchmark Requirements
- **Target verification**: Validate < 20ns overhead requirement
- **Regression detection**: Fail CI if performance degrades > 5% or exceeds 20ns
- **Architecture comparison**: Baseline measurements across x86_64 and aarch64
- **Load testing**: Performance under various CPU utilization levels
- **Statistical rigor**: Measure minimum latency (not just average) to verify best-case performance

### C Interface Testing
**Separate C tests for FFI validation**:
```c
// tests/c_interface_test.c
#include "freq_timer.h"
#include <assert.h>
#include <time.h>

void test_basic_functionality() {
    assert(freq_timer_init() == 0);
    
    uint64_t t1 = freq_timer_now_ns();
    uint64_t t2 = freq_timer_now_ns();
    assert(t2 >= t1); // Monotonic guarantee
    
    freq_timer_cleanup();
}
```

## User Interface Design

### Primary C Interface (Low-Level, High-Performance)
```c
#ifdef __cplusplus
extern "C" {
#endif

// Core timing functions
uint64_t freq_timer_now_ns(void);
uint64_t freq_timer_now_cycles(void);
double   freq_timer_cycles_per_ns(void);

// Timer handle for elapsed time measurement
typedef struct freq_timer_handle {
    uint64_t start_cycles;
} freq_timer_handle_t;

int freq_timer_start(freq_timer_handle_t* timer);
uint64_t freq_timer_elapsed_ns(const freq_timer_handle_t* timer);
uint64_t freq_timer_elapsed_cycles(const freq_timer_handle_t* timer);

// Calibration control
int freq_timer_init(void);
int freq_timer_calibrate(void);
void freq_timer_cleanup(void);

// Batch timing for minimal overhead
typedef struct freq_timer_batch {
    uint64_t* timestamps;
    size_t capacity;
    size_t count;
} freq_timer_batch_t;

int freq_timer_batch_init(freq_timer_batch_t* batch, size_t capacity);
int freq_timer_batch_capture(freq_timer_batch_t* batch);
void freq_timer_batch_cleanup(freq_timer_batch_t* batch);

#ifdef __cplusplus
}
#endif
```

### Optional C++ Wrapper (Header-Only)
```cpp
namespace freq_timer {
    // Thin C++ wrapper for convenience
    inline uint64_t now_ns() noexcept { return freq_timer_now_ns(); }
    inline uint64_t now_cycles() noexcept { return freq_timer_now_cycles(); }
    
    // RAII timer wrapper
    class timer {
        freq_timer_handle_t handle_;
    public:
        timer() noexcept { freq_timer_start(&handle_); }
        uint64_t elapsed_ns() const noexcept { return freq_timer_elapsed_ns(&handle_); }
        uint64_t elapsed_cycles() const noexcept { return freq_timer_elapsed_cycles(&handle_); }
    };
}
```

## Pending Decisions

### 1. CPU Cycle Calibration Strategy
**Primary Approach**: Periodic calibration of CPU cycle counter to wall time ratio

**Calibration Process**:
- Background thread samples CPU cycles (RDTSC) and OS reference time simultaneously
- Calculate cycles-per-nanosecond ratio using linear regression over multiple samples
- Compensate for CPU frequency scaling and thermal throttling
- **Lock-free calibration updates**:
  - Use double-buffering: calibration thread writes to inactive buffer
  - Atomic pointer swap or version counter to switch buffers
  - User thread reads from active buffer without synchronization
- **Hot path isolation**: OS timing APIs (syscalls) never called in `freq_timer_now_ns()`
  - Hot path uses only CPU cycle counter + pre-computed conversion factor
  - Background calibration thread handles all syscalls and heavy computation
- **Monotonic guarantee**: Never allow clock to go backwards during calibration updates
  - Use smooth adjustment over multiple samples instead of immediate jumps
  - Maintain global offset to ensure monotonic progression
  - Calibration thread validates new values before making them visible

**Calibration Triggers**:
- A) Library initialization (mandatory baseline calibration)
- B) Periodic background calibration (configurable interval, default 1 second)
- C) Detection of significant frequency changes (>1% deviation)
- D) User-requested recalibration

**Fallback Strategy**: 
- Use OS reference clock APIs when cycle counter unavailable or unreliable
- **Performance note**: Fallback mode will have higher latency (50-200ns) due to syscall overhead, exceeding 20ns target
- Fallback should be rare on modern Linux systems with stable TSC/timer counter

### 2. Error Handling
**Options:**
- A) Exception-based (std::chrono style)
- B) Error codes/optional returns
- C) Hybrid approach

**Recommendation**: Option C - noexcept fast path, exceptions for setup

### 3. Precision vs Portability Trade-off
**Options:**
- A) Maximum precision on each platform (different APIs)
- B) Common denominator approach (consistent but potentially slower)
- C) Configurable precision levels

**Recommendation**: Option A with compile-time feature detection

### 4. Rust FFI Implementation Details

**Core Rust Implementation with C Exports**:
```rust
// src/ffi.rs
use std::os::raw::c_int;

#[no_mangle]
pub extern "C" fn freq_timer_now_ns() -> u64 {
    crate::timer::now_nanoseconds()
}

#[no_mangle]
pub extern "C" fn freq_timer_init() -> c_int {
    match crate::calibration::initialize() {
        Ok(_) => 0,
        Err(_) => -1,
    }
}

// Calibration data structure
struct CalibrationData {
    cycles_to_ns_factor: f64,
    monotonic_offset: u64,
}

// Double-buffered calibration data
static mut CALIBRATION_BUFFERS: [CalibrationData; 2] = [...];
static mut ACTIVE_BUFFER_INDEX: usize = 0;

// Safe internal Rust API - called from user thread only
pub fn now_nanoseconds() -> u64 {
    // Single-reader optimized: no synchronization needed
    unsafe {
        let calibration = &CALIBRATION_BUFFERS[ACTIVE_BUFFER_INDEX];
        let cycles = read_cpu_cycles();
        (cycles as f64 * calibration.cycles_to_ns_factor) as u64 
            + calibration.monotonic_offset
    }
}

// Called from background calibration thread
fn update_calibration(new_data: CalibrationData) {
    unsafe {
        let inactive_index = 1 - ACTIVE_BUFFER_INDEX;
        CALIBRATION_BUFFERS[inactive_index] = new_data;
        // Memory barrier to ensure data is visible before index swap
        std::sync::atomic::fence(std::sync::atomic::Ordering::Release);
        ACTIVE_BUFFER_INDEX = inactive_index;
    }
}
```

**Build Integration**:
- `cbindgen` for automatic C header generation
- `capi` crate pattern for C library conventions
- Static and dynamic library targets
- Cross-compilation support built-in
- **User-friendly Makefile** for common development tasks

## Implementation Phases

### Phase 1: Rust Core Infrastructure
- Cargo project setup with FFI configuration
- Architecture detection using Rust `cfg` attributes (x86_64 vs aarch64)
- Basic timing API wrappers for Linux
- `cbindgen` configuration for C header generation

### Phase 2: High-Performance Implementation
- Lock-free timer implementations using Rust atomics
- CPU cycle counter integration with inline assembly
- Calibration system with background thread
- Criterion.rs benchmark suite

### Phase 3: C FFI Layer
- Complete C interface implementation
- Error handling and safety guarantees
- C integration tests
- CMake wrapper for easy integration

### Phase 4: Testing & Validation
- Rust unit tests and integration tests
- C interface validation tests
- Cross-platform performance benchmarks
- Accuracy validation against system clocks

## Build System and User Interface

### Makefile Integration
**Simple user interface for common tasks**:
```makefile
# Makefile wrapping Cargo commands
.PHONY: test run bench clean install

# Run all tests (Rust + C FFI)
test:
	cargo test
	$(CC) tests/c_interface_test.c -L target/release -lfreq_timer -o test_c && ./test_c

# Run example programs
run:
	cargo run --example rust_usage
	$(CC) examples/basic_usage.c -L target/release -lfreq_timer -o example_c && ./example_c

# Run performance benchmarks
bench:
	cargo bench
	@echo "Benchmark results saved to target/criterion/reports/"

# Build release library with C headers
build:
	cargo build --release
	cbindgen --config cbindgen.toml --crate freq_timer --output include/freq_timer.h

# Clean build artifacts
clean:
	cargo clean
	rm -f test_c example_c

# Install library system-wide (optional)
install: build
	cp target/release/libfreq_timer.* /usr/local/lib/
	cp include/freq_timer.h /usr/local/include/
	ldconfig
```

### Development Workflow
**Typical user commands**:
- `make test` - Run all tests including C FFI validation
- `make bench` - Execute Criterion performance benchmarks
- `make run` - Build and run example programs
- `make build` - Create release library with C headers
- `make clean` - Clean all build artifacts

### Integration Benefits
- **Familiar interface**: Standard `make` commands for C/C++ developers
- **Cargo wrapping**: Leverages Rust toolchain while hiding complexity
- **Cross-compilation**: Makefile can wrap `cargo build --target` commands
- **CI/CD friendly**: Simple commands for automated testing and benchmarking

## Project Structure

### Rust Implementation with C FFI
```
freq_timer/
├── Makefile                  # User-friendly build interface
├── Cargo.toml                # Main Rust project config
├── cbindgen.toml             # C header generation config
├── CMakeLists.txt            # Optional CMake wrapper for integration
├── build.rs                  # Build script for C header generation
├── include/
│   └── freq_timer.h          # Auto-generated C header
├── src/
│   ├── lib.rs                # Main library entry point
│   ├── ffi.rs                # C FFI exports
│   ├── timer.rs              # Core timing implementation
│   ├── calibration.rs        # Calibration system
│   └── platform.rs           # Platform-specific code (x86_64/aarch64 Linux)
├── tests/
│   ├── integration_tests.rs  # Rust integration tests
│   ├── c_interface_test.c    # C FFI validation
│   └── accuracy_tests.rs     # Clock accuracy validation
├── benches/
│   └── performance.rs        # Criterion performance benchmarks
├── examples/
│   ├── basic_usage.c         # C usage example
│   ├── cpp_wrapper.cpp       # C++ wrapper example
│   └── rust_usage.rs         # Native Rust usage
└── scripts/
    ├── build_c_lib.sh        # Build script for C library
    └── benchmark_ci.sh       # CI benchmark runner
```