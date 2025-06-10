use criterion::{black_box, criterion_group, criterion_main, Criterion, BenchmarkId};
use freq_timer::timer;
use std::time::Instant;

fn bench_freq_timer(c: &mut Criterion) {
    // Initialize the timer system
    freq_timer::initialize().expect("Failed to initialize freq_timer");
    
    // Benchmark freq_timer_now_ns
    c.bench_function("freq_timer_now_ns", |b| {
        b.iter(|| {
            black_box(timer::now_nanoseconds())
        })
    });
    
    // Benchmark freq_timer_now_cycles (raw cycles, should be fastest)
    c.bench_function("freq_timer_now_cycles", |b| {
        b.iter(|| {
            black_box(timer::now_cycles())
        })
    });
    
    // Benchmark std::time::Instant for comparison
    c.bench_function("std_time_instant_now", |b| {
        b.iter(|| {
            black_box(Instant::now())
        })
    });
    
    // Benchmark timer handle operations
    c.bench_function("timer_start_and_elapsed", |b| {
        b.iter(|| {
            let timer = timer::start_timer();
            black_box(timer::elapsed_nanoseconds(&timer))
        })
    });
    
    // Benchmark FFI overhead
    c.bench_function("ffi_freq_timer_now_ns", |b| {
        b.iter(|| {
            black_box(freq_timer::ffi::freq_timer_now_ns())
        })
    });
    
    // Benchmark batch operations
    let mut group = c.benchmark_group("batch_operations");
    for size in [10, 100, 1000].iter() {
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let mut batch = timer::TimerBatch::new(size);
            b.iter(|| {
                batch.clear();
                for _ in 0..size {
                    batch.capture();
                }
            });
        });
    }
    group.finish();
    
    // Cleanup
    freq_timer::cleanup();
}

fn bench_comparison(c: &mut Criterion) {
    freq_timer::initialize().expect("Failed to initialize freq_timer");
    
    let mut group = c.benchmark_group("timing_comparison");
    
    // Compare different timing methods
    group.bench_function("freq_timer", |b| {
        b.iter(|| {
            let start = timer::now_nanoseconds();
            // Minimal work to measure overhead
            let _x = black_box(42);
            let end = timer::now_nanoseconds();
            black_box(end - start)
        })
    });
    
    group.bench_function("std_instant", |b| {
        b.iter(|| {
            let start = Instant::now();
            // Minimal work to measure overhead
            let _x = black_box(42);
            let elapsed = start.elapsed();
            black_box(elapsed.as_nanos())
        })
    });
    
    group.finish();
    
    freq_timer::cleanup();
}

fn bench_accuracy(c: &mut Criterion) {
    freq_timer::initialize().expect("Failed to initialize freq_timer");
    
    // Measure the accuracy of sleep measurements
    c.bench_function("sleep_accuracy_1us", |b| {
        b.iter(|| {
            let start = timer::now_nanoseconds();
            std::thread::sleep(std::time::Duration::from_micros(1));
            let end = timer::now_nanoseconds();
            black_box(end - start)
        })
    });
    
    freq_timer::cleanup();
}

criterion_group!(benches, bench_freq_timer, bench_comparison, bench_accuracy);
criterion_main!(benches);