use crate::platform::{read_cpu_cycles, get_os_time_ns, detect_cpu_info};
use std::sync::atomic::{AtomicUsize, AtomicU64, AtomicBool, Ordering};
use std::thread::{self, JoinHandle};
use std::time::Duration;

// Calibration data structure
#[repr(C, align(64))] // Cache line aligned
#[derive(Copy, Clone)]
struct CalibrationData {
    cycles_to_ns_factor: f64,
    monotonic_offset: u64,
    base_cycles: u64,
    base_ns: u64,
}

// Double-buffered calibration data
static mut CALIBRATION_BUFFERS: [CalibrationData; 2] = [
    CalibrationData {
        cycles_to_ns_factor: 0.5, // Initial guess: 2GHz
        monotonic_offset: 0,
        base_cycles: 0,
        base_ns: 0,
    },
    CalibrationData {
        cycles_to_ns_factor: 0.5,
        monotonic_offset: 0,
        base_cycles: 0,
        base_ns: 0,
    },
];

// Index of the active buffer (0 or 1)
static ACTIVE_BUFFER_INDEX: AtomicUsize = AtomicUsize::new(0);

// Calibration thread control
static CALIBRATION_RUNNING: AtomicBool = AtomicBool::new(false);
static mut CALIBRATION_THREAD: Option<JoinHandle<()>> = None;

// Last known good timestamp for monotonicity
static LAST_TIMESTAMP_NS: AtomicU64 = AtomicU64::new(0);

// Initialize calibration system
pub fn initialize() -> Result<(), &'static str> {
    // Detect CPU information
    let cpu_info = detect_cpu_info();
    
    if !cpu_info.has_constant_tsc {
        eprintln!("Warning: CPU does not support constant TSC, timing may be inaccurate");
    }

    // Perform initial calibration
    perform_initial_calibration()?;
    
    // Start background calibration thread
    start_calibration_thread();
    
    Ok(())
}

// Cleanup calibration system
pub fn cleanup() {
    // Stop calibration thread
    CALIBRATION_RUNNING.store(false, Ordering::Release);
    
    #[allow(static_mut_refs)]
    unsafe {
        if let Some(thread) = CALIBRATION_THREAD.take() {
            let _ = thread.join();
        }
    }
}

// Convert cycles to nanoseconds using current calibration
#[inline(always)]
pub fn cycles_to_nanoseconds(cycles: u64) -> u64 {
    unsafe {
        let index = ACTIVE_BUFFER_INDEX.load(Ordering::Acquire);
        let calibration = &CALIBRATION_BUFFERS[index];
        
        // Calculate nanoseconds
        let ns = ((cycles - calibration.base_cycles) as f64 * calibration.cycles_to_ns_factor) as u64
            + calibration.base_ns
            + calibration.monotonic_offset;
        
        // Ensure monotonicity
        let mut last_ns = LAST_TIMESTAMP_NS.load(Ordering::Acquire);
        loop {
            if ns <= last_ns {
                return last_ns + 1;
            }
            match LAST_TIMESTAMP_NS.compare_exchange_weak(
                last_ns,
                ns,
                Ordering::Release,
                Ordering::Acquire,
            ) {
                Ok(_) => return ns,
                Err(current) => last_ns = current,
            }
        }
    }
}

// Convert cycle delta to nanoseconds delta
#[inline(always)]
pub fn cycles_to_nanoseconds_delta(cycles_delta: u64) -> u64 {
    unsafe {
        let index = ACTIVE_BUFFER_INDEX.load(Ordering::Acquire);
        let calibration = &CALIBRATION_BUFFERS[index];
        (cycles_delta as f64 * calibration.cycles_to_ns_factor) as u64
    }
}

// Get current cycles per nanosecond ratio
pub fn get_cycles_per_ns() -> f64 {
    unsafe {
        let index = ACTIVE_BUFFER_INDEX.load(Ordering::Acquire);
        1.0 / CALIBRATION_BUFFERS[index].cycles_to_ns_factor
    }
}

// Perform initial calibration
fn perform_initial_calibration() -> Result<(), &'static str> {
    const NUM_SAMPLES: usize = 10;
    const SAMPLE_DELAY_MS: u64 = 1;
    
    let mut samples = Vec::with_capacity(NUM_SAMPLES);
    
    // Collect samples
    for _ in 0..NUM_SAMPLES {
        let cycles = read_cpu_cycles();
        let ns = get_os_time_ns();
        samples.push((cycles, ns));
        thread::sleep(Duration::from_millis(SAMPLE_DELAY_MS));
    }
    
    // Calculate cycles-to-ns factor using linear regression
    let (factor, base_cycles, base_ns) = calculate_calibration_factor(&samples)?;
    
    // Update initial calibration data
    unsafe {
        CALIBRATION_BUFFERS[0] = CalibrationData {
            cycles_to_ns_factor: factor,
            monotonic_offset: 0,
            base_cycles,
            base_ns,
        };
        CALIBRATION_BUFFERS[1] = CALIBRATION_BUFFERS[0];
    }
    
    Ok(())
}

// Calculate calibration factor from samples using linear regression
fn calculate_calibration_factor(samples: &[(u64, u64)]) -> Result<(f64, u64, u64), &'static str> {
    if samples.len() < 2 {
        return Err("Not enough samples for calibration");
    }
    
    // Use first sample as base
    let (base_cycles, base_ns) = samples[0];
    
    // Calculate average rate
    let mut sum_rate = 0.0;
    let mut count = 0;
    
    for sample in samples.iter().skip(1) {
        let cycles_delta = (sample.0 - base_cycles) as f64;
        let ns_delta = (sample.1 - base_ns) as f64;
        
        if cycles_delta > 0.0 && ns_delta > 0.0 {
            sum_rate += ns_delta / cycles_delta;
            count += 1;
        }
    }
    
    if count == 0 {
        return Err("No valid samples for calibration");
    }
    
    let avg_factor = sum_rate / count as f64;
    
    Ok((avg_factor, base_cycles, base_ns))
}

// Start background calibration thread
fn start_calibration_thread() {
    CALIBRATION_RUNNING.store(true, Ordering::Release);
    
    let handle = thread::spawn(|| {
        calibration_thread_main();
    });
    
    unsafe {
        CALIBRATION_THREAD = Some(handle);
    }
}

// Main calibration thread function
fn calibration_thread_main() {
    const CALIBRATION_INTERVAL_MS: u64 = 1000; // 1 second
    const NUM_SAMPLES: usize = 20;
    
    while CALIBRATION_RUNNING.load(Ordering::Acquire) {
        thread::sleep(Duration::from_millis(CALIBRATION_INTERVAL_MS));
        
        if !CALIBRATION_RUNNING.load(Ordering::Acquire) {
            break;
        }
        
        // Collect calibration samples
        let mut samples = Vec::with_capacity(NUM_SAMPLES);
        for _ in 0..NUM_SAMPLES {
            let cycles = read_cpu_cycles();
            let ns = get_os_time_ns();
            samples.push((cycles, ns));
            thread::sleep(Duration::from_millis(1));
        }
        
        // Calculate new calibration
        if let Ok((new_factor, new_base_cycles, new_base_ns)) = calculate_calibration_factor(&samples) {
            update_calibration(new_factor, new_base_cycles, new_base_ns);
        }
    }
}

// Update calibration data using double-buffering
fn update_calibration(new_factor: f64, new_base_cycles: u64, new_base_ns: u64) {
    unsafe {
        let current_index = ACTIVE_BUFFER_INDEX.load(Ordering::Acquire);
        let inactive_index = 1 - current_index;
        
        // Get current calibration for smooth transition
        let current_calib = &CALIBRATION_BUFFERS[current_index];
        
        // Calculate adjustment to maintain monotonicity
        let current_cycles = read_cpu_cycles();
        let old_ns = ((current_cycles - current_calib.base_cycles) as f64 
            * current_calib.cycles_to_ns_factor) as u64 
            + current_calib.base_ns 
            + current_calib.monotonic_offset;
        
        let new_ns = ((current_cycles - new_base_cycles) as f64 * new_factor) as u64 + new_base_ns;
        
        let new_offset = old_ns.saturating_sub(new_ns);
        
        // Update inactive buffer
        CALIBRATION_BUFFERS[inactive_index] = CalibrationData {
            cycles_to_ns_factor: new_factor,
            monotonic_offset: current_calib.monotonic_offset + new_offset,
            base_cycles: new_base_cycles,
            base_ns: new_base_ns,
        };
        
        // Memory barrier to ensure data is visible before index swap
        std::sync::atomic::fence(Ordering::Release);
        
        // Atomically swap to new calibration
        ACTIVE_BUFFER_INDEX.store(inactive_index, Ordering::Release);
    }
}

// Force manual calibration
pub fn calibrate() -> Result<(), &'static str> {
    perform_initial_calibration()
}