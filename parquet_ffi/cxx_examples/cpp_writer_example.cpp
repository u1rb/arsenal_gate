// C++ Example: Writing Parquet files using the parquet_ffi library
// with compile-time templated row structures
#include <iostream>
#include <span>
#include <string>
#include <tuple>
#include <vector>

// Include the templated C++ wrapper
#include "parquet_ffi/cpp/parquet_writer_cpp.hpp"

// Example row structures
struct PersonRow {
  std::string name;
  int32_t age;
  double salary;
  bool is_active;
};

struct DataRow {
  int64_t id;
  std::string description;
  std::span<const uint8_t> binary_data;
  float score;
};

int main() {
  try {
    std::cout << "C++ Templated Parquet Writer Example" << std::endl;

    // Example 1: Person data
    {
      std::cout << "\n=== Writing Person Data ===" << std::endl;

      std::vector<std::string> person_columns = {"name",
                                                 "age",
                                                 "salary",
                                                 "is_active"};
      ParquetWriterCpp<std::tuple<std::string, int32_t, double, bool>>
          person_writer("person_data.parquet", person_columns, 100);

      // Add some sample data using tuple syntax
      std::vector<std::tuple<std::string, int32_t, double, bool>> person_data =
          {{"Alice", 30, 75000.0, true},
           {"Bob", 25, 65000.0, true},
           {"Charlie", 35, 85000.0, false},
           {"Diana", 28, 70000.0, true},
           {"Eve", 32, 80000.0, true}};

      for (const auto &row : person_data) {
        if (!person_writer.addRow(row)) {
          std::cerr << "Failed to add person row" << std::endl;
          return 1;
        }
      }

      if (!person_writer.flush() || !person_writer.close()) {
        std::cerr << "Failed to finalize person writer" << std::endl;
        return 1;
      }

      std::cout << "Successfully wrote " << person_data.size()
                << " person rows to person_data.parquet" << std::endl;
    }

    // Example 2: Data with binary content
    {
      std::cout << "\n=== Writing Data with Binary Content ===" << std::endl;

      std::vector<std::string> data_columns = {"id",
                                               "description",
                                               "binary_data",
                                               "score"};
      ParquetWriterCpp<
          std::tuple<int64_t, std::string, std::span<const uint8_t>, float>>
          data_writer("binary_data.parquet", data_columns, 100);

      // Sample binary data
      std::vector<uint8_t> binary1 = {0x01, 0x02, 0x03, 0x04};
      std::vector<uint8_t> binary2 = {0xFF, 0xFE, 0xFD};
      std::vector<uint8_t> binary3 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

      std::vector<
          std::tuple<int64_t, std::string, std::span<const uint8_t>, float>>
          data_rows = {
              {1001, "First record", std::span<const uint8_t>(binary1), 95.5f},
              {1002, "Second record", std::span<const uint8_t>(binary2), 87.2f},
              {1003, "Third record", std::span<const uint8_t>(binary3), 92.8f}};

      for (const auto &row : data_rows) {
        if (!data_writer.addRow(row)) {
          std::cerr << "Failed to add data row" << std::endl;
          return 1;
        }
      }

      if (!data_writer.flush() || !data_writer.close()) {
        std::cerr << "Failed to finalize data writer" << std::endl;
        return 1;
      }

      std::cout << "Successfully wrote " << data_rows.size()
                << " data rows to binary_data.parquet" << std::endl;
    }

    std::cout << "\nAll examples completed successfully!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}