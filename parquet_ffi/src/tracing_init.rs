use std::sync::Once;
use tracing_subscriber::{fmt, EnvFilter};
use std::os::raw::c_int;

static INIT: Once = Once::new();

/// Initialize tracing with default configuration
pub fn init() {
    INIT.call_once(|| {
        // Set the default environment filter to INFO level
        // This can be overridden with the RUST_LOG environment variable
        let env_filter = EnvFilter::try_from_default_env()
            .unwrap_or_else(|_| EnvFilter::new("info"));

        // Initialize the tracing subscriber
        fmt::fmt()
            .with_env_filter(env_filter)
            .with_target(true)
            .with_ansi(false) // Disable ANSI color codes for better compatibility with C logging
            .init();

        tracing::info!("Tracing initialized for parquet_ffi");
    });
}

/// Initialize tracing from C/C++ code
/// 
/// This function provides an explicit way to initialize Rust tracing/logging
/// from C/C++ applications using the FFI interface.
/// 
/// # Returns
/// * 0 on success
/// * Non-zero on failure (currently always returns 0)
/// 
/// # Safety
/// This function is safe to call from C/C++ code. It uses internal synchronization
/// to ensure tracing is only initialized once, even if called multiple times.
/// 
/// # Example C usage
/// ```c
/// #include <parquet_stream.h>
/// 
/// int main() {
///     // Initialize Rust tracing for debugging
///     parquet_ffi_init_tracing();
///     
///     // Now use other parquet_ffi functions...
///     return 0;
/// }
/// ```
#[no_mangle]
pub extern "C" fn parquet_ffi_init_tracing() -> c_int {
    init();
    0 // Success
} 