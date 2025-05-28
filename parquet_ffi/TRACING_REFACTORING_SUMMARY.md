# Tracing Initialization Refactoring Summary

## Overview

Successfully refactored the tracing initialization from automatic static initialization (using `#[ctor::ctor]`) to explicit function-based initialization. This provides better control over when tracing is initialized and is more suitable for FFI usage.

## Changes Made

### **1. Removed Automatic Initialization**

**Before**: Tracing was automatically initialized at static time using the `ctor` crate
```rust
#[ctor::ctor]
fn init_tracing() {
    tracing_init::init();
}
```

**After**: No automatic initialization - tracing is initialized explicitly when needed

### **2. Added Explicit FFI Function**

**New Function**: `parquet_ffi_init_tracing()`
```rust
#[no_mangle]
pub extern "C" fn parquet_ffi_init_tracing() -> c_int {
    init();
    0 // Success
}
```

**Features**:
- Safe to call from C/C++ code
- Thread-safe (uses `std::sync::Once` internally)
- Safe to call multiple times (initialization only happens once)
- Returns 0 on success

### **3. Updated Header File**

Added function declaration to `include/parquet_stream.h`:
```c
/**
 * Initialize Rust tracing/logging system for debugging
 * 
 * This function initializes the Rust tracing system which enables debug output
 * from the Rust backend. It's safe to call multiple times - initialization
 * will only happen once.
 * 
 * Call this function early in your application if you want to see debug output
 * from the parquet_ffi library. The logging level can be controlled with the
 * RUST_LOG environment variable (e.g., RUST_LOG=debug).
 * 
 * @return 0 on success, non-zero on error (currently always returns 0)
 */
int parquet_ffi_init_tracing(void);
```

### **4. Backward Compatibility**

**Auto-initialization**: Added automatic tracing initialization to main FFI functions:
- `export_parquet_file_to_stream()` in reader.rs
- `parquet_stream_writer_init_with_options()` in writer.rs

This ensures existing code continues to work without changes while providing the option for explicit control.

### **5. Removed Dependencies**

**Removed**: `ctor = "0.2"` dependency from Cargo.toml since it's no longer needed.

## Usage Options

### **Option 1: Explicit Initialization (Recommended)**
```c
#include "parquet_stream.h"

int main() {
    // Initialize tracing early for debugging
    parquet_ffi_init_tracing();
    
    // Now use other parquet functions...
    export_parquet_file_to_stream(...);
    return 0;
}
```

### **Option 2: Automatic Initialization (Backward Compatible)**
```c
#include "parquet_stream.h"

int main() {
    // Tracing will be auto-initialized on first FFI call
    export_parquet_file_to_stream(...);  // Tracing initialized here
    return 0;
}
```

## Environment Variables

Control logging level with `RUST_LOG`:
- `RUST_LOG=error` - Only error messages
- `RUST_LOG=warn` - Warning and error messages  
- `RUST_LOG=info` - Info, warning, and error messages (default)
- `RUST_LOG=debug` - Debug and all above messages
- `RUST_LOG=trace` - All messages including trace level

## Example Programs

### **New Example**: `tracing_example.c`
Demonstrates explicit tracing initialization:
```bash
cd build
./tracing_example                    # Info level
RUST_LOG=debug ./tracing_example     # Debug level
```

## Benefits

1. **Better Control**: Applications can choose when to initialize tracing
2. **FFI Friendly**: No automatic static initialization that might interfere with C/C++ applications
3. **Backward Compatible**: Existing code continues to work unchanged
4. **Thread Safe**: Safe to call from multiple threads
5. **Idempotent**: Safe to call multiple times
6. **Reduced Dependencies**: No longer depends on the `ctor` crate

## Verification Results

### ✅ **Build Test**: PASSED
```bash
cargo build --release
# Clean compilation
```

### ✅ **Integration Test**: PASSED
```bash
bash cxx_examples/run.sh --cell=build,write,read
# All functionality works correctly
# Tracing messages appear when functions are called
```

### ✅ **Explicit Initialization Test**: PASSED
```bash
./tracing_example
# ✅ Tracing initialized successfully
```

## Migration Guide

**For existing applications**: No changes required - tracing will auto-initialize.

**For new applications**: Consider calling `parquet_ffi_init_tracing()` early in your application for better control over when debugging output starts.

**For debugging**: Set `RUST_LOG=debug` environment variable to see detailed debug output from the Rust backend.

## Conclusion

The tracing refactoring successfully provides explicit control over tracing initialization while maintaining full backward compatibility. This makes the library more suitable for FFI usage and gives C/C++ applications better control over when Rust debugging output is enabled. 