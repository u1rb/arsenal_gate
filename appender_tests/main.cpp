#include <duckdb.h>
#include <fmt/format.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdlib>
#include <vector>

struct Row {
  int32_t id;
  std::vector<double> values;
};

void AppendRow(duckdb_appender appender, const Row &row) {}

void PrintResult(duckdb_result result) {
  idx_t row_count = duckdb_row_count(&result);
  idx_t column_count = duckdb_column_count(&result);

  for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
    for (idx_t col_idx = 0; col_idx < column_count; col_idx++) {
      char *val_str = duckdb_value_varchar(&result, col_idx, row_idx);
      fmt::println("{}", val_str);
      duckdb_free((void *)val_str);
    }
  }
}

int main() {
  duckdb_database db;
  duckdb_connection connection;
  duckdb_state state;
  duckdb_result result;

  // Open the database in-memory
  state = duckdb_open(NULL, &db);
  if (state == DuckDBError) {
    fmt::println(stderr, "Error opening database");
    exit(1);
  }

  state = duckdb_connect(db, &connection);
  if (state == DuckDBError) {
    fmt::println(stderr, "Error connecting to database");
    exit(1);
  }

  fmt::println("Open and connect to DuckDB successfully");

  // Create a table with a list column
  state = duckdb_query(connection,
                       "CREATE TABLE list_test (id INTEGER, values DOUBLE[])",
                       &result);
  if (state == DuckDBError) {
    fmt::println(stderr, "Error creating table");
    exit(1);
  }
  duckdb_destroy_result(&result);

  // Create an appender for our table
  duckdb_appender appender;
  state = duckdb_appender_create(connection, NULL, "list_test", &appender);
  if (state == DuckDBError) {
    fmt::println(stderr, "Error creating appender");
    exit(1);
  }

  std::vector<Row> rows = {
      {1, {1.1, 2.2, 3.3}},
      {2, {4.4, 5.5}},
  };

  for (const auto &row : rows) {
    AppendRow(appender, row);
  }

  state = duckdb_appender_close(appender);
  if (state == DuckDBError) {
    fmt::println(stderr, "Error closing appender");
    exit(1);
  }

  //   // Verify the data with a query
  //   duckdb_result result;
  //   state = duckdb_query(connection, "SELECT * FROM list_test", &result);
  //   check_error(state, connection);

  //   // Print the results
  //   printf("Table contents after append:\n");
  //   printf("-------------------------\n");

  //   idx_t row_count = duckdb_row_count(&result);
  //   idx_t column_count = duckdb_column_count(&result);

  //   // Print headers
  //   for (idx_t col_idx = 0; col_idx < column_count; col_idx++) {
  //     const char *column_name = duckdb_column_name(&result, col_idx);
  //     printf("%s\t", column_name);
  //   }
  //   printf("\n");

  //   // Print rows
  //   for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
  //     for (idx_t col_idx = 0; col_idx < column_count; col_idx++) {
  //       duckdb_value value = duckdb_value_varchar(&result, col_idx, row_idx);
  //       const char *str_val = duckdb_varchar_cstr(value);
  //       printf("%s\t", str_val);
  //       duckdb_free((void *)str_val);
  //     }
  //     printf("\n");
  //   }

  //   // Clean up
  //   duckdb_destroy_result(&result);
  //   duckdb_destroy_data_chunk(&chunk);
  //   duckdb_destroy_logical_type(&id_type);
  //   duckdb_destroy_logical_type(&double_type);
  //   duckdb_destroy_logical_type(&list_type);
  //   duckdb_free(list_entries);

  //   duckdb_disconnect(&connection);
  //   duckdb_close(&db);

  fmt::println("Done");
  return 0;
}
