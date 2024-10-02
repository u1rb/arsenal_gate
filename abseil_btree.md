# Abseil B-Tree Order Book: Request, Design, and Technical Specification

## 1. Request Summary

The primary goal is to develop and demonstrate an order book implementation using Abseil's `btree_map` suitable for ultra-low-latency (ULL) trading systems. Key requirements include:

*   **Performance:** Sustain update latencies (insert/erase) of less than 100 nanoseconds on a modern x86_64 CPU (3.0 GHz+), compiled with `-O3 -march=native`.
*   **Data Storage:** Support storing price levels for U.S. stocks.
    *   **Key:** `uint64_t` representing price (decimal number multiplied by 1e9).
    *   **Value:** `LevelInfo` struct containing `uint32_t size` and a pointer to an `order_queue` (e.g., `void*`).
*   **API:** Expose a minimal and ergonomic API:
    *   `insert(price, size, order_queue)`
    *   `erase(price)`
    *   `find(price)`
    *   `find_nth(size_t n)`: Amortized O(1) complexity for accessing the Nth price level.
*   **Lazy Deletion (Optional):** Consider lazy deletion if it improves performance by marking nodes for later reclamation to avoid rebalancing hot nodes. `find_nth` should not include these "empty" or tombstoned nodes.
*   **Deliverables for each candidate solution:**
    *   Brief description (author, memory model, tree order, lazy-deletion strategy).
    *   Example CMake or compile flags.
    *   Minimal code snippet (initialize, insert, delete, iterate).
    *   (Optional) Simple microbenchmark harness outline.

## 2. Design Overview

The proposed solution leverages `absl::btree_map` as the core data structure due to its cache-friendly design and efficiency for ordered data.

### 2.1. Core Data Structure
*   **`absl::btree_map<Key, Value, Compare, Alloc, NodeSize>`:**
    *   **Key:** `uint64_t` (price * 1e9)
    *   **Value:** `LevelInfo` struct
    *   **Compare:** `std::less<uint64_t>` (for transparent key lookups)
    *   **Alloc:** `std::allocator<std::pair<const uint64_t, LevelInfo>>`
    *   **NodeSize:** Default (256 bytes) or configurable.

### 2.2. Value Type: `LevelInfo`
A simple struct to hold the aggregated size at a price level and a pointer to the actual queue of orders.

```cpp
struct LevelInfo {
    uint32_t size;      // Aggregated size at this price level
    void* order_queue;  // Pointer to the detailed order queue (implementation-defined)

    // Constructors
    LevelInfo(uint32_t s = 0, void* oq = nullptr) : size(s), order_queue(oq) {}
};
```

### 2.3. Proposed Variants

1.  **`price_tree_default`**: A type alias for `absl::btree_map` using default settings.
    *   **Memory Model:** Cache-friendly B-tree, fixed 256B leaf/internal nodes (approx. 62 slots per node for `uint64_t` keys and small `LevelInfo` values).
    *   **Lazy Deletion:** No (physical erase).
    *   **Order Statistics for `find_nth`:** Requires an external helper or specific iteration pattern.

2.  **`price_tree_tuned<NodeBytes>`**: A template type alias allowing compile-time configuration of the B-tree node size (e.g., 512B, 1024B). Larger nodes can reduce tree height, potentially improving latency for very large books.
    *   **Benefit:** Can improve cache locality and reduce tree traversals. A 512B node size is often a good starting point for tuning.

3.  **`price_tree_lazy`**: A wrapper class around a (typically tuned) `absl::btree_map` that implements lazy deletion.
    *   **Lazy Strategy:** Entries are marked as "tombstones" (e.g., `LevelInfo.size = 0`) instead of being physically removed immediately.
    *   **Internal State:** Maintains a `logical_size_` counter for live entries.
    *   **Compaction:** A mechanism (`compact_if_needed()`) can be implemented to physically remove tombstones when their count exceeds a threshold, reclaiming memory and improving iteration performance. This is a conceptual placeholder in the provided code.
    *   **`find_nth`:** Must be adapted to skip tombstone entries.

### 2.4. `find_nth` Strategy
Achieving amortized O(1) for `find_nth` in a B-tree typically involves:
*   Caching the iterator and rank (index) of the last accessed element.
*   Exploiting the B-tree's property that elements within a node are stored contiguously. If the next requested `n` is within the same node as the cached iterator, access is O(1).
*   If `n` is outside the cached node, a B-tree traversal (typically O(log N) or O(distance) via `std::next`) is needed, after which the cache is updated.
*   For `price_tree_lazy`, the iteration must also skip tombstones.

## 3. Technical Specifications

*   **Key Type:** `uint64_t`
*   **Value Type:** `LevelInfo { uint32_t size; void* order_queue; }`
*   **API Methods & Signatures:**
    *   `void insert(uint64_t price, uint32_t size, void* order_queue)`
    *   `void erase(uint64_t price)`
    *   `iterator find(uint64_t price)` (returns `btree_map::iterator`)
    *   `iterator find_nth(size_t n)` (returns `btree_map::iterator`)
*   **Performance Target:** < 100ns median update latency (inserts/erases) on target hardware.
*   **Lazy Deletion Details (`price_tree_lazy`):**
    *   Tombstone: `LevelInfo.size == 0`.
    *   `logical_size_` tracks live entries.
    *   Compaction logic is conceptual and would require a specific strategy (e.g., periodic rebuild or scan-and-erase).
*   **Compilation:**
    *   C++ Standard: C++17 or C++20 recommended for `absl`.
    *   Flags: `-O3 -pipe -flto -march=native -DNDEBUG`
*   **Dependencies:** Abseil C++ Libraries (specifically `absl::btree`).

## 4. Code Implementation

### 4.1. `LevelInfo` Struct
```cpp
#include <cstdint> // For uint32_t, uint64_t
#include <cstddef> // For void*

struct LevelInfo {
    uint32_t size;
    void* order_queue;

    LevelInfo(uint32_t s = 0, void* oq = nullptr) : size(s), order_queue(oq) {}
};
```

### 4.2. `price_tree_default`
```cpp
#include "absl/container/btree_map.h"
#include <functional> // For std::less
#include <utility>    // For std::pair

// Assumes LevelInfo is defined as above

using price_tree_default = absl::btree_map<
    uint64_t,                          // price * 1e9
    LevelInfo,                         // size + queue ptr
    std::less<uint64_t>,               // transparent comparator
    std::allocator<std::pair<const uint64_t, LevelInfo>>
    // Default node size (256 bytes)
>;

// --- Example Usage ---
// price_tree_default book;
// struct OrderQueueType {}; // Dummy type for example
// OrderQueueType queueA, queueB;
//
// book.insert_or_assign(125123450000ULL, LevelInfo{100, &queueA});
// auto it = book.find(125123450000ULL);
// if (it != book.end()) { /* ... */ }
// book.erase(125123450000ULL);
//
// // For find_nth, a helper like nth_cached (see below) would be used.
// // auto nth_it = nth_cached(book, 0);
```

### 4.3. `price_tree_tuned<NodeBytes>`
```cpp
#include "absl/container/btree_map.h"
#include <functional> // For std::less
#include <utility>    // For std::pair

// Assumes LevelInfo is defined as above

template<int NodeBytes = 512> // Default to 512 bytes for tuned version
using price_tree_tuned = absl::btree_map<
    uint64_t,                                         // price * 1e9
    LevelInfo,                                        // size + queue ptr
    std::less<uint64_t>,                              // transparent comparator
    std::allocator<std::pair<const uint64_t, LevelInfo>>,
    NodeBytes                                         // Configurable node size
>;

// --- Example Usage ---
// price_tree_tuned<1024> large_node_book; // Example with 1KB nodes
// struct OrderQueueType {}; // Dummy type
// OrderQueueType queue_instance;
// large_node_book.insert_or_assign(126000000000ULL, LevelInfo{50, &queue_instance});
```

### 4.4. `price_tree_lazy`
```cpp
#include "absl/container/btree_map.h"
#include <cstdint>
#include <cstddef>
#include <functional>
#include <utility>
#include <iterator> // For std::next in potential nth_cached

// Assumes LevelInfo and price_tree_tuned (as default Tree type) are defined.
// Placeholder for nth_cached helper - see section 4.5
template<class Btree>
typename Btree::iterator nth_cached(Btree& bt, size_t n, bool skip_tombstones = false);


template<class Tree = price_tree_tuned<512>> // Uses tuned btree by default
class price_tree_lazy {
    Tree t_;
    size_t logical_size_ = 0; // Number of live (non-tombstone) entries
    // size_t tombstones_threshold_ = 64; // Example threshold for compaction

public:
    price_tree_lazy() = default;

    using iterator = typename Tree::iterator;
    using const_iterator = typename Tree::const_iterator;

    void insert(uint64_t price, uint32_t size, void* order_queue) {
        if (size == 0) { // Inserting a tombstone or zero-size level is an erase
            erase(price);
            return;
        }

        auto it_existing = t_.find(price);
        bool was_live_before = false;
        if (it_existing != t_.end() && it_existing->second.size > 0) {
            was_live_before = true;
        }

        t_.insert_or_assign(price, LevelInfo{size, order_queue});

        if (!was_live_before) { // If it was not live (new or tombstone) and now is live
            logical_size_++;
        }
        // If it was live and remains live, logical_size_ is unchanged.
    }

    void erase(uint64_t price) {
        auto it = t_.find(price);
        if (it != t_.end()) {
            if (it->second.size > 0) { // If it was a live entry
                logical_size_--;
            }
            it->second.size = 0; // Mark as tombstone
            
            // Conceptual: if (++tombstones_count_ > tombstones_threshold_) compact_if_needed();
        }
    }

    iterator find(uint64_t price) {
        auto it = t_.find(price);
        if (it != t_.end() && it->second.size == 0) { // It's a tombstone
            return t_.end(); // Do not return tombstones via find
        }
        return it;
    }
    
    const_iterator find(uint64_t price) const {
        auto it = t_.find(price);
        if (it != t_.end() && it->second.size == 0) {
            return t_.end();
        }
        return it;
    }

    iterator find_nth(size_t n) {
        if (n >= logical_size_) {
            return t_.end(); 
        }
        // nth_cached MUST be implemented to skip tombstones for price_tree_lazy
        return nth_cached(t_, n, true /* skip_tombstones */); 
    }

    size_t size() const {
        return logical_size_;
    }

    // Iterators: begin()/end() will traverse all elements, including tombstones.
    // Filtering iterators might be needed for live-only iteration.
    iterator begin() { return t_.begin(); }
    iterator end() { return t_.end(); }
    const_iterator cbegin() const { return t_.cbegin(); }
    const_iterator cend() const { return t_.cend(); }

private:
    void compact_if_needed() {
        // Placeholder: Physically remove tombstoned entries.
        // For example, iterate and rebuild into a new tree:
        // Tree new_tree;
        // logical_size_ = 0;
        // for(const auto& pair_val : t_) {
        //    if (pair_val.second.size > 0) { // Live entry
        //        new_tree.insert(pair_val);
        //        logical_size_++;
        //    }
        // }
        // t_ = std::move(new_tree);
    }
};
```

### 4.5. `nth_cached` Helper (Conceptual)
```cpp
// Conceptual template for nth_cached.
// Needs careful implementation for performance and tombstone skipping.
template<class Btree>
typename Btree::iterator nth_cached(Btree& bt, size_t n, bool skip_tombstones = false) {
    // Simplified version: always iterates from beginning.
    // A real implementation would use static thread_local caching.
    auto it = bt.begin();
    size_t count = 0;
    size_t live_elements_passed = 0;

    while (it != bt.end()) {
        bool is_live = true;
        if (skip_tombstones) {
            // Assumes LevelInfo structure for value_type
            if constexpr (std::is_same_v<typename Btree::mapped_type, LevelInfo>) {
                 if (it->second.size == 0) { // Tombstone
                     is_live = false;
                 }
            }
        }

        if (is_live) {
            if (live_elements_passed == n) {
                return it;
            }
            live_elements_passed++;
        }
        ++it;
        // count++; // Total elements passed if needed for caching logic
    }
    return bt.end(); // Nth live element not found
}
```

### 4.6. Microbenchmark Harness (Outline)
```cpp
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cstdio>

#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h> // For __rdtsc
#define HAS_RDTSC 1
#else
#define HAS_RDTSC 0
#endif

static inline uint64_t get_cycles() {
#if HAS_RDTSC
    return __rdtsc();
#else
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

// Generic benchmark function (example)
template<class Book, class Operation>
void run_benchmark(const std::string& name, Book& book, Operation op, 
                   size_t num_ops, double cpu_ghz = 3.4, bool use_rdtsc = HAS_RDTSC) {
    std::vector<uint64_t> latencies_cycles;
    latencies_cycles.reserve(num_ops);

    // Warm-up (optional)
    for (size_t i = 0; i < num_ops / 10; ++i) {
        op(book, i); // op should take book and an index/key
    }
    // Consider clearing/resetting book state if ops are stateful

    for (size_t i = 0; i < num_ops; ++i) {
        uint64_t start_cycles = get_cycles();
        op(book, i + (num_ops / 10)); // Use different keys for actual measurement
        uint64_t end_cycles = get_cycles();
        latencies_cycles.push_back(end_cycles - start_cycles);
    }

    std::sort(latencies_cycles.begin(), latencies_cycles.end());

    if (num_ops == 0) {
        printf("%s: No operations to benchmark.\n", name.c_str());
        return;
    }
    
    double median_cycles = (num_ops % 2 != 0) ? 
                           (double)latencies_cycles[num_ops / 2] :
                           ((double)latencies_cycles[num_ops / 2 - 1] + (double)latencies_cycles[num_ops / 2]) / 2.0;
    
    double p99_cycles = (double)latencies_cycles[static_cast<size_t>(num_ops * 0.99)];

    if (use_rdtsc && HAS_RDTSC) {
        printf("%s: Median Latency: %.2f ns, P99 Latency: %.2f ns (CPU: %.2f GHz)\n",
               name.c_str(), median_cycles / cpu_ghz, p99_cycles / cpu_ghz, cpu_ghz);
    } else {
        // If not using RDTSC, cycles are likely nanoseconds from chrono
        printf("%s: Median Latency: %.0f ns, P99 Latency: %.0f ns (using std::chrono)\n",
               name.c_str(), median_cycles, p99_cycles);
    }
}

// --- Example main for benchmark ---
// int main() {
//     price_tree_tuned<512> book_tuned;
//     // price_tree_lazy<> book_lazy; 
//     struct OrderQueueType {}; OrderQueueType dummy_oq;
//     const size_t N_OPS = 100000; // 100k operations

//     auto insert_op_lambda = [&](auto& current_book, size_t i) {
//         current_book.insert(100000000000ULL + i, (uint32_t)(i % 100 + 1), &dummy_oq);
//     };
//     run_benchmark("TunedInsert", book_tuned, insert_op_lambda, N_OPS);
    
//     // Add benchmarks for erase, find, find_nth etc.
//     // Remember to manage book state between benchmarks (e.g., pre-fill for erase tests).
//     return 0;
// }
```

### 4.7. CMake / Compilation Flags
```cmake
# CMakeLists.txt (partial)
cmake_minimum_required(VERSION 3.10)
project(OrderBookDemo CXX)

set(CMAKE_CXX_STANDARD 17) # Or 20
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add Abseil (e.g., via FetchContent or find_package if installed system-wide)
# Example using FetchContent:
include(FetchContent)
FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20230802.1 # Use a recent stable tag
)
FetchContent_MakeAvailable(abseil-cpp)

add_executable(order_book_demo main.cpp) # Assuming your main file is main.cpp

target_compile_options(order_book_demo PRIVATE
  -O3
  -pipe
  -flto # Link-Time Optimization
  -march=native
  -DNDEBUG # Disables asserts
  # -Wall -Wextra -pedantic # Recommended for development
)

# Link Abseil B-tree library
target_link_libraries(order_book_demo PRIVATE absl::btree)
```

## 5. Practical Recommendations

1.  **Start with `price_tree_tuned<512>`:** This generally provides a good balance of latency and memory footprint. Benchmark with different node sizes (e.g., 256, 512, 1024) to find the optimal for your specific workload and data distribution.
2.  **`price_tree_lazy` for High Churn:** Consider lazy deletion only if delete operations are very frequent for specific symbols and the occasional latency spike from compaction is acceptable. The complexity of `find_nth` and iterators also increases.
3.  **NUMA Considerations:** For multi-socket systems, pin threads processing specific order books to CPU cores on the same NUMA node as the memory where the book resides to minimize cross-node memory access latencies.
4.  **Compilation:** Always build release versions with full optimizations (`-O3`, `-flto`, `-march=native`, `-DNDEBUG`).
5.  **Measure and Profile:** Use the microbenchmark harness (or a more sophisticated one) on target hardware. Profile to identify bottlenecks. `perf` on Linux is invaluable.
6.  **`find_nth` Implementation:** The O(1) amortized `find_nth` requires careful implementation of the caching strategy, especially with lazy deletion. Thoroughly test its correctness and performance.
7.  **Abseil Version:** Keep Abseil updated, as newer versions may contain performance improvements or bug fixes.

## 6. Conclusion

Abseil's `btree_map` provides a strong foundation for building a high-performance order book. By tuning node sizes and optionally implementing strategies like lazy deletion, it's feasible to meet sub-100ns latency targets. Careful benchmarking and profiling on the target system are crucial for validating and optimizing the final implementation.
