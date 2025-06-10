// Platform-specific timing functions

#[cfg(target_arch = "x86_64")]
use std::arch::x86_64::_rdtsc;

#[inline(always)]
#[cfg(target_arch = "x86_64")]
pub fn read_cpu_cycles() -> u64 {
    // Use RDTSCP if available for better accuracy (serializing instruction)
    // For now, we'll use RDTSC which is sufficient for Intel VMware VMs
    unsafe { _rdtsc() }
}

#[inline(always)]
#[cfg(target_arch = "aarch64")]
pub fn read_cpu_cycles() -> u64 {
    let mut val: u64;
    unsafe {
        // Read CNTVCT_EL0 (Virtual Timer Count register)
        std::arch::asm!("mrs {}, cntvct_el0", out(reg) val);
    }
    val
}

// Fallback for unsupported architectures
#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
pub fn read_cpu_cycles() -> u64 {
    // Fallback to OS time
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos() as u64
}

// Get OS reference time in nanoseconds
pub fn get_os_time_ns() -> u64 {
    use libc::{clock_gettime, timespec, CLOCK_MONOTONIC};
    
    let mut ts = timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    
    unsafe {
        if clock_gettime(CLOCK_MONOTONIC, &mut ts) != 0 {
            panic!("clock_gettime failed");
        }
    }
    
    (ts.tv_sec as u64) * 1_000_000_000 + (ts.tv_nsec as u64)
}

// CPU frequency detection helpers
pub fn detect_cpu_info() -> CpuInfo {
    CpuInfo {
        has_constant_tsc: check_constant_tsc(),
        has_nonstop_tsc: check_nonstop_tsc(),
        nominal_frequency: estimate_cpu_frequency(),
    }
}

pub struct CpuInfo {
    pub has_constant_tsc: bool,
    pub has_nonstop_tsc: bool,
    pub nominal_frequency: u64, // Hz
}

#[cfg(target_arch = "x86_64")]
fn check_constant_tsc() -> bool {
    // Check for constant TSC support
    use std::arch::x86_64::__cpuid;
    
    unsafe {
        // Check if CPUID is supported
        let cpuid_result = __cpuid(0x80000007);
        // EDX bit 8 indicates constant TSC
        (cpuid_result.edx & (1 << 8)) != 0
    }
}

#[cfg(not(target_arch = "x86_64"))]
fn check_constant_tsc() -> bool {
    false // Assume no constant TSC on non-x86
}

#[cfg(target_arch = "x86_64")]
fn check_nonstop_tsc() -> bool {
    // Similar check for nonstop TSC
    true // Simplified for now
}

#[cfg(not(target_arch = "x86_64"))]
fn check_nonstop_tsc() -> bool {
    false
}

// Estimate CPU frequency by measuring cycles over a known time period
fn estimate_cpu_frequency() -> u64 {
    use std::thread;
    use std::time::Duration;
    
    const MEASUREMENT_MS: u64 = 10;
    
    let start_cycles = read_cpu_cycles();
    let start_time = get_os_time_ns();
    
    thread::sleep(Duration::from_millis(MEASUREMENT_MS));
    
    let end_cycles = read_cpu_cycles();
    let end_time = get_os_time_ns();
    
    let elapsed_cycles = end_cycles - start_cycles;
    let elapsed_ns = end_time - start_time;
    
    if elapsed_ns == 0 {
        // Fallback frequency
        2_000_000_000 // 2 GHz
    } else {
        // Convert to Hz
        (elapsed_cycles as u128 * 1_000_000_000) as u64 / elapsed_ns
    }
}