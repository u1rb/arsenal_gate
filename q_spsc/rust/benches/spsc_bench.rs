use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};
use q_spsc::{BoundedSPSCQueue, HugePagesPolicy};
use std::ptr;
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

// Set thread affinity if available
#[cfg(target_os = "linux")]
#[allow(dead_code)]
fn set_cpu_affinity(cpu: usize) {
    use libc::{cpu_set_t, sched_setaffinity, CPU_SET, CPU_ZERO};
    use std::mem;

    unsafe {
        let mut set: cpu_set_t = mem::zeroed();
        CPU_ZERO(&mut set);
        CPU_SET(cpu, &mut set);
        sched_setaffinity(0, mem::size_of::<cpu_set_t>(), &set);
    }
}

fn bench_single_thread_latency(c: &mut Criterion) {
    let mut group = c.benchmark_group("single_thread_latency");
    group.sample_size(10);
    group.measurement_time(Duration::from_secs(2));

    for size in [32, 128, 512].iter() {
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let queue = BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5);
            let data = vec![0u8; size];

            b.iter(|| {
                // Write
                if let Some(write_pos) = queue.prepare_write(size) {
                    unsafe {
                        ptr::copy_nonoverlapping(data.as_ptr(), write_pos, size);
                    }
                    queue.finish_and_commit_write(size);
                }

                // Read
                if let Some(read_pos) = queue.prepare_read() {
                    unsafe {
                        let _ = black_box(ptr::read(read_pos));
                    }
                    queue.finish_read(size);
                    queue.commit_read();
                }
            });
        });
    }
    group.finish();
}

fn bench_producer_hot_path(c: &mut Criterion) {
    let mut group = c.benchmark_group("producer_hot_path");
    group.sample_size(10);
    group.measurement_time(Duration::from_secs(2));

    for size in [32, 128, 512].iter() {
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let queue = Arc::new(BoundedSPSCQueue::new(4096, HugePagesPolicy::Never, 5));
            let queue_consumer = queue.clone();
            let data = vec![0u8; size];

            // Start consumer thread
            let handle = thread::spawn(move || loop {
                if let Some(read_pos) = queue_consumer.prepare_read() {
                    unsafe {
                        let _ = black_box(ptr::read(read_pos));
                    }
                    queue_consumer.finish_read(size);
                    queue_consumer.commit_read();
                }
                thread::yield_now();
            });

            thread::sleep(Duration::from_millis(10));

            b.iter(|| {
                if let Some(write_pos) = queue.prepare_write(size) {
                    unsafe {
                        ptr::copy_nonoverlapping(data.as_ptr(), write_pos, size);
                    }
                    queue.finish_and_commit_write(size);
                }
            });

            drop(handle);
        });
    }
    group.finish();
}

fn bench_ping_pong(c: &mut Criterion) {
    c.bench_function("ping_pong", |b| {
        b.iter_custom(|iters| {
            let queue1 = Arc::new(BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5));
            let queue2 = Arc::new(BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5));

            let q1_consumer = queue1.clone();
            let q2_producer = queue2.clone();

            // Consumer thread
            let handle = thread::spawn(move || {
                for _ in 0..iters {
                    // Wait for ping
                    while q1_consumer.prepare_read().is_none() {
                        thread::yield_now();
                    }
                    q1_consumer.finish_read(1);
                    q1_consumer.commit_read();

                    // Send pong
                    while q2_producer.prepare_write(1).is_none() {
                        thread::yield_now();
                    }
                    unsafe {
                        ptr::write(q2_producer.prepare_write(1).unwrap(), 1);
                    }
                    q2_producer.finish_and_commit_write(1);
                }
            });

            let start = Instant::now();

            // Producer thread
            for _ in 0..iters {
                // Send ping
                while queue1.prepare_write(1).is_none() {
                    thread::yield_now();
                }
                unsafe {
                    ptr::write(queue1.prepare_write(1).unwrap(), 1);
                }
                queue1.finish_and_commit_write(1);

                // Wait for pong
                while queue2.prepare_read().is_none() {
                    thread::yield_now();
                }
                queue2.finish_read(1);
                queue2.commit_read();
            }

            handle.join().unwrap();
            start.elapsed()
        });
    });
}

fn bench_throughput(c: &mut Criterion) {
    let mut group = c.benchmark_group("throughput");
    group.sample_size(10);
    group.measurement_time(Duration::from_secs(2));

    for size in [16, 64, 256, 1024].iter() {
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let queue = Arc::new(BoundedSPSCQueue::new(65536, HugePagesPolicy::Never, 5));
            let queue_consumer = queue.clone();
            let data = vec![0u8; size];

            // Start consumer thread
            let running = Arc::new(std::sync::atomic::AtomicBool::new(true));
            let running_clone = running.clone();
            let handle = thread::spawn(move || {
                while running_clone.load(std::sync::atomic::Ordering::Relaxed) {
                    if let Some(read_pos) = queue_consumer.prepare_read() {
                        unsafe {
                            let _ = black_box(ptr::read(read_pos));
                        }
                        queue_consumer.finish_read(size);
                        queue_consumer.commit_read();
                    } else {
                        thread::yield_now();
                    }
                }
            });

            thread::sleep(Duration::from_millis(10));

            b.iter(|| {
                for _ in 0..1000 {
                    while queue.prepare_write(size).is_none() {
                        thread::yield_now();
                    }
                    if let Some(write_pos) = queue.prepare_write(size) {
                        unsafe {
                            ptr::copy_nonoverlapping(data.as_ptr(), write_pos, size);
                        }
                        queue.finish_and_commit_write(size);
                    }
                }
            });

            running.store(false, std::sync::atomic::Ordering::Relaxed);
            handle.join().unwrap();
        });
    }
    group.finish();
}

criterion_group!(
    benches,
    bench_single_thread_latency,
    bench_producer_hot_path,
    bench_ping_pong,
    bench_throughput
);
criterion_main!(benches);
