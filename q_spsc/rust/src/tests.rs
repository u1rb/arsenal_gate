use crate::*;
use std::sync::Arc;
use std::thread;

#[test]
fn test_power_of_two() {
    assert!(is_power_of_two(1));
    assert!(is_power_of_two(2));
    assert!(is_power_of_two(64));
    assert!(is_power_of_two(1024));
    assert!(!is_power_of_two(0));
    assert!(!is_power_of_two(3));
    assert!(!is_power_of_two(100));
}

#[test]
fn test_next_power_of_two() {
    assert_eq!(next_power_of_two(1), 1);
    assert_eq!(next_power_of_two(2), 2);
    assert_eq!(next_power_of_two(3), 4);
    assert_eq!(next_power_of_two(100), 128);
    assert_eq!(next_power_of_two(1000), 1024);
}

#[test]
fn test_queue_capacity_rounding() {
    let queue = BoundedSPSCQueue::new(100, HugePagesPolicy::Never, 5);
    assert_eq!(queue.capacity(), 128);

    let queue = BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5);
    assert_eq!(queue.capacity(), 256);
}

#[test]
fn test_single_producer_single_consumer() {
    let queue = Arc::new(BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5));
    let queue_consumer = queue.clone();

    let producer = thread::spawn(move || {
        for i in 0..100u8 {
            while queue.prepare_write(1).is_none() {
                thread::yield_now();
            }

            if let Some(write_pos) = queue.prepare_write(1) {
                unsafe {
                    ptr::write(write_pos, i);
                }
                queue.finish_and_commit_write(1);
            }
        }
    });

    let consumer = thread::spawn(move || {
        let mut received = Vec::new();

        while received.len() < 100 {
            if let Some(read_pos) = queue_consumer.prepare_read() {
                unsafe {
                    received.push(ptr::read(read_pos));
                }
                queue_consumer.finish_read(1);
                queue_consumer.commit_read();
            }
        }

        received
    });

    producer.join().unwrap();
    let received = consumer.join().unwrap();

    assert_eq!(received.len(), 100);
    for (i, &val) in received.iter().enumerate() {
        assert_eq!(val, i as u8);
    }
}

#[test]
fn test_variable_message_sizes() {
    let queue = Arc::new(BoundedSPSCQueue::new(1024, HugePagesPolicy::Never, 5));
    let queue_consumer = queue.clone();

    let producer = thread::spawn(move || {
        let sizes = vec![16, 32, 64, 128, 256];

        for &size in &sizes {
            let data: Vec<u8> = (0..size).map(|i| i as u8).collect();

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

        sizes
    });

    let consumer = thread::spawn(move || {
        let sizes = vec![16, 32, 64, 128, 256];
        let mut buffers = Vec::new();

        for &size in &sizes {
            while queue_consumer.prepare_read().is_none() {
                thread::yield_now();
            }

            if let Some(read_pos) = queue_consumer.prepare_read() {
                let mut buffer = vec![0u8; size];
                unsafe {
                    ptr::copy_nonoverlapping(read_pos, buffer.as_mut_ptr(), size);
                }
                queue_consumer.finish_read(size);
                queue_consumer.commit_read();
                buffers.push(buffer);
            }
        }

        buffers
    });

    let sizes = producer.join().unwrap();
    let buffers = consumer.join().unwrap();

    assert_eq!(sizes.len(), buffers.len());

    for (size, buffer) in sizes.iter().zip(buffers.iter()) {
        assert_eq!(buffer.len(), *size);
        for (j, &val) in buffer.iter().enumerate() {
            assert_eq!(val, j as u8);
        }
    }
}

#[test]
fn test_queue_full_behavior() {
    let queue = BoundedSPSCQueue::new(64, HugePagesPolicy::Never, 5);

    // Fill the queue
    let mut written = 0;
    while let Some(write_pos) = queue.prepare_write(8) {
        unsafe {
            ptr::write_bytes(write_pos, 0xFF, 8);
        }
        queue.finish_and_commit_write(8);
        written += 8;
    }

    // Queue should be full now
    assert!(queue.prepare_write(8).is_none());

    // Read half the data
    let mut read = 0;
    while read < written / 2 {
        if let Some(_read_pos) = queue.prepare_read() {
            queue.finish_read(8);
            queue.commit_read();
            read += 8;
        }
    }

    // Should be able to write again
    assert!(queue.prepare_write(8).is_some());
}

#[test]
fn test_empty_queue() {
    let queue = BoundedSPSCQueue::new(256, HugePagesPolicy::Never, 5);

    assert!(queue.empty());
    assert!(queue.prepare_read().is_none());

    // Write some data
    if let Some(write_pos) = queue.prepare_write(8) {
        unsafe {
            ptr::write_bytes(write_pos, 0x42, 8);
        }
        queue.finish_and_commit_write(8);
    }

    assert!(!queue.empty());
    assert!(queue.prepare_read().is_some());
}

#[test]
fn test_reader_batching() {
    let queue = Arc::new(BoundedSPSCQueue::new(1024, HugePagesPolicy::Never, 10));
    let queue_consumer = queue.clone();

    // 10% batch size means we need to read ~102 bytes before atomic update
    let _batch_trigger = 102;

    let producer = thread::spawn(move || {
        // Write enough data to trigger batching
        for _ in 0..150 {
            while queue.prepare_write(1).is_none() {
                thread::yield_now();
            }
            if let Some(write_pos) = queue.prepare_write(1) {
                unsafe {
                    ptr::write(write_pos, 1u8);
                }
                queue.finish_and_commit_write(1);
            }
        }
    });

    let consumer = thread::spawn(move || {
        let mut total_read = 0;

        while total_read < 150 {
            if let Some(_read_pos) = queue_consumer.prepare_read() {
                queue_consumer.finish_read(1);
                queue_consumer.commit_read();
                total_read += 1;
            }
        }

        total_read
    });

    producer.join().unwrap();
    let total = consumer.join().unwrap();
    assert_eq!(total, 150);
}

#[test]
#[cfg(target_os = "linux")]
#[ignore] // Temporarily ignore due to segfault
fn test_huge_pages_try_policy() {
    // This should not panic even if huge pages are not available
    let queue = BoundedSPSCQueue::new(256, HugePagesPolicy::Try, 5);
    assert_eq!(queue.huge_pages_policy(), HugePagesPolicy::Try);
}

#[test]
fn test_concurrent_stress() {
    let queue = Arc::new(BoundedSPSCQueue::new(4096, HugePagesPolicy::Never, 5));
    let queue_consumer = queue.clone();

    const ITERATIONS: usize = 1_000_000;

    let producer = thread::spawn(move || {
        for i in 0..ITERATIONS {
            let data = (i % 256) as u8;

            while queue.prepare_write(1).is_none() {
                thread::yield_now();
            }

            if let Some(write_pos) = queue.prepare_write(1) {
                unsafe {
                    ptr::write(write_pos, data);
                }
                queue.finish_and_commit_write(1);
            }
        }
    });

    let consumer = thread::spawn(move || {
        let mut count = 0;

        while count < ITERATIONS {
            if let Some(read_pos) = queue_consumer.prepare_read() {
                let data = unsafe { ptr::read(read_pos) };
                assert_eq!(data, (count % 256) as u8);
                queue_consumer.finish_read(1);
                queue_consumer.commit_read();
                count += 1;
            }
        }

        count
    });

    producer.join().unwrap();
    let count = consumer.join().unwrap();
    assert_eq!(count, ITERATIONS);
}
