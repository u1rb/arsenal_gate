#pragma once

#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "parquet_ffi/parquet_stream.h"
#include "parquet_ffi/parquet_writer_stream.h"

// Type traits for mapping C++ types to Arrow format strings
template <typename T> struct arrow_type_traits;

template <> struct arrow_type_traits<bool> {
  static constexpr const char *format = ARROW_FORMAT_BOOL;
};
template <> struct arrow_type_traits<int8_t> {
  static constexpr const char *format = ARROW_FORMAT_INT8;
};
template <> struct arrow_type_traits<uint8_t> {
  static constexpr const char *format = ARROW_FORMAT_UINT8;
};
template <> struct arrow_type_traits<int16_t> {
  static constexpr const char *format = ARROW_FORMAT_INT16;
};
template <> struct arrow_type_traits<uint16_t> {
  static constexpr const char *format = ARROW_FORMAT_UINT16;
};
template <> struct arrow_type_traits<int32_t> {
  static constexpr const char *format = ARROW_FORMAT_INT32;
};
template <> struct arrow_type_traits<uint32_t> {
  static constexpr const char *format = ARROW_FORMAT_UINT32;
};
template <> struct arrow_type_traits<int64_t> {
  static constexpr const char *format = ARROW_FORMAT_INT64;
};
template <> struct arrow_type_traits<uint64_t> {
  static constexpr const char *format = ARROW_FORMAT_UINT64;
};
template <> struct arrow_type_traits<float> {
  static constexpr const char *format = ARROW_FORMAT_FLOAT;
};
template <> struct arrow_type_traits<double> {
  static constexpr const char *format = ARROW_FORMAT_DOUBLE;
};
template <> struct arrow_type_traits<std::string> {
  static constexpr const char *format = ARROW_FORMAT_STRING;
};
template <> struct arrow_type_traits<std::span<const uint8_t>> {
  static constexpr const char *format = ARROW_FORMAT_BINARY;
};

// Helper to check if a type is a primitive numeric type
template <typename T>
constexpr bool is_primitive_numeric_v =
    std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

// Helper to check if a type is string
template <typename T>
constexpr bool is_string_v = std::is_same_v<T, std::string>;

// Helper to check if a type is binary span
template <typename T>
constexpr bool is_binary_span_v = std::is_same_v<T, std::span<const uint8_t>>;

template <typename RowType> class ParquetWriterCpp {
private:
  std::unique_ptr<StreamWriter, decltype(&free_writer)> writer_;
  std::vector<ColumnDef> schema_;

  // Extract column names and types from the row structure at compile time
  template <std::size_t I = 0>
  void build_schema_impl(const std::vector<std::string> &column_names) {
    if constexpr (I < std::tuple_size_v<RowType>) {
      using FieldType = std::tuple_element_t<I, RowType>;

      ColumnDef coldef = create_column_def(column_names[I].c_str(),
                                           arrow_type_traits<FieldType>::format,
                                           true // nullable
      );
      schema_.push_back(coldef);

      build_schema_impl<I + 1>(column_names);
    }
  }

  // Helper to add a single field value to the arrays
  template <std::size_t I>
  void add_field_value(const RowType &row, std::vector<const void *> &values,
                       std::unique_ptr<bool[]> &nulls_array,
                       std::vector<size_t> &sizes,
                       std::vector<std::string> &string_storage,
                       size_t field_index) {
    if constexpr (I < std::tuple_size_v<RowType>) {
      using FieldType = std::tuple_element_t<I, RowType>;
      const auto &field_value = std::get<I>(row);

      if constexpr (is_primitive_numeric_v<FieldType> ||
                    std::is_same_v<FieldType, bool>) {
        // Primitive numeric types and bool
        values[field_index] = &field_value;
        nulls_array[field_index] = false;
        sizes[field_index] = sizeof(FieldType);
      } else if constexpr (is_string_v<FieldType>) {
        // String type
        values[field_index] = field_value.c_str();
        nulls_array[field_index] = false;
        sizes[field_index] = field_value.length();
      } else if constexpr (is_binary_span_v<FieldType>) {
        // Binary span type
        values[field_index] = field_value.data();
        nulls_array[field_index] = false;
        sizes[field_index] = field_value.size();
      } else {
        static_assert(is_primitive_numeric_v<FieldType> ||
                          is_string_v<FieldType> || is_binary_span_v<FieldType>,
                      "Unsupported field type. Use primitive numeric types, "
                      "std::string, or std::span<const uint8_t>");
      }
    }
  }

  // Recursively process all fields in the row
  template <std::size_t I = 0>
  void process_row_fields(const RowType &row, std::vector<const void *> &values,
                          std::unique_ptr<bool[]> &nulls_array,
                          std::vector<size_t> &sizes,
                          std::vector<std::string> &string_storage) {
    if constexpr (I < std::tuple_size_v<RowType>) {
      add_field_value<I>(row, values, nulls_array, sizes, string_storage, I);
      process_row_fields<I + 1>(row, values, nulls_array, sizes,
                                string_storage);
    }
  }

public:
  ParquetWriterCpp(const std::string &filename,
                   const std::vector<std::string> &column_names,
                   size_t batch_size = 1000)
      : writer_(nullptr, free_writer) {
    static_assert(std::tuple_size_v<RowType> > 0,
                  "Row type must have at least one field");

    // Initialize tracing
    parquet_ffi_init_tracing();

    // Validate column names count
    if (column_names.size() != std::tuple_size_v<RowType>) {
      throw std::invalid_argument(
          "Number of column names must match number of fields in row type");
    }

    // Build schema from template parameters
    schema_.reserve(std::tuple_size_v<RowType>);
    build_schema_impl(column_names);

    // Create writer with custom options
    WriterOptions options = create_default_writer_options();
    options.compression = PARQUET_COMPRESSION_SNAPPY;
    options.enable_dictionary = true;
    options.enable_statistics = true;

    StreamWriter *raw_writer = create_writer_with_options(
        filename.c_str(), batch_size, schema_.data(), schema_.size(), options);

    if (!raw_writer) {
      throw std::runtime_error("Failed to create parquet writer");
    }

    writer_.reset(raw_writer);
  }

  // Add a row with compile-time type safety
  bool addRow(const RowType &row) {
    constexpr size_t num_fields = std::tuple_size_v<RowType>;

    // Prepare data for C interface
    std::vector<const void *> values(num_fields);
    std::unique_ptr<bool[]> nulls_array(new bool[num_fields]);
    std::vector<size_t> sizes(num_fields);
    std::vector<std::string>
        string_storage; // For temporary string storage if needed

    // Process all fields in the row
    process_row_fields(row, values, nulls_array, sizes, string_storage);

    return add_row(writer_.get(), values.data(), nulls_array.get(),
                   sizes.data());
  }

  bool flush() { return flush_writer(writer_.get()); }

  bool close() { return close_writer(writer_.get()); }
};