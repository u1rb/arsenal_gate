#pragma once

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
  #include <malloc.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
#endif

// x86 architecture detection
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
  #define Q_SPSC_X86ARCH
#endif

#if defined(Q_SPSC_X86ARCH)
  #if defined(_WIN32)
    #include <intrin.h>
  #else
    #include <immintrin.h>
  #endif
#endif

namespace q_spsc {

// Cache line size - typically 64 bytes on modern processors
static constexpr size_t CACHE_LINE_SIZE = 64u;
static constexpr size_t CACHE_LINE_ALIGNED = CACHE_LINE_SIZE;

// Huge pages policy for Linux systems
enum class HugePagesPolicy {
  Never,  // Do not use huge pages
  Always, // Use huge pages, fail if unavailable
  Try     // Try huge pages, but fall back to normal pages if unavailable
};

// Compiler hints for hot/cold paths
#if defined(__GNUC__) || defined(__clang__)
  #define Q_SPSC_HOT __attribute__((hot))
  #define Q_SPSC_NODISCARD [[nodiscard]]
  #define Q_SPSC_LIKELY(x) __builtin_expect(!!(x), 1)
  #define Q_SPSC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define Q_SPSC_HOT
  #define Q_SPSC_NODISCARD
  #define Q_SPSC_LIKELY(x) (x)
  #define Q_SPSC_UNLIKELY(x) (x)
#endif

namespace detail {

// Check if a number is a power of 2
constexpr bool is_power_of_two(uint64_t number) noexcept {
  return (number != 0) && ((number & (number - 1)) == 0);
}

// Round up to the next power of 2
template <typename T>
T next_power_of_two(T n) noexcept {
  if (is_power_of_two(static_cast<uint64_t>(n))) {
    return n;
  }
  
  T result = 1;
  while (result < n) {
    result <<= 1;
  }
  
  assert(is_power_of_two(static_cast<uint64_t>(result)));
  return result;
}

// Align a pointer to the given alignment
static std::byte* align_pointer(void* pointer, size_t alignment) noexcept {
  assert(is_power_of_two(alignment) && "alignment must be a power of two");
  return reinterpret_cast<std::byte*>(
    (reinterpret_cast<uintptr_t>(pointer) + (alignment - 1ul)) & ~(alignment - 1ul)
  );
}

// Aligned memory allocation
static void* alloc_aligned(size_t size, size_t alignment, HugePagesPolicy huge_pages_policy) {
#if defined(_WIN32)
  void* p = _aligned_malloc(size, alignment);
  if (!p) {
    throw std::runtime_error("alloc_aligned failed with errno: " + std::to_string(errno));
  }
  return p;
#else
  // Calculate the total size including the metadata and alignment
  constexpr size_t metadata_size = 2u * sizeof(size_t);
  size_t const total_size = size + metadata_size + alignment;
  
  // Allocate the memory
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  
  #if defined(__linux__)
  if (huge_pages_policy != HugePagesPolicy::Never) {
    flags |= MAP_HUGETLB;
  }
  #endif
  
  void* mem = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);
  
  #if defined(__linux__)
  if ((mem == MAP_FAILED) && (huge_pages_policy == HugePagesPolicy::Try)) {
    // Try normal pages if huge pages failed
    flags &= ~MAP_HUGETLB;
    mem = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);
  }
  #endif
  
  if (mem == MAP_FAILED) {
    throw std::runtime_error("mmap failed. errno: " + std::to_string(errno));
  }
  
  // Calculate the aligned address after the metadata
  std::byte* aligned_address = align_pointer(static_cast<std::byte*>(mem) + metadata_size, alignment);
  
  // Calculate the offset from the original memory location
  auto const offset = static_cast<size_t>(aligned_address - static_cast<std::byte*>(mem));
  
  // Store the size and offset information in the metadata
  std::memcpy(aligned_address - sizeof(size_t), &total_size, sizeof(total_size));
  std::memcpy(aligned_address - (2u * sizeof(size_t)), &offset, sizeof(offset));
  
  return aligned_address;
#endif
}

// Free aligned memory
static void free_aligned(void* ptr) noexcept {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  // Retrieve the size and offset information from the metadata
  size_t offset;
  std::memcpy(&offset, static_cast<std::byte*>(ptr) - (2u * sizeof(size_t)), sizeof(offset));
  
  size_t total_size;
  std::memcpy(&total_size, static_cast<std::byte*>(ptr) - sizeof(size_t), sizeof(total_size));
  
  // Calculate the original memory block address
  void* mem = static_cast<std::byte*>(ptr) - offset;
  
  ::munmap(mem, total_size);
#endif
}

} // namespace detail

/**
 * A bounded single producer single consumer ring buffer.
 * 
 * This is a wait-free SPSC queue optimized for low latency HFT applications.
 * Features:
 * - Fixed capacity (power of 2) with no runtime allocations
 * - Cache-line aligned producer and consumer data
 * - x86 optimizations including cache prefetching and flushing
 * - Support for huge pages on Linux
 * - Batched reader commits to reduce cache coherence traffic
 * 
 * Usage pattern:
 * Producer:
 *   auto* write_pos = queue.prepare_write(size);
 *   if (write_pos) {
 *     // Write data to write_pos
 *     queue.finish_write(size);
 *     queue.commit_write();
 *   }
 * 
 * Consumer:
 *   auto* read_pos = queue.prepare_read();
 *   if (read_pos) {
 *     // Read data from read_pos
 *     size_t bytes_read = process_data(read_pos);
 *     queue.finish_read(bytes_read);
 *     queue.commit_read();
 *   }
 */
template <typename T = size_t>
class BoundedSPSCQueue {
public:
  using integer_type = T;
  
  /**
   * Constructor
   * @param capacity Queue capacity (will be rounded up to power of 2)
   * @param huge_pages_policy Policy for huge pages allocation (Linux only)
   * @param reader_store_percent Percentage of capacity for batched reader commits (0 = no batching for lowest latency)
   * @param enable_cache_flushing Enable cache line flushing (false = lowest latency on modern x86)
   * @param avg_message_size Average message size in bytes for optimal prefetch distance (0 = disable prefetch)
   */
  Q_SPSC_HOT explicit BoundedSPSCQueue(
    integer_type capacity,
    HugePagesPolicy huge_pages_policy = HugePagesPolicy::Never,
    integer_type reader_store_percent = 0,
    bool enable_cache_flushing = false,
    integer_type avg_message_size = 64)
    : _capacity(detail::next_power_of_two(capacity)),
      _mask(_capacity - 1),
      _bytes_per_batch(static_cast<integer_type>(
        static_cast<double>(_capacity * reader_store_percent) / 100.0)),
      _storage(static_cast<std::byte*>(detail::alloc_aligned(
        static_cast<uint64_t>(_capacity), CACHE_LINE_ALIGNED, huge_pages_policy))),
      _huge_pages_policy(huge_pages_policy),
      _enable_cache_flushing(enable_cache_flushing),
      _prefetch_distance(avg_message_size > 0 ? 
        ((avg_message_size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE : 0) {
    
    std::memset(_storage, 0, static_cast<uint64_t>(_capacity));
    
    _atomic_writer_pos.store(0);
    _atomic_reader_pos.store(0);
    
#if defined(Q_SPSC_X86ARCH)
    // Remove log memory from cache
    for (uint64_t i = 0; i < static_cast<uint64_t>(_capacity); i += CACHE_LINE_SIZE) {
      _mm_clflush(_storage + i);
    }
    
    // Load cache lines into memory
    // Only apply optimization for larger queues
    if (_capacity >= 1024) {
      uint64_t const cache_lines = (_capacity >= 2048) ? 32 : 16;
      
      for (uint64_t i = 0; i < cache_lines; ++i) {
        _mm_prefetch(reinterpret_cast<char const*>(_storage + (CACHE_LINE_SIZE * i)), _MM_HINT_T0);
      }
    }
#endif
  }
  
  ~BoundedSPSCQueue() { 
    detail::free_aligned(_storage);
  }
  
  // Deleted copy/move operations
  BoundedSPSCQueue(BoundedSPSCQueue const&) = delete;
  BoundedSPSCQueue& operator=(BoundedSPSCQueue const&) = delete;
  BoundedSPSCQueue(BoundedSPSCQueue&&) = delete;
  BoundedSPSCQueue& operator=(BoundedSPSCQueue&&) = delete;
  
  /**
   * Prepare to write n bytes. Returns pointer to write position or nullptr if not enough space.
   * Must be called by producer only.
   */
  Q_SPSC_NODISCARD Q_SPSC_HOT std::byte* prepare_write(integer_type n) noexcept {
    if ((_capacity - static_cast<integer_type>(_writer_pos - _reader_pos_cache)) < n) {
      // Not enough space, load reader position and re-check
      _reader_pos_cache = _atomic_reader_pos.load(std::memory_order_acquire);
      
      if ((_capacity - static_cast<integer_type>(_writer_pos - _reader_pos_cache)) < n) {
        return nullptr;
      }
    }
    
    return _storage + (_writer_pos & _mask);
  }
  
  /**
   * Finish writing n bytes. Must be called after prepare_write.
   * Must be called by producer only.
   */
  Q_SPSC_HOT void finish_write(integer_type n) noexcept {
    _writer_pos += n;
  }
  
  /**
   * Commit writes, making them visible to the consumer.
   * Must be called by producer only.
   */
  Q_SPSC_HOT void commit_write() noexcept {
    // Make writes visible to reader
    _atomic_writer_pos.store(_writer_pos, std::memory_order_release);
    
#if defined(Q_SPSC_X86ARCH)
    if (_enable_cache_flushing) {
      // Flush written cache lines
      _flush_cachelines(_last_flushed_writer_pos, _writer_pos);
    }
    
    // Prefetch a future cache line based on average message size
    if (_prefetch_distance > 0) {
      _mm_prefetch(
        reinterpret_cast<char const*>(_storage + ((_writer_pos + _prefetch_distance) & _mask)),
        _MM_HINT_T0);
    }
#endif
  }
  
  /**
   * Combined finish and commit write operation.
   * Must be called by producer only.
   */
  Q_SPSC_HOT void finish_and_commit_write(integer_type n) noexcept {
    finish_write(n);
    commit_write();
  }
  
  /**
   * Prepare to read. Returns pointer to read position or nullptr if empty.
   * Must be called by consumer only.
   */
  Q_SPSC_NODISCARD Q_SPSC_HOT std::byte* prepare_read() noexcept {
    if (empty()) {
      return nullptr;
    }
    
    return _storage + (_reader_pos & _mask);
  }
  
  /**
   * Finish reading n bytes. Must be called after prepare_read.
   * Must be called by consumer only.
   */
  Q_SPSC_HOT void finish_read(integer_type n) noexcept {
    _reader_pos += n;
  }
  
  /**
   * Commit reads. Uses batching to reduce cache coherence traffic if configured.
   * Must be called by consumer only.
   */
  Q_SPSC_HOT void commit_read() noexcept {
    if (_bytes_per_batch == 0 || 
        static_cast<integer_type>(_reader_pos - _atomic_reader_pos.load(std::memory_order_relaxed)) >= 
        _bytes_per_batch) {
      _atomic_reader_pos.store(_reader_pos, std::memory_order_release);
      
#if defined(Q_SPSC_X86ARCH)
      if (_enable_cache_flushing) {
        _flush_cachelines(_last_flushed_reader_pos, _reader_pos);
      }
#endif
    }
  }
  
  /**
   * Check if the queue is empty from the consumer's perspective.
   * Must be called by consumer only.
   */
  Q_SPSC_NODISCARD Q_SPSC_HOT bool empty() const noexcept {
    if (_writer_pos_cache == _reader_pos) {
      // Check atomic variable if we think queue is empty
      _writer_pos_cache = _atomic_writer_pos.load(std::memory_order_acquire);
      
      if (_writer_pos_cache == _reader_pos) {
        return true;
      }
    }
    
    return false;
  }
  
  /**
   * Get the capacity of the queue.
   */
  Q_SPSC_NODISCARD integer_type capacity() const noexcept {
    return static_cast<integer_type>(_capacity);
  }
  
  /**
   * Get the huge pages policy used by this queue.
   */
  Q_SPSC_NODISCARD HugePagesPolicy huge_pages_policy() const noexcept {
    return _huge_pages_policy;
  }
  
private:
#if defined(Q_SPSC_X86ARCH)
  static constexpr integer_type CACHE_LINE_MASK = CACHE_LINE_SIZE - 1;
  
  Q_SPSC_HOT void _flush_cachelines(integer_type& last, integer_type offset) {
    integer_type last_diff = last - (last & CACHE_LINE_MASK);
    integer_type const cur_diff = offset - (offset & CACHE_LINE_MASK);
    
    while (cur_diff > last_diff) {
      _mm_clflushopt(_storage + (last_diff & _mask));
      last_diff += CACHE_LINE_SIZE;
      last = last_diff;
    }
  }
#endif
  
private:
  // Constant members
  integer_type const _capacity;
  integer_type const _mask;
  integer_type const _bytes_per_batch;
  std::byte* const _storage{nullptr};
  HugePagesPolicy const _huge_pages_policy;
  bool const _enable_cache_flushing;
  integer_type const _prefetch_distance;
  
  // Producer cache line
  alignas(CACHE_LINE_ALIGNED) std::atomic<integer_type> _atomic_writer_pos{0};
  alignas(CACHE_LINE_ALIGNED) integer_type _writer_pos{0};
  integer_type _reader_pos_cache{0};
  integer_type _last_flushed_writer_pos{0};
  
  // Consumer cache line
  alignas(CACHE_LINE_ALIGNED) std::atomic<integer_type> _atomic_reader_pos{0};
  alignas(CACHE_LINE_ALIGNED) integer_type _reader_pos{0};
  mutable integer_type _writer_pos_cache{0};
  integer_type _last_flushed_reader_pos{0};
};

} // namespace q_spsc