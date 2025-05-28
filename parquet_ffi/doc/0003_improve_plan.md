@cxx_examples/run.sh This is the entrypoint of a project. It calls parquet_ffi, which is a Rust ffi project. We want
   to improve the read parquet speed, give me some suggestions and plans


● Based on my analysis of your parquet FFI project, here are key performance improvement suggestions:

  Major Bottlenecks Found:

  1. Individual malloc calls for each column - use arena allocation instead
  2. Deep copying of primitive arrays - implement zero-copy views
  3. Fixed 8192 batch size - add adaptive sizing based on memory targets
  4. Sequential column processing - parallelize with Rayon

  High-Impact Optimizations:

  Memory Management:
  - Replace individual mallocs with single arena allocation per batch
  - Implement zero-copy views for primitive types
  - Add memory pooling for repeated allocations

  Batch Processing:
  - Adaptive batch sizing based on estimated row size and memory budget
  - Column projection support to skip unused columns
  - Parallel column processing during Arrow→C conversion

  Arrow Interface:
  - Add prefetching/buffering for stream readers
  - Enable page indexes in ParquetRecordBatchReaderBuilder
  - Support row filtering pushdown

  Expected Improvements:

  - 2-3x faster memory allocation
  - 1.5-2x better throughput for primitives
  - 3-5x improvement for string data
  - Linear scaling with CPU cores

  The arena allocation and zero-copy views would provide the biggest wins with minimal code changes to
  src/ffi/reader.rs:parquet_reader_read_next_batch.