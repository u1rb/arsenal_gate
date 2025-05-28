#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <parquet_ffi/parquet_writer_stream.h>

// =============================================================================
// Demo Schema Definition
// =============================================================================

// =============================================================================
// Data Generation
// =============================================================================

static uint32_t rand_state = 1;
static uint32_t simple_rand() {
  rand_state = rand_state * 1103515245 + 12345;
  return rand_state;
}

static void generate_row_data(int64_t row_idx,
                              void **values,
                              bool *nulls,
                              size_t *sizes) {
  static int32_t id;
  static uint64_t id2;
  static double value;
  static char label[16];
  static uint8_t binary_data[100];

  // Generate data
  id = (int32_t) row_idx;
  id2 = (uint64_t) row_idx;
  value = row_idx * 0.01;

  // Generate random string
  rand_state = (uint32_t) (row_idx + 1);
  for (int i = 0; i < 10; i++) {
    label[i] = 32 + (simple_rand() % 95);
  }
  label[10] = '\0';

  // Generate random binary data
  rand_state = (uint32_t) (row_idx + 1000);
  for (int i = 0; i < 100; i++) {
    binary_data[i] = simple_rand() & 0xFF;
  }

  // Set output values
  values[0] = &id;
  values[1] = &id2;
  values[2] = &value;
  values[3] = label;
  values[4] = binary_data;

  // Set null flags
  nulls[0] = false;
  nulls[1] = false;
  nulls[2] = (row_idx % 5 == 0); // Every 5th value is null
  nulls[3] = false;
  nulls[4] = false;

  // Set sizes (only needed for binary data)
  sizes[4] = 100;
}

// =============================================================================
// Schema Definition
// =============================================================================

static const ColumnDef *get_default_schema(size_t *num_columns) {
  static const ColumnDef DEFAULT_SCHEMA[] = {
      {.name = "id",
       .format = ARROW_FORMAT_INT32,
       .nullable = false,
       .encoding = PARQUET_ENCODING_DELTA_BINARY_PACKED,
       .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
       .use_dictionary = false,
       .enable_bloom_filter = false,
       .enable_statistics = true},
      {.name = "id2",
       .format = ARROW_FORMAT_UINT64,
       .nullable = false,
       .encoding = PARQUET_ENCODING_DELTA_BINARY_PACKED,
       .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
       .use_dictionary = false,
       .enable_bloom_filter = false,
       .enable_statistics = true},
      {.name = "value",
       .format = ARROW_FORMAT_DOUBLE,
       .nullable = true,
       .encoding = PARQUET_ENCODING_BYTE_STREAM_SPLIT,
       .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
       .use_dictionary = false,
       .enable_bloom_filter = false,
       .enable_statistics = true},
      {.name = "label",
       .format = ARROW_FORMAT_STRING,
       .nullable = false,
       .encoding = PARQUET_ENCODING_PLAIN,
       .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
       .use_dictionary = true,
       .enable_bloom_filter = false,
       .enable_statistics = true},
      {.name = "binary_data",
       .format = ARROW_FORMAT_BINARY,
       .nullable = false,
       .encoding = PARQUET_ENCODING_DELTA_LENGTH_BYTE_ARRAY,
       .compression = PARQUET_COMPRESSION_UNCOMPRESSED,
       .use_dictionary = false,
       .enable_bloom_filter = false,
       .enable_statistics = true}};

  *num_columns = sizeof(DEFAULT_SCHEMA) / sizeof(DEFAULT_SCHEMA[0]);
  return DEFAULT_SCHEMA;
}

// =============================================================================
// Main Program
// =============================================================================

typedef struct {
  const char *output_file;
  int64_t num_rows;
  int64_t batch_size;
} Config;

static int parse_args(int argc, char *argv[], Config *config) {
  if (argc < 4) {
    fprintf(stderr,
            "Usage: %s <output_file> <num_rows> <batch_size>\n",
            argv[0]);
    return 0;
  }

  config->output_file = argv[1];
  config->num_rows = atoll(argv[2]);
  config->batch_size = atoll(argv[3]);

  if (config->num_rows <= 0 || config->batch_size <= 0) {
    fprintf(stderr, "num_rows and batch_size must be positive\n");
    return 0;
  }

  if (config->batch_size > MAX_BATCH_SIZE) {
    fprintf(stderr, "Warning: Limiting batch size to %d\n", MAX_BATCH_SIZE);
    config->batch_size = MAX_BATCH_SIZE;
  }

  return 1;
}

static int write_data_to_parquet(StreamWriter *writer,
                                 int64_t num_rows,
                                 size_t num_columns) {
  // Generate and write data - using runtime NUM_COLUMNS for variable-length
  // arrays
  const void *values[num_columns];
  bool nulls[num_columns];
  size_t sizes[num_columns];

  // Initialize arrays properly for variable-length arrays
  for (size_t i = 0; i < num_columns; i++) {
    values[i] = NULL;
    nulls[i] = false;
    sizes[i] = 0;
  }

  for (int64_t i = 0; i < num_rows; i++) {
    generate_row_data(i, (void **) values, nulls, sizes);

    if (!add_row(writer, values, nulls, sizes)) {
      fprintf(stderr, "Failed to add row %ld\n", i);
      return 0;
    }

    if (i > 0 && i % 1000000 == 0) {
      printf("Added %ld rows...\n", i);
    }
  }

  return 1;
}

static void print_performance_stats(const char *output_file,
                                    clock_t start,
                                    clock_t end) {
  double elapsed = (double) (end - start) / CLOCKS_PER_SEC;

  // Calculate file size and throughput
  FILE *file = fopen(output_file, "rb");
  long file_size = 0;
  if (file) {
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fclose(file);
  }

  double mb_written = file_size / (1024.0 * 1024.0);
  double mb_per_sec = elapsed > 0 ? mb_written / elapsed : 0;

  printf("Completed in %.2f seconds\n", elapsed);
  printf("File size: %.2f MB, Write speed: %.2f MB/s\n",
         mb_written,
         mb_per_sec);
}

static StreamWriter *create_parquet_writer(const Config *config,
                                           const ColumnDef *schema,
                                           size_t num_columns) {
  printf("Writing %ld rows to %s (batch size: %ld)\n",
         config->num_rows,
         config->output_file,
         config->batch_size);

  StreamWriter *writer =
      create_writer_with_compression(config->output_file,
                                     config->batch_size,
                                     schema,
                                     num_columns,
                                     PARQUET_COMPRESSION_LZ4_RAW);
  if (!writer) {
    fprintf(stderr, "Failed to create writer\n");
    return NULL;
  }

  return writer;
}

int main(int argc, char *argv[]) {
  // Initialize Rust tracing for debugging (optional but recommended)
  parquet_ffi_init_tracing();

  // Get schema
  size_t NUM_COLUMNS;
  const ColumnDef *DEFAULT_SCHEMA = get_default_schema(&NUM_COLUMNS);

  Config config;
  if (!parse_args(argc, argv, &config))
    return 1;

  StreamWriter *writer =
      create_parquet_writer(&config, DEFAULT_SCHEMA, NUM_COLUMNS);
  FAIL_IF(!writer, "Failed to create writer");

  clock_t start = clock();

  if (!write_data_to_parquet(writer, config.num_rows, NUM_COLUMNS)) {
    fprintf(stderr, "Failed to write data to parquet\n");
    free_writer(writer);
    return 1;
  }

  if (!close_writer(writer)) {
    fprintf(stderr, "Failed to close writer\n");
    free_writer(writer);
    return 1;
  }

  clock_t end = clock();

  print_performance_stats(config.output_file, start, end);

  free_writer(writer);
  return 0;
}