use std::os::raw::c_int;
use std::ptr;

use crate::{timer, calibration};

// Core timing functions

#[no_mangle]
pub extern "C" fn freq_timer_now_ns() -> u64 {
    timer::now_nanoseconds()
}

#[no_mangle]
pub extern "C" fn freq_timer_now_cycles() -> u64 {
    timer::now_cycles()
}

#[no_mangle]
pub extern "C" fn freq_timer_cycles_per_ns() -> f64 {
    calibration::get_cycles_per_ns()
}

// Timer handle for elapsed time measurement

#[repr(C)]
pub struct freq_timer_handle {
    start_cycles: u64,
}

/// Start a timer by recording the current CPU cycle count.
///
/// # Safety
/// 
/// The caller must ensure that `timer` points to a valid `freq_timer_handle` structure.
#[no_mangle]
pub unsafe extern "C" fn freq_timer_start(timer: *mut freq_timer_handle) -> c_int {
    if timer.is_null() {
        return -1;
    }
    
    (*timer).start_cycles = timer::now_cycles();
    
    0
}

/// Get elapsed time in nanoseconds since timer was started.
///
/// # Safety
/// 
/// The caller must ensure that `timer` points to a valid, initialized `freq_timer_handle`.
#[no_mangle]
pub unsafe extern "C" fn freq_timer_elapsed_ns(timer: *const freq_timer_handle) -> u64 {
    if timer.is_null() {
        return 0;
    }
    
    let handle = timer::TimerHandle {
        start_cycles: (*timer).start_cycles,
    };
    timer::elapsed_nanoseconds(&handle)
}

/// Get elapsed CPU cycles since timer was started.
///
/// # Safety
/// 
/// The caller must ensure that `timer` points to a valid, initialized `freq_timer_handle`.
#[no_mangle]
pub unsafe extern "C" fn freq_timer_elapsed_cycles(timer: *const freq_timer_handle) -> u64 {
    if timer.is_null() {
        return 0;
    }
    
    let handle = timer::TimerHandle {
        start_cycles: (*timer).start_cycles,
    };
    timer::elapsed_cycles(&handle)
}

// Calibration control

#[no_mangle]
pub extern "C" fn freq_timer_init() -> c_int {
    match crate::initialize() {
        Ok(_) => 0,
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn freq_timer_calibrate() -> c_int {
    match calibration::calibrate() {
        Ok(_) => 0,
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn freq_timer_cleanup() {
    crate::cleanup();
}

// Batch timing for minimal overhead

#[repr(C)]
pub struct freq_timer_batch {
    timestamps: *mut u64,
    capacity: usize,
    count: usize,
}

/// Initialize a batch timer with the specified capacity.
///
/// # Safety
/// 
/// The caller must ensure that `batch` points to a valid `freq_timer_batch` structure
/// that will remain valid for the lifetime of the batch.
#[no_mangle]
pub unsafe extern "C" fn freq_timer_batch_init(batch: *mut freq_timer_batch, capacity: usize) -> c_int {
    if batch.is_null() || capacity == 0 {
        return -1;
    }
    
    // Allocate memory for timestamps
    let layout = std::alloc::Layout::array::<u64>(capacity).unwrap();
    let ptr = std::alloc::alloc(layout) as *mut u64;
    
    if ptr.is_null() {
        return -1;
    }
    
    (*batch).timestamps = ptr;
    (*batch).capacity = capacity;
    (*batch).count = 0;
    
    0
}

/// Capture a timestamp in the batch.
///
/// # Safety
/// 
/// The caller must ensure that `batch` points to a valid, initialized `freq_timer_batch`.
#[no_mangle]
pub unsafe extern "C" fn freq_timer_batch_capture(batch: *mut freq_timer_batch) -> c_int {
    if batch.is_null() {
        return -1;
    }
    
    if (*batch).count >= (*batch).capacity {
        return -1; // Batch is full
    }
    
    let timestamp = timer::now_nanoseconds();
    *(*batch).timestamps.add((*batch).count) = timestamp;
    (*batch).count += 1;
    
    0
}

/// Clean up and free memory allocated for a batch timer.
///
/// # Safety
/// 
/// The caller must ensure that `batch` points to a valid `freq_timer_batch` that was
/// previously initialized with `freq_timer_batch_init`.
#[no_mangle]
pub unsafe extern "C" fn freq_timer_batch_cleanup(batch: *mut freq_timer_batch) {
    if batch.is_null() {
        return;
    }
    
    if !(*batch).timestamps.is_null() && (*batch).capacity > 0 {
        let layout = std::alloc::Layout::array::<u64>((*batch).capacity).unwrap();
        std::alloc::dealloc((*batch).timestamps as *mut u8, layout);
        
        (*batch).timestamps = ptr::null_mut();
        (*batch).capacity = 0;
        (*batch).count = 0;
    }
}