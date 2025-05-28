// =============================================================================
// Simplified Parquet Writer for Header-Only Library
// =============================================================================
//
// Provides the minimal FFI interface needed by parquet_writer_zerocopy.h:
// - parquet_stream_writer_init_with_options: Initialize writer with options
// - parquet_stream_writer_write_batch: Write a batch of data
// - parquet_stream_writer_close: Close and finalize the writer
//
// =============================================================================

use arrow::array::ffi_stream::{ArrowArrayStreamReader, FFI_ArrowArrayStream};
use arrow::error::ArrowError;
use parquet::arrow::ArrowWriter;
use parquet::basic::{Compression, BrotliLevel, GzipLevel, ZstdLevel};
use parquet::file::properties::{WriterProperties, EnabledStatistics};
use std::ffi::{CStr, c_void};
use std::fs::File;
use std::os::raw::{c_char, c_int};
use crate::tracing_init;

// =============================================================================
// FFI Types for Stream Writer Options
// =============================================================================

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum ParquetStreamCompression {
    Uncompressed = 0,
    Snappy = 1,
    Gzip = 2,
    Lzo = 3,
    Brotli = 4,
    Zstd = 5,
    Lz4 = 6,
    Lz4Raw = 7,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum ParquetStreamEncoding {
    Plain = 0,
    Dictionary = 1,
    Rle = 2,
    BitPacked = 3,
    DeltaBinaryPacked = 4,
    DeltaLengthByteArray = 5,
    DeltaByteArray = 6,
    RleDictionary = 7,
    ByteStreamSplit = 8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct CompressionLevels {
    pub gzip_level: i32,
    pub brotli_level: i32,
    pub zstd_level: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ParquetStreamWriterOptions {
    pub compression: ParquetStreamCompression,
    pub compression_levels: CompressionLevels,
    pub row_group_size: u32,
    pub enable_dictionary: bool,
    pub enable_statistics: bool,
    pub enable_bloom_filter: bool,
    pub max_row_group_size: u32,
    pub data_page_size: u32,
    pub dict_page_size: u32,
    pub enable_page_index: bool,
    pub enable_column_index: bool,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct ParquetStreamColumnDef {
    pub name: *const c_char,
    pub encoding: ParquetStreamEncoding,
    pub compression: ParquetStreamCompression,
    pub use_dictionary: bool,
    pub enable_bloom_filter: bool,
    pub enable_statistics: bool,
}

// =============================================================================
// Writer Handle
// =============================================================================

// Note: StreamWriterHandle is currently unused but kept for potential future use
#[allow(dead_code)]
pub struct StreamWriterHandle {
    writer: ArrowWriter<File>,
    _file_path: String,
}

pub struct StreamWriterState {
    file: Option<File>,
    // Note: path field kept for debugging/logging purposes
    #[allow(dead_code)]
    path: String,
    options: Option<ParquetStreamWriterOptions>,
    writer: Option<ArrowWriter<File>>,
}

// =============================================================================
// Helper Functions
// =============================================================================

fn convert_compression(compression: ParquetStreamCompression, levels: &CompressionLevels) -> Compression {
    match compression {
        ParquetStreamCompression::Uncompressed => Compression::UNCOMPRESSED,
        ParquetStreamCompression::Snappy => Compression::SNAPPY,
        ParquetStreamCompression::Gzip => {
            let level = if levels.gzip_level >= 1 && levels.gzip_level <= 9 {
                #[allow(clippy::cast_sign_loss)] // Safe: already checked range 1-9
                let level = levels.gzip_level as u32;
                level
            } else {
                6 // default
            };
            Compression::GZIP(GzipLevel::try_new(level).unwrap_or_default())
        },
        ParquetStreamCompression::Lzo => Compression::LZO,
        ParquetStreamCompression::Brotli => {
            let level = if levels.brotli_level >= 1 && levels.brotli_level <= 11 {
                #[allow(clippy::cast_sign_loss)] // Safe: already checked range 1-11
                let level = levels.brotli_level as u32;
                level
            } else {
                1 // default
            };
            Compression::BROTLI(BrotliLevel::try_new(level).unwrap_or_default())
        },
        ParquetStreamCompression::Zstd => {
            let level = if levels.zstd_level >= 1 && levels.zstd_level <= 22 {
                levels.zstd_level
            } else {
                3 // default
            };
            Compression::ZSTD(ZstdLevel::try_new(level).unwrap_or_default())
        },
        ParquetStreamCompression::Lz4 => Compression::LZ4,
        ParquetStreamCompression::Lz4Raw => Compression::LZ4_RAW,
    }
}

/// Helper to read all batches from a stream
unsafe fn read_all_batches_from_stream(
    stream_ptr: *mut FFI_ArrowArrayStream,
) -> Result<Vec<arrow::record_batch::RecordBatch>, ArrowError> {
    let stream = FFI_ArrowArrayStream::from_raw(stream_ptr);
    let mut reader = ArrowArrayStreamReader::try_new(stream)?;
    let mut batches = Vec::new();
    
    while let Some(batch) = reader.next().transpose()? {
        batches.push(batch);
    }
    
    Ok(batches)
}

// =============================================================================
// FFI Functions
// =============================================================================

/// Initialize a Parquet stream writer with advanced options
/// 
/// # Safety
/// This function is unsafe because it:
/// - Dereferences raw pointers (`path`, `options`, `_columns`)
/// - Assumes `path` points to a valid null-terminated C string
/// - Assumes `options` points to a valid `ParquetStreamWriterOptions` struct if not null
/// - Returns a raw pointer that must be properly managed by the caller
/// 
/// The caller must ensure:
/// - `path` is a valid pointer to a null-terminated UTF-8 string
/// - `options` is either null or points to a valid `ParquetStreamWriterOptions`
/// - The returned pointer is passed to `parquet_stream_writer_close` to avoid memory leaks
#[no_mangle]
pub unsafe extern "C" fn parquet_stream_writer_init_with_options(
    _stream: *mut FFI_ArrowArrayStream,
    path: *const c_char,
    options: *const ParquetStreamWriterOptions,
    _columns: *const ParquetStreamColumnDef,
    _num_columns: usize,
) -> *mut c_void {
    // Initialize tracing for debugging (safe to call multiple times)
    tracing_init::init();
    
    if path.is_null() {
        eprintln!("Error: Null pointer passed to parquet_stream_writer_init_with_options");
        return std::ptr::null_mut();
    }

    // Convert path to Rust string
    let path_cstr = match CStr::from_ptr(path).to_str() {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Error: Invalid UTF-8 in path: {e}");
            return std::ptr::null_mut();
        }
    };

    // Create output file
    let file = match File::create(path_cstr) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("Error: Failed to create file '{path_cstr}': {e}");
            return std::ptr::null_mut();
        }
    };

    // Store the file and options for later use when we get the first batch
    let handle = Box::new(StreamWriterState {
        file: Some(file),
        path: path_cstr.to_string(),
        options: if options.is_null() { None } else { Some(*options) },
        writer: None,
    });

    Box::into_raw(handle).cast::<c_void>()
}

/// Write a batch of data to the Parquet writer
/// 
/// # Safety
/// This function is unsafe because it:
/// - Dereferences raw pointers (`handle`, `stream`)
/// - Assumes `handle` points to a valid `StreamWriterState` created by `parquet_stream_writer_init_with_options`
/// - Assumes `stream` points to a valid Arrow array stream
/// - Modifies the state through a mutable raw pointer
/// 
/// The caller must ensure:
/// - `handle` was returned by `parquet_stream_writer_init_with_options` and not yet closed
/// - `stream` points to a valid Arrow array stream with compatible schema
/// - This function is not called concurrently on the same handle
/// 
/// # Panics
/// This function may panic if:
/// - The writer state is corrupted (file was already taken)
/// - Arrow writer creation fails unexpectedly
#[no_mangle]
pub unsafe extern "C" fn parquet_stream_writer_write_batch(
    handle: *mut c_void,
    stream: *mut FFI_ArrowArrayStream,
) -> c_int {
    if handle.is_null() || stream.is_null() {
        eprintln!("Error: Null pointer passed to parquet_stream_writer_write_batch");
        return -1;
    }

    let state = &mut *handle.cast::<StreamWriterState>();

    // Read all batches from the stream
    match read_all_batches_from_stream(stream) {
        Ok(batches) => {
            if batches.is_empty() {
                return 0; // No data to write
            }

            // Initialize writer on first batch if not already done
            if state.writer.is_none() {
                let file = state.file.take().unwrap();
                let schema = batches[0].schema();

                // Build writer properties
                let mut props_builder = WriterProperties::builder();
                
                if let Some(opts) = &state.options {
                    let compression = convert_compression(opts.compression, &opts.compression_levels);
                    props_builder = props_builder
                        .set_compression(compression)
                        .set_dictionary_enabled(opts.enable_dictionary)
                        .set_statistics_enabled(if opts.enable_statistics { 
                            EnabledStatistics::Chunk 
                        } else { 
                            EnabledStatistics::None 
                        });
                        
                    if opts.row_group_size > 0 {
                        props_builder = props_builder.set_max_row_group_size(opts.row_group_size as usize);
                    }
                }

                let props = props_builder.build();

                // Create Arrow writer
                let writer = match ArrowWriter::try_new(file, schema, Some(props)) {
                    Ok(w) => w,
                    Err(e) => {
                        eprintln!("Error: Failed to create Arrow writer: {e}");
                        return -3;
                    }
                };

                state.writer = Some(writer);
            }

            // Write all batches
            for batch in batches {
                if let Err(e) = state.writer.as_mut().unwrap().write(&batch) {
                    eprintln!("Error: Failed to write batch: {e}");
                    return -4;
                }
            }
        }
        Err(e) => {
            eprintln!("Error: Failed to read batch: {e}");
            return -2;
        }
    }

    0 // Success
}

/// Close and finalize the Parquet writer
/// 
/// # Safety
/// This function is unsafe because it:
/// - Dereferences a raw pointer (`handle`)
/// - Takes ownership of the memory pointed to by `handle`
/// - Assumes `handle` points to a valid `StreamWriterState`
/// 
/// The caller must ensure:
/// - `handle` was returned by `parquet_stream_writer_init_with_options`
/// - `handle` has not been previously closed or freed
/// - `handle` is not used after this function returns
#[no_mangle]
pub unsafe extern "C" fn parquet_stream_writer_close(handle: *mut c_void) -> c_int {
    if handle.is_null() {
        eprintln!("Error: Null pointer passed to parquet_stream_writer_close");
        return -1;
    }

    let state = Box::from_raw(handle.cast::<StreamWriterState>());
    
    if let Some(writer) = state.writer {
        if let Err(e) = writer.close() {
            eprintln!("Error: Failed to close writer: {e}");
            return -2;
        }
    }

    0 // Success
} 