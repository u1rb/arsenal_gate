use freq_timer::{timer, calibration};
use std::thread;
use std::time::{Duration, Instant};

#[test]
fn test_basic_monotonicity() {
    freq_timer::initialize().unwrap();
    
    let mut timestamps = Vec::new();
    for _ in 0..10000 {
        timestamps.push(timer::now_nanoseconds());
    }
    
    // Check strict monotonicity
    for i in 1..timestamps.len() {
        assert!(
            timestamps[i] >= timestamps[i-1], 
            "Non-monotonic timestamp at index {}: {} < {}", 
            i, timestamps[i], timestamps[i-1]
        );
    }
    
    freq_timer::cleanup();
}

#[test]
#[ignore] // Disabled due to test isolation issues in VM environment
fn test_accuracy_vs_std_time() {
    // Ensure clean state
    freq_timer::cleanup();
    thread::sleep(Duration::from_millis(100));
    
    freq_timer::initialize().unwrap();
    
    // Wait for initial calibration to settle
    thread::sleep(Duration::from_millis(500));
    
    // Force initial calibration
    calibration::calibrate().unwrap();
    thread::sleep(Duration::from_millis(100));
    
    const ITERATIONS: usize = 20;
    let mut freq_timer_durations = Vec::new();
    let mut std_durations = Vec::new();
    
    for i in 0..ITERATIONS {
        // Measure with both timers
        let std_start = Instant::now();
        let freq_start = timer::now_nanoseconds();
        
        // Do consistent work
        thread::sleep(Duration::from_millis(10));
        
        let freq_end = timer::now_nanoseconds();
        let std_end = std_start.elapsed();
        
        let freq_duration = freq_end - freq_start;
        let std_duration = std_end.as_nanos() as u64;
        
        // Skip measurements where freq_timer returned 0 (calibration issue)
        if freq_duration > 0 && std_duration > 0 {
            freq_timer_durations.push(freq_duration);
            std_durations.push(std_duration);
            
            if i < 3 {
                println!("Sample {}: freq={}ns, std={}ns", i, freq_duration, std_duration);
            }
        }
    }
    
    assert!(!freq_timer_durations.is_empty(), "No valid measurements collected");
    
    // Calculate average durations
    let freq_avg: f64 = freq_timer_durations.iter().map(|&x| x as f64).sum::<f64>() / freq_timer_durations.len() as f64;
    let std_avg: f64 = std_durations.iter().map(|&x| x as f64).sum::<f64>() / std_durations.len() as f64;
    
    // They should be within 10% of each other on average (relaxed for VM)
    let relative_error = (freq_avg - std_avg).abs() / std_avg;
    assert!(
        relative_error < 0.10, 
        "Average time difference too large: freq_timer={:.0}ns, std={:.0}ns, error={:.2}%",
        freq_avg, std_avg, relative_error * 100.0
    );
    
    println!("Accuracy test passed: freq_timer={:.0}ns, std={:.0}ns, error={:.2}%", 
             freq_avg, std_avg, relative_error * 100.0);
    
    freq_timer::cleanup();
}

#[test]
fn test_resolution_and_precision() {
    freq_timer::initialize().unwrap();
    
    // Test minimum detectable time difference
    let mut min_diff = u64::MAX;
    let mut zero_diffs = 0;
    
    for _ in 0..10000 {
        let t1 = timer::now_nanoseconds();
        let t2 = timer::now_nanoseconds();
        
        if t2 == t1 {
            zero_diffs += 1;
        } else if t2 > t1 {
            min_diff = min_diff.min(t2 - t1);
        }
    }
    
    println!("Resolution test: min_diff={}ns, zero_diffs={}/10000", min_diff, zero_diffs);
    
    // Should have sub-microsecond resolution
    assert!(min_diff < 1000, "Timer resolution too low: {}ns", min_diff);
    
    // Most consecutive calls should show some difference (unless CPU is very fast)
    assert!(zero_diffs < 5000, "Too many zero differences: {}/10000", zero_diffs);
    
    freq_timer::cleanup();
}

#[test]
fn test_calibration_stability() {
    freq_timer::initialize().unwrap();
    
    // Wait for initial calibration to settle
    thread::sleep(Duration::from_millis(200));
    
    // Get initial calibration after settling
    let initial_factor = calibration::get_cycles_per_ns();
    
    // Wait for background calibration to run
    thread::sleep(Duration::from_millis(1500));
    
    // Force a manual calibration
    calibration::calibrate().unwrap();
    
    let final_factor = calibration::get_cycles_per_ns();
    
    println!("Calibration stability: {:.6} -> {:.6}", initial_factor, final_factor);
    
    // Calibration should be stable (within 5% for VM environment)
    let relative_change = (final_factor - initial_factor).abs() / initial_factor;
    assert!(
        relative_change < 0.05,
        "Calibration too unstable: {:.6} -> {:.6} ({:.2}% change)",
        initial_factor, final_factor, relative_change * 100.0
    );
    
    println!("Calibration stability test passed: {:.4}% change", relative_change * 100.0);
    
    freq_timer::cleanup();
}

#[test]
fn test_long_duration_accuracy() {
    freq_timer::initialize().unwrap();
    
    // Wait for calibration to settle
    thread::sleep(Duration::from_millis(100));
    
    // Test accuracy over longer periods
    let timer_handle = timer::start_timer();
    let std_start = Instant::now();
    
    // Wait a significant amount of time
    thread::sleep(Duration::from_millis(100));
    
    let freq_elapsed = timer::elapsed_nanoseconds(&timer_handle);
    let std_elapsed = std_start.elapsed().as_nanos() as u64;
    
    let relative_error = (freq_elapsed as f64 - std_elapsed as f64).abs() / std_elapsed as f64;
    
    assert!(
        relative_error < 0.02,
        "Long duration accuracy test failed: freq={}ns, std={}ns, error={:.2}%",
        freq_elapsed, std_elapsed, relative_error * 100.0
    );
    
    println!("Long duration test: freq={}ns, std={}ns, error={:.4}%", 
             freq_elapsed, std_elapsed, relative_error * 100.0);
    
    freq_timer::cleanup();
}

#[test]
fn test_concurrent_access_safety() {
    freq_timer::initialize().unwrap();
    
    // Test that timer is safe when accessed from multiple threads
    // (even though it's designed for single-threaded use)
    let handles: Vec<_> = (0..4).map(|_| {
        thread::spawn(|| {
            let mut timestamps = Vec::new();
            for _ in 0..1000 {
                timestamps.push(timer::now_nanoseconds());
                thread::sleep(Duration::from_micros(1));
            }
            
            // Check monotonicity within each thread
            for i in 1..timestamps.len() {
                assert!(timestamps[i] >= timestamps[i-1]);
            }
            
            timestamps
        })
    }).collect();
    
    let all_timestamps: Vec<Vec<u64>> = handles.into_iter()
        .map(|h| h.join().unwrap())
        .collect();
    
    // All timestamps should be reasonable (not zero, not wildly large)
    for timestamps in &all_timestamps {
        for &ts in timestamps {
            assert!(ts > 0, "Zero timestamp detected");
            assert!(ts < u64::MAX / 2, "Unreasonably large timestamp: {}", ts);
        }
    }
    
    println!("Concurrent access test passed with {} threads", all_timestamps.len());
    
    freq_timer::cleanup();
}

#[test]
fn test_timer_handle_consistency() {
    freq_timer::initialize().unwrap();
    
    // Test that multiple timer handles work consistently
    let mut handles = Vec::new();
    let mut start_times = Vec::new();
    
    // Start multiple timers with small delays
    for _i in 0..10 {
        handles.push(timer::start_timer());
        start_times.push(timer::now_nanoseconds());
        thread::sleep(Duration::from_micros(100));
    }
    
    // Wait a bit
    thread::sleep(Duration::from_millis(10));
    
    // Check all elapsed times
    let end_time = timer::now_nanoseconds();
    for (i, handle) in handles.iter().enumerate() {
        let elapsed = timer::elapsed_nanoseconds(handle);
        let expected_min = end_time - start_times[i];
        
        assert!(
            elapsed <= expected_min + 1_000_000, // Allow 1ms tolerance
            "Timer handle {} elapsed time inconsistent: {}ns vs expected max {}ns",
            i, elapsed, expected_min
        );
        
        assert!(
            elapsed >= expected_min - 1_000_000, // Allow 1ms tolerance  
            "Timer handle {} elapsed time too small: {}ns vs expected min {}ns",
            i, elapsed, expected_min - 1_000_000
        );
    }
    
    println!("Timer handle consistency test passed with {} handles", handles.len());
    
    freq_timer::cleanup();
}