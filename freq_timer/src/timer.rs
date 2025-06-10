use crate::calibration;
use crate::platform::read_cpu_cycles;

// Timer handle for elapsed time measurement
#[repr(C)]
pub struct TimerHandle {
    pub start_cycles: u64,
}

#[inline(always)]
pub fn now_nanoseconds() -> u64 {
    let cycles = read_cpu_cycles();
    calibration::cycles_to_nanoseconds(cycles)
}

#[inline(always)]
pub fn now_cycles() -> u64 {
    read_cpu_cycles()
}

#[inline(always)]
pub fn start_timer() -> TimerHandle {
    TimerHandle {
        start_cycles: read_cpu_cycles(),
    }
}

#[inline(always)]
pub fn elapsed_nanoseconds(handle: &TimerHandle) -> u64 {
    let current_cycles = read_cpu_cycles();
    let elapsed_cycles = current_cycles.saturating_sub(handle.start_cycles);
    calibration::cycles_to_nanoseconds_delta(elapsed_cycles)
}

#[inline(always)]
pub fn elapsed_cycles(handle: &TimerHandle) -> u64 {
    let current_cycles = read_cpu_cycles();
    current_cycles.saturating_sub(handle.start_cycles)
}

// Batch timing support
#[repr(C)]
pub struct TimerBatch {
    pub timestamps: Vec<u64>,
    pub capacity: usize,
}

impl TimerBatch {
    pub fn new(capacity: usize) -> Self {
        TimerBatch {
            timestamps: Vec::with_capacity(capacity),
            capacity,
        }
    }

    #[inline(always)]
    pub fn capture(&mut self) -> bool {
        if self.timestamps.len() < self.capacity {
            self.timestamps.push(now_nanoseconds());
            true
        } else {
            false
        }
    }

    pub fn clear(&mut self) {
        self.timestamps.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn test_monotonic() {
        let t1 = now_nanoseconds();
        let t2 = now_nanoseconds();
        assert!(t2 >= t1, "Time should be monotonic");
    }

    #[test]
    fn test_timer_elapsed() {
        let timer = start_timer();
        thread::sleep(Duration::from_millis(1));
        let elapsed = elapsed_nanoseconds(&timer);
        assert!(elapsed > 1_000_000, "Should measure at least 1ms");
        assert!(elapsed < 100_000_000, "Should measure less than 100ms");
    }
}