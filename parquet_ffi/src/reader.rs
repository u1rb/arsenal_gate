// =============================================================================
// Simplified Parquet Reader for Header-Only Library
// =============================================================================
//
// Provides the minimal FFI interface needed by parquet_reader_stream.h:
// - export_parquet_file_to_stream: Exports a Parquet file as Arrow stream
//
// =============================================================================

use arrow::ffi_stream::FFI_ArrowArrayStream;
use parquet::arrow::arrow_reader::ParquetRecordBatchReaderBuilder;
use std::ffi::CStr;
use std::fs::File;
use std::os::raw::c_char;
use crate::tracing_init;

/// Export a Parquet file to an Arrow C stream interface
/// 
/// This function opens a Parquet file and creates an Arrow stream that can be
/// consumed by the C header-only library.
///
/// # Arguments
/// * `path` - Path to the Parquet file (null-terminated C string)
/// * `out_stream` - Pointer to `ArrowArrayStream` to populate
///
/// # Returns
/// * 0 on success
/// * Non-zero error code on failure
/// 
/// # Safety
/// This function is unsafe because it:
/// - Dereferences raw pointers (`path`, `out_stream`)
/// - Assumes `path` points to a valid null-terminated C string
/// - Writes to the memory location pointed to by `out_stream`
/// - Creates an Arrow stream that must be properly released by the caller
/// 
/// The caller must ensure:
/// - `path` is a valid pointer to a null-terminated UTF-8 string
/// - `out_stream` points to valid memory that can hold an `FFI_ArrowArrayStream`
/// - The resulting stream is properly released using Arrow's release mechanism
#[no_mangle]
pub unsafe extern "C" fn export_parquet_file_to_stream(
    path: *const c_char,
    out_stream: *mut FFI_ArrowArrayStream,
) -> i32 {
    // Initialize tracing for debugging (safe to call multiple times)
    tracing_init::init();
    
    if path.is_null() || out_stream.is_null() {
        eprintln!("Error: Null pointer passed to export_parquet_file_to_stream");
        return -1;
    }

    // Convert C string to Rust string
    let path_cstr = match CStr::from_ptr(path).to_str() {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Error: Invalid UTF-8 in path: {e}");
            return -2;
        }
    };

    // Open the Parquet file
    let file = match File::open(path_cstr) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("Error: Failed to open file '{path_cstr}': {e}");
            return -3;
        }
    };

    // Create Parquet reader
    let builder = match ParquetRecordBatchReaderBuilder::try_new(file) {
        Ok(b) => b,
        Err(e) => {
            eprintln!("Error: Failed to create Parquet reader for '{path_cstr}': {e}");
            return -4;
        }
    };

    // Build the record batch reader
    let reader = match builder.build() {
        Ok(r) => r,
        Err(e) => {
            eprintln!("Error: Failed to build Parquet reader for '{path_cstr}': {e}");
            return -5;
        }
    };

    // Convert to Arrow stream
    let stream = Box::new(reader);
    let ffi_stream = FFI_ArrowArrayStream::new(stream);

    // Move the stream to the output pointer
    std::ptr::write(out_stream, ffi_stream);

    0 // Success
} 