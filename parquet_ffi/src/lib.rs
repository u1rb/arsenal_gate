// =============================================================================
// Parquet FFI - Simplified Backend for Header-Only Libraries
// =============================================================================
//
// This library provides the minimal Rust backend needed to support:
// - parquet_reader_stream.h (zero-copy reader with merger)
// - parquet_writer_zerocopy.h (zero-copy writer)
//
// =============================================================================

pub mod reader;
pub mod writer;
pub mod tracing_init;