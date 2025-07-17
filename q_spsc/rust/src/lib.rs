#![allow(dead_code)]

use std::alloc::Layout;
use std::cell::Cell;
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};

#[cfg(target_os = "linux")]
use libc::{
    mmap, munmap, MAP_ANONYMOUS, MAP_FAILED, MAP_HUGETLB, MAP_PRIVATE, PROT_READ, PROT_WRITE,
};

#[cfg(target_arch = "x86_64")]
use std::arch::x86_64::{_mm_clflush, _mm_prefetch, _MM_HINT_T0};

const CACHE_LINE_SIZE: usize = 64;
const CACHE_LINE_ALIGNED: usize = CACHE_LINE_SIZE;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum HugePagesPolicy {
    Never,
    Always,
    Try,
}

#[inline(always)]
const fn is_power_of_two(n: usize) -> bool {
    n != 0 && (n & (n - 1)) == 0
}

#[inline(always)]
fn next_power_of_two(n: usize) -> usize {
    if is_power_of_two(n) {
        n
    } else {
        n.next_power_of_two()
    }
}

#[inline(always)]
fn align_ptr(ptr: *mut u8, align: usize) -> *mut u8 {
    debug_assert!(is_power_of_two(align));
    let addr = ptr as usize;
    let aligned = (addr + align - 1) & !(align - 1);
    aligned as *mut u8
}

struct AlignedAlloc {
    ptr: *mut u8,
    layout: Layout,
    mmap_info: Option<(*mut u8, usize)>,
}

impl AlignedAlloc {
    fn new(size: usize, align: usize, huge_pages: HugePagesPolicy) -> Self {
        #[cfg(not(target_os = "linux"))]
        {
            let layout = Layout::from_size_align(size, align).unwrap();
            let ptr = unsafe { std::alloc::alloc(layout) };
            if ptr.is_null() {
                panic!("Failed to allocate memory");
            }
            Self {
                ptr,
                layout,
                mmap_info: None,
            }
        }

        #[cfg(target_os = "linux")]
        {
            const METADATA_SIZE: usize = 2 * std::mem::size_of::<usize>();
            let total_size = size + METADATA_SIZE + align;

            let mut flags = MAP_PRIVATE | MAP_ANONYMOUS;
            if huge_pages != HugePagesPolicy::Never {
                flags |= MAP_HUGETLB;
            }

            let mut mem = unsafe {
                mmap(
                    ptr::null_mut(),
                    total_size,
                    PROT_READ | PROT_WRITE,
                    flags,
                    -1,
                    0,
                )
            };

            if mem == MAP_FAILED && huge_pages == HugePagesPolicy::Try {
                mem = unsafe {
                    mmap(
                        ptr::null_mut(),
                        total_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0,
                    )
                };
                if mem == MAP_FAILED {
                    panic!("mmap failed");
                }
            } else if mem == MAP_FAILED {
                panic!("mmap failed");
            }

            let aligned_ptr = align_ptr(unsafe { mem.add(METADATA_SIZE) } as *mut u8, align);
            let offset = aligned_ptr as usize - mem as usize;

            unsafe {
                ptr::write((aligned_ptr as *mut usize).sub(1), total_size);
                ptr::write((aligned_ptr as *mut usize).sub(2), offset);
            }

            Self {
                ptr: aligned_ptr,
                layout: Layout::from_size_align(size, align).unwrap(),
                mmap_info: Some((mem as *mut u8, total_size)),
            }
        }
    }

    #[inline(always)]
    fn as_ptr(&self) -> *mut u8 {
        self.ptr
    }
}

impl Drop for AlignedAlloc {
    fn drop(&mut self) {
        if let Some((mem, total_size)) = self.mmap_info {
            #[cfg(target_os = "linux")]
            unsafe {
                munmap(mem as *mut _, total_size);
            }
        } else {
            unsafe {
                std::alloc::dealloc(self.ptr, self.layout);
            }
        }
    }
}

#[repr(C, align(64))]
struct ProducerCacheLine {
    atomic_writer_pos: AtomicUsize,
    _pad1: [u8; CACHE_LINE_ALIGNED - std::mem::size_of::<AtomicUsize>()],
    writer_pos: Cell<usize>,
    reader_pos_cache: Cell<usize>,
    last_flushed_writer_pos: Cell<usize>,
}

#[repr(C, align(64))]
struct ConsumerCacheLine {
    atomic_reader_pos: AtomicUsize,
    _pad1: [u8; CACHE_LINE_ALIGNED - std::mem::size_of::<AtomicUsize>()],
    reader_pos: Cell<usize>,
    writer_pos_cache: Cell<usize>,
    last_flushed_reader_pos: Cell<usize>,
}

pub struct BoundedSPSCQueue {
    capacity: usize,
    mask: usize,
    bytes_per_batch: usize,
    storage: AlignedAlloc,
    huge_pages_policy: HugePagesPolicy,
    enable_cache_flushing: bool,
    prefetch_distance: usize,
    producer: ProducerCacheLine,
    consumer: ConsumerCacheLine,
}

unsafe impl Send for BoundedSPSCQueue {}
unsafe impl Sync for BoundedSPSCQueue {}

impl BoundedSPSCQueue {
    pub fn new(
        capacity: usize,
        huge_pages_policy: HugePagesPolicy,
        reader_store_percent: usize,
    ) -> Self {
        Self::with_config(capacity, huge_pages_policy, reader_store_percent, false, 64)
    }

    pub fn with_config(
        capacity: usize,
        huge_pages_policy: HugePagesPolicy,
        reader_store_percent: usize,
        enable_cache_flushing: bool,
        avg_message_size: usize,
    ) -> Self {
        let capacity = next_power_of_two(capacity);
        let mask = capacity - 1;
        let bytes_per_batch = (capacity * reader_store_percent) / 100;
        let prefetch_distance = if avg_message_size > 0 {
            avg_message_size.div_ceil(CACHE_LINE_SIZE) * CACHE_LINE_SIZE
        } else {
            0
        };

        let storage = AlignedAlloc::new(capacity, CACHE_LINE_ALIGNED, huge_pages_policy);

        // Initialize storage to zero
        unsafe {
            ptr::write_bytes(storage.as_ptr(), 0, capacity);
        }

        let producer = ProducerCacheLine {
            atomic_writer_pos: AtomicUsize::new(0),
            _pad1: [0; CACHE_LINE_ALIGNED - std::mem::size_of::<AtomicUsize>()],
            writer_pos: Cell::new(0),
            reader_pos_cache: Cell::new(0),
            last_flushed_writer_pos: Cell::new(0),
        };

        let consumer = ConsumerCacheLine {
            atomic_reader_pos: AtomicUsize::new(0),
            _pad1: [0; CACHE_LINE_ALIGNED - std::mem::size_of::<AtomicUsize>()],
            reader_pos: Cell::new(0),
            writer_pos_cache: Cell::new(0),
            last_flushed_reader_pos: Cell::new(0),
        };

        #[cfg(target_arch = "x86_64")]
        {
            unsafe {
                // Remove log memory from cache
                let mut i = 0;
                while i < capacity {
                    _mm_clflush(storage.as_ptr().add(i) as *const u8);
                    i += CACHE_LINE_SIZE;
                }

                // Load cache lines into memory - only for larger queues
                if capacity >= 1024 {
                    let cache_lines = if capacity >= 2048 { 32 } else { 16 };

                    for j in 0..cache_lines {
                        _mm_prefetch(
                            storage.as_ptr().add(CACHE_LINE_SIZE * j) as *const i8,
                            _MM_HINT_T0,
                        );
                    }
                }
            }
        }

        Self {
            capacity,
            mask,
            bytes_per_batch,
            storage,
            huge_pages_policy,
            enable_cache_flushing,
            prefetch_distance,
            producer,
            consumer,
        }
    }

    #[inline(always)]
    pub fn prepare_write(&self, n: usize) -> Option<*mut u8> {
        let writer_pos = self.producer.writer_pos.get();
        let reader_pos_cache = self.producer.reader_pos_cache.get();

        if self.capacity - (writer_pos - reader_pos_cache) < n {
            let reader_pos = self.consumer.atomic_reader_pos.load(Ordering::Acquire);
            self.producer.reader_pos_cache.set(reader_pos);

            if self.capacity - (writer_pos - reader_pos) < n {
                return None;
            }
        }

        unsafe { Some(self.storage.as_ptr().add(writer_pos & self.mask)) }
    }

    #[inline(always)]
    pub fn finish_write(&self, n: usize) {
        self.producer
            .writer_pos
            .set(self.producer.writer_pos.get() + n);
    }

    #[inline(always)]
    pub fn commit_write(&self) {
        let writer_pos = self.producer.writer_pos.get();
        self.producer
            .atomic_writer_pos
            .store(writer_pos, Ordering::Release);

        #[cfg(target_arch = "x86_64")]
        {
            if self.enable_cache_flushing {
                self.flush_cachelines_producer(writer_pos);
            }

            if self.prefetch_distance > 0 {
                unsafe {
                    let prefetch_addr = self
                        .storage
                        .as_ptr()
                        .add((writer_pos + self.prefetch_distance) & self.mask);
                    _mm_prefetch(prefetch_addr as *const i8, _MM_HINT_T0);
                }
            }
        }
    }

    #[inline(always)]
    pub fn finish_and_commit_write(&self, n: usize) {
        self.finish_write(n);
        self.commit_write();
    }

    #[inline(always)]
    pub fn prepare_read(&self) -> Option<*mut u8> {
        if self.empty() {
            None
        } else {
            let reader_pos = self.consumer.reader_pos.get();
            unsafe { Some(self.storage.as_ptr().add(reader_pos & self.mask)) }
        }
    }

    #[inline(always)]
    pub fn finish_read(&self, n: usize) {
        self.consumer
            .reader_pos
            .set(self.consumer.reader_pos.get() + n);
    }

    #[inline(always)]
    pub fn commit_read(&self) {
        let reader_pos = self.consumer.reader_pos.get();
        let atomic_reader_pos = self.consumer.atomic_reader_pos.load(Ordering::Relaxed);

        if self.bytes_per_batch == 0 || reader_pos - atomic_reader_pos >= self.bytes_per_batch {
            self.consumer
                .atomic_reader_pos
                .store(reader_pos, Ordering::Release);

            #[cfg(target_arch = "x86_64")]
            {
                if self.enable_cache_flushing {
                    self.flush_cachelines_consumer(reader_pos);
                }
            }
        }
    }

    #[inline(always)]
    pub fn empty(&self) -> bool {
        let reader_pos = self.consumer.reader_pos.get();
        let writer_pos_cache = self.consumer.writer_pos_cache.get();

        if writer_pos_cache == reader_pos {
            let writer_pos = self.producer.atomic_writer_pos.load(Ordering::Acquire);
            self.consumer.writer_pos_cache.set(writer_pos);

            if writer_pos == reader_pos {
                return true;
            }
        }

        false
    }

    #[inline(always)]
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    #[inline(always)]
    pub fn huge_pages_policy(&self) -> HugePagesPolicy {
        self.huge_pages_policy
    }

    #[cfg(target_arch = "x86_64")]
    #[inline(always)]
    fn flush_cachelines_producer(&self, offset: usize) {
        const CACHE_LINE_MASK: usize = CACHE_LINE_SIZE - 1;

        let last = self.producer.last_flushed_writer_pos.get();
        let mut last_diff = last - (last & CACHE_LINE_MASK);
        let cur_diff = offset - (offset & CACHE_LINE_MASK);

        while cur_diff > last_diff {
            unsafe {
                _mm_clflush(self.storage.as_ptr().add(last_diff & self.mask) as *const u8);
            }

            last_diff += CACHE_LINE_SIZE;
            self.producer.last_flushed_writer_pos.set(last_diff);
        }
    }

    #[cfg(target_arch = "x86_64")]
    #[inline(always)]
    fn flush_cachelines_consumer(&self, offset: usize) {
        const CACHE_LINE_MASK: usize = CACHE_LINE_SIZE - 1;

        let last = self.consumer.last_flushed_reader_pos.get();
        let mut last_diff = last - (last & CACHE_LINE_MASK);
        let cur_diff = offset - (offset & CACHE_LINE_MASK);

        while cur_diff > last_diff {
            unsafe {
                _mm_clflush(self.storage.as_ptr().add(last_diff & self.mask) as *const u8);
            }

            last_diff += CACHE_LINE_SIZE;
            self.consumer.last_flushed_reader_pos.set(last_diff);
        }
    }
}

#[cfg(test)]
mod tests;

pub mod ffi;
