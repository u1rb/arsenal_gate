// =============================================================================
// Parquet Reader Stream Benchmark Tool
// =============================================================================
//
// A comprehensive benchmark tool for testing Parquet reading performance with:
// - Configurable workload simulation (light/heavy processing)
// - Batch size control for both synchronous and threaded readers
// - Support for real-world file sizes (100M-10B rows)
// - Detailed performance metrics and analysis
//
// Usage:
//   ./a1_read_stream [options] <file1.parquet> [file2.parquet] ...
//
// Options:
//   --threaded              Enable multi-threaded batch prefetching
//   --workload=<light|heavy> Set processing workload (default: light)
//   --iterations=<n>        Number of checksum iterations for heavy workload
//   (default: 100)
//   --batch-size=<n>        Set batch size for reading (default: 65536)
//   --no-display            Skip displaying first rows
//   --quiet                 Minimal output (only final metrics)
//   --help                  Show this help message
//
// Examples:
//   ./a1_read_stream --threaded --workload=heavy --batch-size=100000
//   large.parquet
//   ./a1_read_stream --workload=light --iterations=10 file1.parquet
//   file2.parquet
//
// =============================================================================

#include <getopt.h>
#include <parquet_ffi/parquet_reader_stream.h>
#include <parquet_ffi/parquet_reader_stream_threaded.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

// =============================================================================
// Configuration and Constants
// =============================================================================

#define MAX_DISPLAY_ROWS 10
#define SAMPLE_ROWS 1000
#define PROGRESS_INTERVAL 10000000
#define DEFAULT_BATCH_SIZE 65536
#define DEFAULT_ITERATIONS 100

// Workload types
typedef enum { WORKLOAD_LIGHT, WORKLOAD_HEAVY } WorkloadType;

// Configuration structure
typedef struct {
  bool use_threading;
  WorkloadType workload;
  int iterations;
  int batch_size;
  bool display_rows;
  bool quiet_mode;
} BenchmarkConfig;

// =============================================================================
// Data Verification with Configurable Workload
// =============================================================================

// Light workload - single checksum calculation
static uint64_t calculate_checksum_light(PacketStream *stream) {
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
      const char *str;
      size_t str_len;
      if (packet_stream_get_string_zerocopy(stream, i, &str, &str_len) && str) {
        checksum = checksum * 31 + str_len + (uint64_t) str[0];
      }
    } else if (strcmp(format, "z") == 0 || strcmp(format, "Z") == 0) {
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

// Heavy workload - multiple checksum iterations to simulate CPU-intensive
// processing
static uint64_t calculate_checksum_heavy(PacketStream *stream, int iterations) {
  uint64_t checksum = 0;

  // Perform the checksum calculation multiple times
  for (int iter = 0; iter < iterations; iter++) {
    uint64_t iter_checksum = calculate_checksum_light(stream);

    // Mix in the iteration number to ensure different calculations
    checksum = checksum * 37 + iter_checksum * (iter + 1);

    // Additional CPU work: some mathematical operations
    checksum = ((checksum << 13) | (checksum >> 51)) ^ (checksum * 0x5bd1e995);
  }

  return checksum;
}

// Main verification function that dispatches based on workload type
static uint64_t verify_row_data(PacketStream *stream,
                                const BenchmarkConfig *config) {
  if (config->workload == WORKLOAD_HEAVY) {
    return calculate_checksum_heavy(stream, config->iterations);
  } else {
    return calculate_checksum_light(stream);
  }
}

// Validate data consistency
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
static void print_schema(PacketStream *stream, const BenchmarkConfig *config) {
  if (config->quiet_mode)
    return;

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

// Display a single row
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

// Calculate row size using zero-copy access
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

// Categorize file size
static const char *categorize_file_size(int64_t row_count) {
  if (row_count >= 1000000000) { // 1B+ rows
    return "Extra Large (1B+ rows)";
  } else if (row_count >= 200000000) { // 200M+ rows
    return "Large (200M+ rows)";
  } else if (row_count >= 10000000) { // 10M+ rows
    return "Medium (10M-200M rows)";
  } else if (row_count >= 1000000) { // 1M+ rows
    return "Small (1M-10M rows)";
  } else {
    return "Tiny (<1M rows)";
  }
}

// =============================================================================
// Main Processing Logic
// =============================================================================

// Process stream and collect statistics
static void process_stream(PacketStream *stream,
                           const char **file_paths,
                           uint32_t num_files,
                           const BenchmarkConfig *config) {
  bool is_merger = (num_files > 1);

  // Print schema
  print_schema(stream, config);

  // Initialize counters
  int64_t row_count = 0;
  size_t total_data_size = 0;
  int sample_count = 0;
  uint64_t total_checksum = 0;
  int verification_errors = 0;
  int expected_id = 0;
  int last_id = -1;

  if (!config->quiet_mode && config->display_rows) {
    printf("\nData (first %d rows):\n", MAX_DISPLAY_ROWS);
  }

  struct timeval start, end;
  gettimeofday(&start, NULL);

  // Process all rows
  while (packet_stream_next(stream)) {
    row_count++;

    // Verify data integrity with configurable workload
    uint64_t row_checksum = verify_row_data(stream, config);
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
    if (!config->quiet_mode && config->display_rows &&
        row_count <= MAX_DISPLAY_ROWS) {
      display_row(stream, row_count, row_checksum);
    }

    // Sample data size calculation
    if (sample_count < SAMPLE_ROWS) {
      total_data_size += calculate_row_size(stream);
      sample_count++;
    }

    // Progress reporting
    if (!config->quiet_mode && row_count % PROGRESS_INTERVAL == 0) {
      printf("Processed %lld rows...\n", (long long) row_count);
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
  if (!config->quiet_mode) {
    printf("\n=== DATA VERIFICATION RESULTS ===\n");
    printf("Total data checksum: 0x%lx\n", total_checksum);
    printf("Verification errors: %d\n", verification_errors);
    printf("%s All data verification checks PASSED\n",
           verification_errors == 0 ? "✅" : "❌");
  }

  printf("\n=== PERFORMANCE SUMMARY ===\n");
  printf("File category: %s\n", categorize_file_size(row_count));
  printf("Processed %lld rows in %.3f seconds (%.1f rows/sec, %.2f MB/s)\n",
         (long long) row_count,
         elapsed,
         rows_per_sec,
         mb_per_sec);
  printf("File size: %.2f MB, Average row size: %.1f bytes\n",
         file_mb,
         avg_row_size);

  if (is_merger) {
    printf("Multi-file merge: %d files processed\n", num_files);
  }

  printf("\n=== BENCHMARK CONFIGURATION ===\n");
  printf("Threading mode: %s\n",
         config->use_threading ? "ENABLED" : "DISABLED");
  printf("Workload type: %s\n",
         config->workload == WORKLOAD_HEAVY ? "HEAVY" : "LIGHT");
  if (config->workload == WORKLOAD_HEAVY) {
    printf("Checksum iterations: %d\n", config->iterations);
  }
  printf("Batch size: %d rows\n", config->batch_size);

  // Calculate and display processing time per row
  double us_per_row = (elapsed * 1000000.0) / row_count;
  printf("\n=== DETAILED METRICS ===\n");
  printf("Processing time per row: %.3f μs\n", us_per_row);
  printf("Batch processing rate: %.1f batches/sec\n",
         (double) row_count / config->batch_size / elapsed);

  // Estimate scaling for larger files
  if (!config->quiet_mode && row_count < 200000000) {
    printf("\n=== SCALING ESTIMATES ===\n");
    printf("Estimated time for 200M rows: %.1f seconds\n",
           200000000.0 / rows_per_sec);
    printf("Estimated time for 1B rows: %.1f seconds (%.1f minutes)\n",
           1000000000.0 / rows_per_sec,
           1000000000.0 / rows_per_sec / 60.0);
    printf("Estimated time for 10B rows: %.1f seconds (%.1f hours)\n",
           10000000000.0 / rows_per_sec,
           10000000000.0 / rows_per_sec / 3600.0);
  }
}

// =============================================================================
// Command Line Parsing
// =============================================================================

static void print_usage(const char *program_name) {
  printf("Usage: %s [options] <file1.parquet> [file2.parquet] ...\n",
         program_name);
  printf("\nOptions:\n");
  printf("  --threaded              Enable multi-threaded batch prefetching\n");
  printf(
      "  --workload=<light|heavy> Set processing workload (default: light)\n");
  printf("  --iterations=<n>        Number of checksum iterations for heavy "
         "workload (default: %d)\n",
         DEFAULT_ITERATIONS);
  printf("  --batch-size=<n>        Set batch size for reading (default: %d)\n",
         DEFAULT_BATCH_SIZE);
  printf("  --no-display            Skip displaying first rows\n");
  printf("  --quiet                 Minimal output (only final metrics)\n");
  printf("  --help                  Show this help message\n");
  printf("\nExamples:\n");
  printf("  %s --threaded --workload=heavy --batch-size=100000 large.parquet\n",
         program_name);
  printf("  %s --workload=light --iterations=10 file1.parquet file2.parquet\n",
         program_name);
  printf("\nFile size categories:\n");
  printf("  Tiny: <1M rows\n");
  printf("  Small: 1M-10M rows\n");
  printf("  Medium: 10M-200M rows\n");
  printf("  Large: 200M+ rows (real-world size)\n");
  printf("  Extra Large: 1B+ rows\n");
}

static int parse_arguments(int argc,
                           char *argv[],
                           BenchmarkConfig *config,
                           int *file_arg_start) {
  // Initialize default configuration
  config->use_threading = false;
  config->workload = WORKLOAD_LIGHT;
  config->iterations = DEFAULT_ITERATIONS;
  config->batch_size = DEFAULT_BATCH_SIZE;
  config->display_rows = true;
  config->quiet_mode = false;

  static struct option long_options[] = {
      {"threaded", no_argument, 0, 't'},
      {"workload", required_argument, 0, 'w'},
      {"iterations", required_argument, 0, 'i'},
      {"batch-size", required_argument, 0, 'b'},
      {"no-display", no_argument, 0, 'n'},
      {"quiet", no_argument, 0, 'q'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int option_index = 0;
  int c;

  while ((c = getopt_long(
              argc, argv, "tw:i:b:nqh", long_options, &option_index)) != -1) {
    switch (c) {
      case 't':
        config->use_threading = true;
        break;
      case 'w':
        if (strcmp(optarg, "light") == 0) {
          config->workload = WORKLOAD_LIGHT;
        } else if (strcmp(optarg, "heavy") == 0) {
          config->workload = WORKLOAD_HEAVY;
        } else {
          fprintf(stderr, "Invalid workload type: %s\n", optarg);
          return -1;
        }
        break;
      case 'i':
        config->iterations = atoi(optarg);
        if (config->iterations <= 0) {
          fprintf(stderr, "Invalid iterations: %s\n", optarg);
          return -1;
        }
        break;
      case 'b':
        config->batch_size = atoi(optarg);
        if (config->batch_size <= 0) {
          fprintf(stderr, "Invalid batch size: %s\n", optarg);
          return -1;
        }
        break;
      case 'n':
        config->display_rows = false;
        break;
      case 'q':
        config->quiet_mode = true;
        break;
      case 'h':
        print_usage(argv[0]);
        return 1;
      default:
        return -1;
    }
  }

  *file_arg_start = optind;
  return 0;
}

// =============================================================================
// Main Program
// =============================================================================

int main(int argc, char *argv[]) {
  BenchmarkConfig config;
  int file_arg_start;

  // Parse command line arguments
  int parse_result = parse_arguments(argc, argv, &config, &file_arg_start);
  if (parse_result != 0) {
    if (parse_result < 0) {
      fprintf(stderr,
              "Error parsing arguments. Use --help for usage information.\n");
    }
    return parse_result < 0 ? 1 : 0;
  }

  // Check for file arguments
  if (file_arg_start >= argc) {
    fprintf(stderr, "Error: No input files specified.\n");
    print_usage(argv[0]);
    return 1;
  }

  const char **file_paths = (const char **) &argv[file_arg_start];
  uint32_t num_files = argc - file_arg_start;

  if (!config.quiet_mode) {
    printf("=== Parquet Reader Stream Benchmark Tool ===\n");
    printf("Configuration:\n");
    printf("  Threading: %s\n", config.use_threading ? "ENABLED" : "DISABLED");
    printf("  Workload: %s\n",
           config.workload == WORKLOAD_HEAVY ? "HEAVY" : "LIGHT");
    if (config.workload == WORKLOAD_HEAVY) {
      printf("  Iterations: %d\n", config.iterations);
    }
    printf("  Batch size: %d\n", config.batch_size);

    if (num_files == 1) {
      printf("\nReading single file: %s\n", file_paths[0]);
    } else {
      printf("\nMerging %d files:\n", num_files);
      for (uint32_t i = 0; i < num_files; i++) {
        printf("  %d. %s\n", i + 1, file_paths[i]);
      }
    }
  }

  // Initialize stream with or without threading
  PacketStream *stream;
  if (num_files == 1) {
    if (config.use_threading) {
      stream = parquet_reader_init_stream_threaded_with_batch_size(
          file_paths[0], config.batch_size);
    } else {
      stream = parquet_reader_init_stream_with_batch_size(file_paths[0],
                                                          config.batch_size);
    }
  } else {
    if (config.use_threading) {
      if (!config.quiet_mode) {
        printf("Note: Threaded merger not yet implemented, falling back to "
               "synchronous mode\n");
      }
      stream = parquet_merger_init_stream_with_batch_size(file_paths,
                                                          num_files,
                                                          0,
                                                          config.batch_size);
      config.use_threading = false; // Update flag for accurate reporting
    } else {
      stream = parquet_merger_init_stream_with_batch_size(file_paths,
                                                          num_files,
                                                          0,
                                                          config.batch_size);
    }
  }

  if (!stream) {
    fprintf(stderr,
            "Failed to initialize %s stream\n",
            config.use_threading ? "threaded" : "synchronous");
    return 1;
  }

  // Process the stream
  process_stream(stream, file_paths, num_files, &config);

  // Cleanup
  packet_stream_release(stream);

  if (!config.quiet_mode) {
    printf("\n=== Benchmark completed successfully! ===\n");
  }
  return 0;
}