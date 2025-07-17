use std::ffi::c_void;
use std::ptr;

use crate::{BoundedSPSCQueue, HugePagesPolicy};

#[repr(C)]
pub enum QSPSCHugePagesPolicy {
    Never = 0,
    Always = 1,
    Try = 2,
}

impl From<QSPSCHugePagesPolicy> for HugePagesPolicy {
    fn from(policy: QSPSCHugePagesPolicy) -> Self {
        match policy {
            QSPSCHugePagesPolicy::Never => HugePagesPolicy::Never,
            QSPSCHugePagesPolicy::Always => HugePagesPolicy::Always,
            QSPSCHugePagesPolicy::Try => HugePagesPolicy::Try,
        }
    }
}

impl From<HugePagesPolicy> for QSPSCHugePagesPolicy {
    fn from(policy: HugePagesPolicy) -> Self {
        match policy {
            HugePagesPolicy::Never => QSPSCHugePagesPolicy::Never,
            HugePagesPolicy::Always => QSPSCHugePagesPolicy::Always,
            HugePagesPolicy::Try => QSPSCHugePagesPolicy::Try,
        }
    }
}

pub type QSPSCQueue = *mut c_void;

#[no_mangle]
pub extern "C" fn qspsc_new(
    capacity: usize,
    huge_pages_policy: QSPSCHugePagesPolicy,
    reader_store_percent: usize,
) -> QSPSCQueue {
    let queue = Box::new(BoundedSPSCQueue::new(
        capacity,
        huge_pages_policy.into(),
        reader_store_percent,
    ));
    Box::into_raw(queue) as QSPSCQueue
}

#[no_mangle]
pub extern "C" fn qspsc_destroy(queue: QSPSCQueue) {
    if queue.is_null() {
        return;
    }
    unsafe {
        let _ = Box::from_raw(queue as *mut BoundedSPSCQueue);
    }
}

#[no_mangle]
pub extern "C" fn qspsc_prepare_write(queue: QSPSCQueue, n: usize) -> *mut u8 {
    if queue.is_null() {
        return ptr::null_mut();
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.prepare_write(n).unwrap_or(ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn qspsc_finish_write(queue: QSPSCQueue, n: usize) {
    if queue.is_null() {
        return;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.finish_write(n);
}

#[no_mangle]
pub extern "C" fn qspsc_commit_write(queue: QSPSCQueue) {
    if queue.is_null() {
        return;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.commit_write();
}

#[no_mangle]
pub extern "C" fn qspsc_finish_and_commit_write(queue: QSPSCQueue, n: usize) {
    if queue.is_null() {
        return;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.finish_and_commit_write(n);
}

#[no_mangle]
pub extern "C" fn qspsc_prepare_read(queue: QSPSCQueue) -> *mut u8 {
    if queue.is_null() {
        return ptr::null_mut();
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.prepare_read().unwrap_or(ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn qspsc_finish_read(queue: QSPSCQueue, n: usize) {
    if queue.is_null() {
        return;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.finish_read(n);
}

#[no_mangle]
pub extern "C" fn qspsc_commit_read(queue: QSPSCQueue) {
    if queue.is_null() {
        return;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.commit_read();
}

#[no_mangle]
pub extern "C" fn qspsc_empty(queue: QSPSCQueue) -> bool {
    if queue.is_null() {
        return true;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.empty()
}

#[no_mangle]
pub extern "C" fn qspsc_capacity(queue: QSPSCQueue) -> usize {
    if queue.is_null() {
        return 0;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.capacity()
}

#[no_mangle]
pub extern "C" fn qspsc_huge_pages_policy(queue: QSPSCQueue) -> QSPSCHugePagesPolicy {
    if queue.is_null() {
        return QSPSCHugePagesPolicy::Never;
    }
    let queue = unsafe { &*(queue as *const BoundedSPSCQueue) };
    queue.huge_pages_policy().into()
}
