pub mod platform;
pub mod timer;
pub mod calibration;
pub mod ffi;

use std::sync::atomic::{AtomicBool, Ordering};

static INITIALIZED: AtomicBool = AtomicBool::new(false);

pub fn initialize() -> Result<(), &'static str> {
    if INITIALIZED.swap(true, Ordering::AcqRel) {
        return Ok(()); // Already initialized
    }

    // Initialize calibration system
    calibration::initialize()?;
    
    Ok(())
}

pub fn cleanup() {
    if !INITIALIZED.swap(false, Ordering::AcqRel) {
        return; // Not initialized
    }

    calibration::cleanup();
}