# 0002 Prompt: Read Parquet via Rust + C/C++ FFI

## 1. Overview
Provide C and C++ APIs backed by a Rust implementation for reading Parquet files into Arrow RecordBatches and streaming them via the C ABI.

## 2. Requirements

### 2.1 Rust Implementation
- Open and read Parquet files using the `arrow` and `parquet` crates.
- Convert each `RecordBatch` into an `ArrowArrayStream` via `arrow-ffi`.
- Expose C ABI:
  ```c
  /// Exports the contents of a Parquet file as an Arrow C stream.
  /// @param file_path null-terminated UTF-8 path to the .parquet file, support s3://, gs://, file://, etc.
  /// @param out_stream pointer to ArrowArrayStream struct to initialize.
  /// @return 0 on success, non-zero on error (use get_last_error to retrieve message).
  ParquetFfiError export_parquet_file_to_stream(const char *file_path,
                                    struct ArrowArrayStream *out_stream);
  ```
- Provide complementary error query:
  ```c
  const char *get_last_error(void);
  ```
- Support predicate pushdown and row filtering: allow specifying an optional filter expression or configuration when creating the ArrowArrayStream, leveraging Parquet bloom filters and metadata to skip non-matching row groups and rows at the Rust level.

### 2.2 C API
- Declare functions in `include/parquet_ffi.h` and `include/parquet_stream.h` with `extern "C"` linkage.
- Include `<arrow_c.h>` for `ArrowArrayStream` and `ArrowSchema` types.
- Document ownership: caller must invoke `release` on the stream when done.
- Support passing filter options or a serialized predicate to the C API when initializing the stream, enabling row-level filtering and bloom filter pruning before data is streamed.

### 2.3 C++ Wrapper
- Do not link against or depend on the Arrow C++ library; the wrapper must use only the Arrow C interface (`arrow_c.h`).
- Implement an RAII class `ParquetStream` in `include/parquet_stream.h`.
- Constructor calls `export_parquet_file_to_stream`.
- Destructor invokes `.release()` on the underlying `ArrowArrayStream`.
- Method `.next_batch()` returns `std::shared_ptr<arrow::RecordBatch>` (or raw pointer).

### 2.4 Remote & Streaming Support
- Support S3 and GCS URIs (`s3://`, `gs://`) via the `object_store` crate or AWS/GCS SDKs.
- Provide an `AsyncRead + Seek` adapter for remote files with HTTP range requests per row group.

### 2.5 Flow Control & Backpressure
- Allow clients to configure batch size and consumption rate.
- Ensure proper backpressure when downstream processing is slower than I/O.

## 3. Technical Specifications

### 3.1 Streaming & Memory Footprint
- Read one record batch or row group at a time for files larger than memory.
- Avoid loading the entire file into memory.

### 3.2 Zero-Copy & `next_batch` Semantics
- Lazily fetch the next batch; minimize allocations, especially for string/binary columns.
- Reuse Arrow buffers (data, validity, offset) whenever possible.

### 3.3 Buffer Ownership
- Rust side allocates and owns buffers via `arrow-ffi`.
- C/C++ consumers rely on `release` callbacks for deallocation.

### 3.4 Performance Targets
- Achieve multi-GB/s throughput on modern hardware.
- Memory usage bounded by the configured batch size.

### 3.5 Parallelism (Optional)
- Design for thread-safe or parallel row-group reading without inflating memory footprint.

## 4. C API Details

### 4.1 Core Functions
- `int export_parquet_file_to_stream(const char *file_path, struct ArrowArrayStream *out_stream);`
- `const char *get_last_error(void);`

### 4.2 Row Stream API
- `bool parquet_stream_get_next(PacketStream *stream);` advances to the next row.
- Row-level getters (e.g., `packet_stream_get_int32`, `packet_stream_get_string`).

### 4.3 Lookahead
- `bool parquet_stream_peek(PacketStream *stream);` inspects the next row without consuming.

## 5. Build & Integration

### 5.1 Cargo & Rust
- Core library in `src/`, managed by `Cargo.toml`.
- Use `cargo fmt`, `cargo clippy`, and `cargo build --release`.

### 5.2 CMake & Corrosion
- Top-level `CMakeLists.txt` imports the Rust crate via Corrosion as `parquet_ffi`.
- Install C headers into `include/` and link the Rust library.
- Build C++ examples in `cxx_examples/`.

### 5.3 Project Layout
```
/                # Workspace root
├── include/     # C/C++ headers
├── src/         # Rust implementation
├── tests/       # C integration tests
├── cxx_examples/# C++ example programs
├── Cargo.toml   # Rust manifest
└── CMakeLists.txt
```

### 5.4 Automation Script
- `scripts/run.sh`: format, lint, build, generate build directory, run C/C++ examples, execute tests.
- Cross-platform CPU detection (`nproc` vs `sysctl -n hw.ncpu`).

## 6. Testing & Examples

### 6.1 C Integration Test
- `tests/read_test.c`: validate schema, column names, iterate batches, check row counts.

### 6.2 C++ Example
- `cxx_examples/read_example.c`: demonstrate usage of the `ParquetStream` wrapper.

## 7. Deliverables
- `src/read_lib.rs` and new Rust modules.
- Modifications to `include/parquet_ffi.h` and `include/parquet_stream.h`.
- Updates to `CMakeLists.txt` for Corrosion and linking.
- `tests/read_test.c` and `cxx_examples/read_example.c`.

---

*Ensure adherence to existing code style, graceful error handling, and no memory leaks. After implementation, run `cargo fmt`, `cargo clippy`, and verify all examples and tests build and pass`.*