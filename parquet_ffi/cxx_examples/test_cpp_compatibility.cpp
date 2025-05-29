// Test C++ compatibility of parquet_ffi headers
#include <iostream>
#include <string>
#include <vector>

// Include the C headers - they should work in C++
#include "parquet_ffi/parquet_reader_stream.h"
#include "parquet_ffi/parquet_stream.h"
#include "parquet_ffi/parquet_writer_stream.h"

int main() {
  std::cout << "Testing C++ compatibility of parquet_ffi headers..."
            << std::endl;

  // Test that we can create basic structures
  WriterOptions options = create_default_writer_options();
  std::cout << "Created WriterOptions with compression: " << options.compression
            << std::endl;

  // Test that we can create column definitions
  ColumnDef col = create_column_def("test_column", ARROW_FORMAT_INT32, false);
  std::cout << "Created ColumnDef: " << col.name
            << " with format: " << col.format << std::endl;

  // Test that we can call the tracing init function
  int result = parquet_ffi_init_tracing();
  std::cout << "Tracing init result: " << result << std::endl;

  std::cout << "All C++ compatibility tests passed!" << std::endl;
  return 0;
}