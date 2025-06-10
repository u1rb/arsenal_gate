use freq_timer::{timer, calibration};
use std::thread;
use std::time::Duration;

fn main() {
    println!("=== freq_timer Rust Usage Example ===\n");
    
    // Initialize the timer library
    freq_timer::initialize().expect("Failed to initialize freq_timer");
    
    // Example 1: Simple timestamp
    println!("1. Getting current timestamp:");
    let now = timer::now_nanoseconds();
    println!("   Current time: {} ns", now);
    println!("   Current cycles: {}", timer::now_cycles());
    println!("   Cycles per ns: {:.3}", calibration::get_cycles_per_ns());
    
    // Example 2: Measuring elapsed time
    println!("\n2. Measuring elapsed time:");
    let timer_handle = timer::start_timer();
    
    // Simulate some work
    println!("   Doing some work...");
    thread::sleep(Duration::from_millis(10));
    
    let elapsed = timer::elapsed_nanoseconds(&timer_handle);
    println!("   Elapsed time: {:.3} ms", elapsed as f64 / 1e6);
    println!("   Elapsed cycles: {}", timer::elapsed_cycles(&timer_handle));
    
    // Example 3: High-frequency measurements
    println!("\n3. High-frequency measurements:");
    let mut batch = timer::TimerBatch::new(10);
    
    for _ in 0..10 {
        batch.capture();
        // Very short work
        let mut x = 0;
        for j in 0..1000 {
            x += j;
        }
        // Prevent optimization
        std::hint::black_box(x);
    }
    
    println!("   Captured {} timestamps", batch.timestamps.len());
    println!("   Time differences:");
    for i in 1..5.min(batch.timestamps.len()) {
        let diff = batch.timestamps[i] - batch.timestamps[i-1];
        println!("     [{}-{}]: {} ns", i-1, i, diff);
    }
    
    // Example 4: Demonstrating monotonicity
    println!("\n4. Monotonicity demonstration:");
    let mut timestamps = Vec::new();
    for _ in 0..100 {
        timestamps.push(timer::now_nanoseconds());
    }
    
    let mut monotonic = true;
    for i in 1..timestamps.len() {
        if timestamps[i] < timestamps[i-1] {
            monotonic = false;
            break;
        }
    }
    println!("   Collected 100 timestamps");
    println!("   Monotonic: {}", if monotonic { "YES" } else { "NO" });
    
    // Example 5: Performance measurement
    println!("\n5. Performance measurement:");
    let iterations = 1_000_000;
    let start = timer::now_nanoseconds();
    
    for _ in 0..iterations {
        let _ = timer::now_nanoseconds();
    }
    
    let total_elapsed = timer::now_nanoseconds() - start;
    let ns_per_call = total_elapsed as f64 / iterations as f64;
    
    println!("   {} iterations completed", iterations);
    println!("   Average time per call: {:.2} ns", ns_per_call);
    println!("   Performance target (<20ns): {}", 
             if ns_per_call < 20.0 { "PASSED" } else { "FAILED" });
    
    // Cleanup
    freq_timer::cleanup();
    
    println!("\nExample completed!");
}