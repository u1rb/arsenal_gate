// src/lib.rs

use chrono::{DateTime, NaiveDateTime, TimeZone};
use chrono_tz::Tz;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

/// Extracts a specific part of the date/time from an epoch timestamp considering the timezone.
///
/// # Arguments
///
/// * `part_name` - The part of the date/time to extract (e.g., "year", "month", "nanosecond").
/// * `timezone` - The IANA timezone identifier (e.g., "America/New_York").
/// * `epoch_ts` - The epoch timestamp in nanoseconds.
///
/// # Returns
///
/// * `Ok(u64)` containing the extracted part if successful.
/// * `Err(String)` containing an error message if something goes wrong.
pub fn get_date_part(part_name: &str, timezone: &str, epoch_ts: u64) -> Result<u64, String> {
    // Convert nanoseconds to seconds and nanoseconds
    let secs = (epoch_ts / 1_000_000_000) as i64;
    let nanos = (epoch_ts % 1_000_000_000) as u32;

    // Create NaiveDateTime from epoch
    let naive_dt = NaiveDateTime::from_timestamp_opt(secs, nanos)
        .ok_or_else(|| "Invalid timestamp".to_string())?;

    // Parse timezone
    let tz: Tz = timezone.parse().map_err(|_| "Invalid timezone".to_string())?;

    // Create DateTime in specified timezone
    let datetime: DateTime<Tz> = tz.from_utc_datetime(&naive_dt);

    // Match the part_name and extract accordingly
    match part_name.to_lowercase().as_str() {
        "year" => Ok(datetime.year() as u64),
        "month" => Ok(datetime.month() as u64),
        "day" => Ok(datetime.day() as u64),
        "hour" => Ok(datetime.hour() as u64),
        "minute" => Ok(datetime.minute() as u64),
        "second" => Ok(datetime.second() as u64),
        "nanosecond" => Ok(datetime.nanosecond() as u64),
        _ => Err("Unsupported part_name".to_string()),
    }
}

/// C-compatible FFI function to extract a date part.
///
/// # Safety
///
/// This function is unsafe because it dereferences raw pointers provided by the caller.
/// The caller must ensure that `part_name` and `timezone` are valid null-terminated C strings,
/// and that `output` is a valid pointer to a `uint64_t`.
#[no_mangle]
pub extern "C" fn rust_get_date_part(
    part_name: *const c_char,
    timezone: *const c_char,
    ts: u64,
    output: *mut u64,
) -> i32 {
    // Safety checks and conversions
    unsafe {
        if part_name.is_null() || timezone.is_null() || output.is_null() {
            return 1; // Error code 1 for null pointer
        }

        // Convert C strings to Rust strings
        let part_cstr = CStr::from_ptr(part_name);
        let timezone_cstr = CStr::from_ptr(timezone);

        let part_str = match part_cstr.to_str() {
            Ok(s) => s,
            Err(_) => return 2, // Error code 2 for invalid UTF-8
        };

        let timezone_str = match timezone_cstr.to_str() {
            Ok(s) => s,
            Err(_) => return 3, // Error code 3 for invalid UTF-8
        };

        // Call the Rust function
        match get_date_part(part_str, timezone_str, ts) {
            Ok(value) => {
                *output = value;
                0 // Success
            }
            Err(_) => 4, // Error code 4 for processing error
        }
    }
}