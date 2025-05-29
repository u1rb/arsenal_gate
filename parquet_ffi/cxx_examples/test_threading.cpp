#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

// Include both the regular and threaded reader headers
#include "parquet_ffi/parquet_reader_stream.h"
#include "parquet_ffi/parquet_reader_stream_threaded.h"

// Simple timer class for performance measurement
class Timer {
private:
  std::chrono::high_resolution_clock::time_point start_time;

public:
  void start() {
    start_time = std::chrono::high_resolution_clock::now();
  }

  double elapsed_ms() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    return duration.count() / 1000.0; // Convert to milliseconds
  }
};

void test_reader_performance(const char *file_path, bool use_threading) {
  std::cout << "\n=== Testing " << (use_threading ? "THREADED" : "SYNCHRONOUS")
            << " Reader ===" << std::endl;

  Timer timer;
  timer.start();

  PacketStream *stream;
  if (use_threading) {
    stream = parquet_reader_init_stream_threaded(file_path);
  } else {
    stream = parquet_reader_init_stream(file_path);
  }

  if (!stream) {
    std::cerr << "Failed to initialize "
              << (use_threading ? "threaded" : "synchronous") << " stream"
              << std::endl;
    return;
  }

  int row_count = 0;
  while (packet_stream_next(stream)) {
    row_count++;
  }

  double elapsed = timer.elapsed_ms();
  double throughput = (row_count / elapsed) * 1000.0; // rows per second

  std::cout << "Results:" << std::endl;
  std::cout << "  Rows processed: " << row_count << std::endl;
  std::cout << "  Time elapsed: " << elapsed << " ms" << std::endl;
  std::cout << "  Throughput: " << static_cast<int>(throughput) << " rows/sec"
            << std::endl;

  packet_stream_release(stream);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <parquet_file>" << std::endl;
    return 1;
  }

  const char *filename = argv[1];

  std::cout << "Threading Performance Test" << std::endl;
  std::cout << "File: " << filename << std::endl;

  // Test synchronous vs threaded performance
  test_reader_performance(filename, false);
  test_reader_performance(filename, true);

  return 0;
}