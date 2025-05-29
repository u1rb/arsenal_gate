// Simple example of templated C++ Parquet writer
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "parquet_ffi/cpp/parquet_writer_cpp.hpp"

int main() {
  try {
    // Define your row structure using std::tuple
    using PersonRow = std::tuple<std::string, int32_t, double>;
    //                           name        age      salary

    // Column names (must match tuple order)
    std::vector<std::string> columns = {"name", "age", "salary"};

    // Create writer with automatic schema generation
    ParquetWriterCpp<PersonRow> writer("simple_example.parquet", columns);

    // Add rows with compile-time type safety
    writer.addRow({"Alice", 30, 75000.0});
    writer.addRow({"Bob", 25, 65000.0});
    writer.addRow({"Charlie", 35, 85000.0});

    // Finalize the file
    writer.flush();
    writer.close();

    std::cout << "Successfully created simple_example.parquet with 3 rows"
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}