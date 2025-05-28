// =============================================================================
// Parquet Reader Stream Demo - Using Simplified API
// =============================================================================
//
// Demonstrates zero-copy Parquet reading with:
// - Single file reading and multi-file merging
// - High-performance data verification
// - Memory-efficient streaming
//
// Usage:
//   ./d_read_stream file.parquet                    # Single file
//   ./d_read_stream file1.parquet file2.parquet    # Multi-file merge
//
// =============================================================================

#include <parquet_ffi/parquet_reader_stream.h>
#include <sys/stat.h>
#include <sys/time.h>

// =============================================================================
// Configuration and Constants
// =============================================================================

#define MAX_DISPLAY_ROWS 10
#define SAMPLE_ROWS 1000
#define PROGRESS_INTERVAL 10000000

// =============================================================================
// Data Verification
// =============================================================================

// Fast checksum for data verification using simplified API
static uint64_t verify_row_data(PacketStream *stream) {
  uint64_t checksum = 0;
  int num_columns = packet_stream_get_column_count(stream);

  for (int i = 0; i < num_columns; i++) {
    const char *format = packet_stream_get_column_format(stream, i);

    if (packet_stream_is_null(stream, i)) {
      checksum = checksum * 31 + 0xDEADBEEF;
    } else if (strcmp(format, "i") == 0) {
      checksum =
          checksum * 31 + (uint64_t) packet_stream_get_value_int32(stream, i);
    } else if (strcmp(format, "l") == 0) {
      checksum =
          checksum * 31 + (uint64_t) packet_stream_get_value_int64(stream, i);
    } else if (strcmp(format, "L") == 0) {
      checksum = checksum * 31 + packet_stream_get_value_uint64(stream, i);
    } else if (strcmp(format, "g") == 0) {
      double value = packet_stream_get_value_double(stream, i);
      checksum = checksum * 31 + *(uint64_t *) &value;
    } else if (strcmp(format, "f") == 0) {
      float value = packet_stream_get_value_float(stream, i);
      checksum = checksum * 31 + *(uint32_t *) &value;
    } else if (strcmp(format, "u") == 0 || strcmp(format, "U") == 0) {
      // Zero-copy string verification
      const char *str;
      size_t str_len;
      if (packet_stream_get_string_zerocopy(stream, i, &str, &str_len) && str) {
        checksum = checksum * 31 + str_len + (uint64_t) str[0];
      }
    } else if (strcmp(format, "z") == 0 || strcmp(format, "Z") == 0) {
      // Zero-copy binary verification
      const uint8_t *data;
      size_t data_len;
      if (packet_stream_get_binary_zerocopy(stream, i, &data, &data_len) &&
          data) {
        checksum = checksum * 31 + data_len + (uint64_t) data[0];
      }
    }
  }

  return checksum;
}

// Validate data consistency using simplified API
static bool validate_data_consistency(PacketStream *stream,
                                      int expected_id,
                                      bool is_merger) {
  int32_t actual_id = packet_stream_get_value_int32(stream, 0);
  return is_merger ? (actual_id >= 0) : (actual_id == expected_id);
}

// =============================================================================
// Display and Formatting
// =============================================================================

// Print schema information
static void print_schema(PacketStream *stream) {
  int num_columns = packet_stream_get_column_count(stream);
  printf("\nSchema (%d columns):\n", num_columns);

  for (int i = 0; i < num_columns; i++) {
    const char *name = packet_stream_get_column_name(stream, i);
    const char *format = packet_stream_get_column_format(stream, i);
    printf("  Column %d: %s (%s)\n",
           i,
           name ? name : "(unnamed)",
           format ? format : "(unknown)");
  }
}

// Display a single row using simplified API
static void display_row(PacketStream *stream,
                        int row_num,
                        uint64_t row_checksum) {
  printf("Row %d: ", row_num);
  int num_columns = packet_stream_get_column_count(stream);

  for (int i = 0; i < num_columns; i++) {
    const char *format = packet_stream_get_column_format(stream, i);

    if (packet_stream_is_null(stream, i)) {
      printf("NULL, ");
    } else if (strcmp(format, "i") == 0) {
      printf("%d, ", packet_stream_get_value_int32(stream, i));
    } else if (strcmp(format, "l") == 0) {
      printf("%ld, ", packet_stream_get_value_int64(stream, i));
    } else if (strcmp(format, "L") == 0) {
      printf("%lu, ", packet_stream_get_value_uint64(stream, i));
    } else if (strcmp(format, "g") == 0) {
      printf("%g, ", packet_stream_get_value_double(stream, i));
    } else if (strcmp(format, "f") == 0) {
      printf("%g, ", packet_stream_get_value_float(stream, i));
    } else if (strcmp(format, "u") == 0 || strcmp(format, "U") == 0) {
      const char *str;
      size_t str_len;
      if (packet_stream_get_string_zerocopy(stream, i, &str, &str_len)) {
        printf(str ? "\"%.*s\", " : "NULL, ", (int) str_len, str);
      } else {
        printf("ERROR, ");
      }
    } else if (strcmp(format, "z") == 0 || strcmp(format, "Z") == 0) {
      const uint8_t *data;
      size_t data_len;
      if (packet_stream_get_binary_zerocopy(stream, i, &data, &data_len)) {
        printf("[%zu bytes], ", data_len);
      } else {
        printf("ERROR, ");
      }
    } else {
      printf("[%s], ", format);
    }
  }
  printf("(checksum: 0x%lx)\n", row_checksum);
}

// =============================================================================
// Performance Measurement
// =============================================================================

// Calculate row size using zero-copy access and simplified API
static size_t calculate_row_size(PacketStream *stream) {
  size_t row_size = 0;
  int num_columns = packet_stream_get_column_count(stream);

  for (int i = 0; i < num_columns; i++) {
    const char *format = packet_stream_get_column_format(stream, i);

    if (packet_stream_is_null(stream, i)) {
      row_size += 1; // Validity bit
    } else if (strcmp(format, "i") == 0 || strcmp(format, "I") == 0) {
      row_size += 4;
    } else if (strcmp(format, "l") == 0 || strcmp(format, "L") == 0) {
      row_size += 8;
    } else if (strcmp(format, "g") == 0) {
      row_size += 8;
    } else if (strcmp(format, "f") == 0) {
      row_size += 4;
    } else if (strcmp(format, "u") == 0 || strcmp(format, "U") == 0) {
      const char *str;
      size_t str_len;
      if (packet_stream_get_string_zerocopy(stream, i, &str, &str_len)) {
        row_size += str_len;
      }
    } else if (strcmp(format, "z") == 0 || strcmp(format, "Z") == 0) {
      const uint8_t *data;
      size_t data_len;
      if (packet_stream_get_binary_zerocopy(stream, i, &data, &data_len)) {
        row_size += data_len;
      }
    } else {
      row_size += 8; // Default size
    }
  }

  return row_size;
}

// Get total file size
static long long get_total_file_size(const char **file_paths,
                                     uint32_t num_files) {
  long long total_size = 0;
  struct stat st;

  for (uint32_t i = 0; i < num_files; i++) {
    if (stat(file_paths[i], &st) == 0) {
      total_size += st.st_size;
    }
  }

  return total_size;
}

// =============================================================================
// Main Processing Logic
// =============================================================================

// Process stream and collect statistics
static void process_stream(PacketStream *stream,
                           const char **file_paths,
                           uint32_t num_files) {
  bool is_merger = (num_files > 1);

  // Print schema
  print_schema(stream);

  // Initialize counters
  int row_count = 0;
  size_t total_data_size = 0;
  int sample_count = 0;
  uint64_t total_checksum = 0;
  int verification_errors = 0;
  int expected_id = 0;
  int last_id = -1;

  printf("\nData (first %d rows):\n", MAX_DISPLAY_ROWS);

  struct timeval start, end;
  gettimeofday(&start, NULL);

  // Process all rows
  while (packet_stream_next(stream)) {
    row_count++;

    // Verify data integrity
    uint64_t row_checksum = verify_row_data(stream);
    total_checksum = total_checksum * 31 + row_checksum;

    // Validate data consistency
    if (!is_merger) {
      if (!validate_data_consistency(stream, expected_id, false)) {
        verification_errors++;
      }
      expected_id++;
    } else {
      // For merger, check ordering
      int32_t current_id = packet_stream_get_value_int32(stream, 0);
      if (current_id < last_id) {
        verification_errors++;
      }
      last_id = current_id;
    }

    // Display first few rows
    if (row_count <= MAX_DISPLAY_ROWS) {
      display_row(stream, row_count, row_checksum);
    }

    // Sample data size calculation
    if (sample_count < SAMPLE_ROWS) {
      total_data_size += calculate_row_size(stream);
      sample_count++;
    }

    // Progress reporting
    if (row_count % PROGRESS_INTERVAL == 0) {
      printf("Processed %d rows...\n", row_count);
    }
  }

  gettimeofday(&end, NULL);

  // Calculate performance metrics
  double elapsed =
      (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
  double rows_per_sec = row_count / elapsed;

  // Estimate throughput
  double avg_row_size =
      sample_count > 0 ? (double) total_data_size / sample_count : 0;
  double mb_per_sec = (rows_per_sec * avg_row_size) / (1024 * 1024);

  // Get file size for comparison
  long long file_size = get_total_file_size(file_paths, num_files);
  double file_mb = file_size / (1024.0 * 1024.0);

  // Print results
  printf("\n=== DATA VERIFICATION RESULTS ===\n");
  printf("Total data checksum: 0x%lx\n", total_checksum);
  printf("Verification errors: %d\n", verification_errors);
  printf("%s All data verification checks PASSED\n",
         verification_errors == 0 ? "✅" : "❌");

  printf("\n=== PERFORMANCE SUMMARY ===\n");
  printf("Processed %d rows in %.3f seconds (%.1f rows/sec, %.2f MB/s)\n",
         row_count,
         elapsed,
         rows_per_sec,
         mb_per_sec);
  printf("File size: %.2f MB, Average row size: %.1f bytes\n",
         file_mb,
         avg_row_size);

  if (is_merger) {
    printf("Multi-file merge: %d files processed\n", num_files);
  }
}

// =============================================================================
// Main Program
// =============================================================================

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <file1.parquet> [file2.parquet] [file3.parquet] ...\n",
           argv[0]);
    printf("  Single file: reads and verifies one Parquet file\n");
    printf("  Multiple files: merges files in sorted order by first column\n");
    return 1;
  }

  const char **file_paths = (const char **) &argv[1];
  uint32_t num_files = argc - 1;

  printf("=== Parquet Reader Stream Demo (Simplified API) ===\n");
  if (num_files == 1) {
    printf("Reading single file: %s\n", file_paths[0]);
  } else {
    printf("Merging %d files:\n", num_files);
    for (uint32_t i = 0; i < num_files; i++) {
      printf("  %d. %s\n", i + 1, file_paths[i]);
    }
  }

  // Initialize stream
  PacketStream *stream;
  if (num_files == 1) {
    stream = parquet_reader_init_stream(file_paths[0]);
  } else {
    stream = parquet_merger_init_stream(file_paths, num_files, 0);
  }

  if (!stream) {
    fprintf(stderr, "Failed to initialize stream\n");
    return 1;
  }

  // Process the stream
  process_stream(stream, file_paths, num_files);

  // Cleanup
  packet_stream_release(stream);

  printf("\n=== Demo completed successfully! ===\n");
  return 0;
}