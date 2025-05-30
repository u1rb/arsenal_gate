#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <parquet_ffi/parquet_writer_stream.h>

// =============================================================================
// CSV Parser and Data Structures
// =============================================================================

#define MAX_LINE_LENGTH 4096
#define MAX_FIELD_LENGTH 1024
#define MAX_COLUMNS 32

typedef struct {
    char **field_names;
    char **field_types;  // Arrow format strings
    bool *nullable;
    size_t num_columns;
} CSVSchema;

typedef struct {
    char **fields;
    size_t num_fields;
} CSVRow;

typedef struct {
    const char *input_file;
    const char *output_file;
    size_t batch_size;
    bool has_header;
    char delimiter;
    ParquetStreamCompression compression;
} Config;

// =============================================================================
// CSV Schema Detection and Parsing
// =============================================================================

static void free_csv_schema(CSVSchema *schema) {
    if (!schema) return;
    
    for (size_t i = 0; i < schema->num_columns; i++) {
        free(schema->field_names[i]);
        free(schema->field_types[i]);
    }
    free(schema->field_names);
    free(schema->field_types);
    free(schema->nullable);
    memset(schema, 0, sizeof(CSVSchema));
}

static void free_csv_row(CSVRow *row) {
    if (!row) return;
    
    for (size_t i = 0; i < row->num_fields; i++) {
        free(row->fields[i]);
    }
    free(row->fields);
    memset(row, 0, sizeof(CSVRow));
}

static int parse_csv_line(const char *line, char delimiter, CSVRow *row) {
    memset(row, 0, sizeof(CSVRow));
    
    // Skip empty lines
    if (!line || strlen(line) == 0) {
        return 0;
    }
    
    // Remove trailing newline/carriage return
    char *line_copy = strdup(line);
    if (!line_copy) return 0;
    
    size_t len = strlen(line_copy);
    while (len > 0 && (line_copy[len-1] == '\n' || line_copy[len-1] == '\r')) {
        line_copy[len-1] = '\0';
        len--;
    }
    
    // Skip empty lines after trimming
    if (len == 0) {
        free(line_copy);
        return 0;
    }
    
    // Count fields first
    size_t field_count = 1;
    for (const char *p = line_copy; *p; p++) {
        if (*p == delimiter) field_count++;
    }
    
    row->fields = (char **)calloc(field_count, sizeof(char *));
    if (!row->fields) {
        free(line_copy);
        return 0;
    }
    
    // Parse fields
    const char *start = line_copy;
    size_t field_idx = 0;
    
    for (const char *p = line_copy; ; p++) {
        if (*p == delimiter || *p == '\0') {
            size_t field_len = p - start;
            
            // Trim whitespace
            while (field_len > 0 && (start[field_len-1] == ' ' || start[field_len-1] == '\t')) field_len--;
            while (field_len > 0 && (*start == ' ' || *start == '\t')) { start++; field_len--; }
            
            // Handle quoted fields
            if (field_len >= 2 && start[0] == '"' && start[field_len-1] == '"') {
                start++;
                field_len -= 2;
            }
            
            row->fields[field_idx] = (char *)malloc(field_len + 1);
            if (!row->fields[field_idx]) {
                free(line_copy);
                free_csv_row(row);
                return 0;
            }
            
            memcpy(row->fields[field_idx], start, field_len);
            row->fields[field_idx][field_len] = '\0';
            field_idx++;
            
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    
    row->num_fields = field_idx;
    free(line_copy);
    return 1;
}

static const char *detect_field_type(const char *value) {
    if (!value || strlen(value) == 0) {
        return ARROW_FORMAT_STRING;  // Default to string for empty values
    }
    
    // Check for integer
    char *endptr;
    long long_val = strtol(value, &endptr, 10);
    if (*endptr == '\0') {
        if (long_val >= INT32_MIN && long_val <= INT32_MAX) {
            return ARROW_FORMAT_INT32;
        } else {
            return ARROW_FORMAT_INT64;
        }
    }
    
    // Check for double
    double double_val = strtod(value, &endptr);
    if (*endptr == '\0') {
        (void)double_val;  // Suppress unused variable warning
        return ARROW_FORMAT_DOUBLE;
    }
    
    // Check for boolean
    if (strcasecmp(value, "true") == 0 || strcasecmp(value, "false") == 0 ||
        strcmp(value, "1") == 0 || strcmp(value, "0") == 0) {
        return ARROW_FORMAT_BOOL;
    }
    
    // Default to string
    return ARROW_FORMAT_STRING;
}

static int detect_csv_schema(FILE *file, char delimiter, CSVSchema *schema) {
    char line[MAX_LINE_LENGTH];
    CSVRow header_row, sample_row;
    
    memset(schema, 0, sizeof(CSVSchema));
    
    // Read header line
    if (!fgets(line, sizeof(line), file)) {
        fprintf(stderr, "Error: Could not read header line\n");
        return 0;
    }
    
    if (!parse_csv_line(line, delimiter, &header_row)) {
        fprintf(stderr, "Error: Could not parse header line\n");
        return 0;
    }
    
    schema->num_columns = header_row.num_fields;
    schema->field_names = (char **)calloc(schema->num_columns, sizeof(char *));
    schema->field_types = (char **)calloc(schema->num_columns, sizeof(char *));
    schema->nullable = (bool *)calloc(schema->num_columns, sizeof(bool));
    
    if (!schema->field_names || !schema->field_types || !schema->nullable) {
        free_csv_row(&header_row);
        free_csv_schema(schema);
        return 0;
    }
    
    // Copy field names
    for (size_t i = 0; i < schema->num_columns; i++) {
        schema->field_names[i] = strdup(header_row.fields[i]);
        schema->nullable[i] = true;  // Assume all fields are nullable
    }
    
    free_csv_row(&header_row);
    
    // Read a few sample lines to detect types
    const char *detected_types[MAX_COLUMNS] = {0};
    for (size_t i = 0; i < schema->num_columns; i++) {
        detected_types[i] = ARROW_FORMAT_STRING;  // Default
    }
    
    int sample_count = 0;
    while (sample_count < 10 && fgets(line, sizeof(line), file)) {
        if (!parse_csv_line(line, delimiter, &sample_row)) continue;
        
        for (size_t i = 0; i < schema->num_columns && i < sample_row.num_fields; i++) {
            if (sample_row.fields[i] && strlen(sample_row.fields[i]) > 0) {
                const char *type = detect_field_type(sample_row.fields[i]);
                
                // Upgrade type if needed (string is most general)
                if (strcmp(detected_types[i], ARROW_FORMAT_STRING) != 0) {
                    if (strcmp(type, ARROW_FORMAT_STRING) == 0) {
                        detected_types[i] = ARROW_FORMAT_STRING;
                    } else if (strcmp(detected_types[i], ARROW_FORMAT_INT32) == 0 && 
                               strcmp(type, ARROW_FORMAT_INT64) == 0) {
                        detected_types[i] = ARROW_FORMAT_INT64;
                    } else if (strcmp(detected_types[i], ARROW_FORMAT_INT32) == 0 && 
                               strcmp(type, ARROW_FORMAT_DOUBLE) == 0) {
                        detected_types[i] = ARROW_FORMAT_DOUBLE;
                    } else if (strcmp(detected_types[i], ARROW_FORMAT_INT64) == 0 && 
                               strcmp(type, ARROW_FORMAT_DOUBLE) == 0) {
                        detected_types[i] = ARROW_FORMAT_DOUBLE;
                    }
                }
            }
        }
        
        free_csv_row(&sample_row);
        sample_count++;
    }
    
    // Set detected types
    for (size_t i = 0; i < schema->num_columns; i++) {
        schema->field_types[i] = strdup(detected_types[i]);
    }
    
    // Reset file position to beginning
    rewind(file);
    
    return 1;
}

// =============================================================================
// Data Conversion Functions
// =============================================================================

static int convert_field_value(const char *field_value, const char *field_type, 
                              void **value, bool *is_null, size_t *size) {
    *is_null = (!field_value || strlen(field_value) == 0 || 
                strcasecmp(field_value, "null") == 0 || 
                strcasecmp(field_value, "na") == 0);
    
    if (*is_null) {
        *value = NULL;
        *size = 0;
        return 1;
    }
    
    if (strcmp(field_type, ARROW_FORMAT_INT32) == 0) {
        static int32_t int_val;
        int_val = (int32_t)strtol(field_value, NULL, 10);
        *value = &int_val;
        *size = sizeof(int32_t);
    } else if (strcmp(field_type, ARROW_FORMAT_INT64) == 0) {
        static int64_t long_val;
        long_val = strtoll(field_value, NULL, 10);
        *value = &long_val;
        *size = sizeof(int64_t);
    } else if (strcmp(field_type, ARROW_FORMAT_UINT64) == 0) {
        static uint64_t ulong_val;
        ulong_val = strtoull(field_value, NULL, 10);
        *value = &ulong_val;
        *size = sizeof(uint64_t);
    } else if (strcmp(field_type, ARROW_FORMAT_DOUBLE) == 0) {
        static double double_val;
        double_val = strtod(field_value, NULL);
        *value = &double_val;
        *size = sizeof(double);
    } else if (strcmp(field_type, ARROW_FORMAT_FLOAT) == 0) {
        static float float_val;
        float_val = (float)strtod(field_value, NULL);
        *value = &float_val;
        *size = sizeof(float);
    } else if (strcmp(field_type, ARROW_FORMAT_BOOL) == 0) {
        static bool bool_val;
        bool_val = (strcasecmp(field_value, "true") == 0 || strcmp(field_value, "1") == 0);
        *value = &bool_val;
        *size = sizeof(bool);
    } else {
        // String or binary
        *value = (void *)field_value;
        *size = strlen(field_value);
    }
    
    return 1;
}

// =============================================================================
// Main Conversion Logic
// =============================================================================

static int convert_csv_to_parquet(const Config *config) {
    FILE *csv_file = fopen(config->input_file, "r");
    if (!csv_file) {
        fprintf(stderr, "Error: Could not open CSV file '%s': %s\n", 
                config->input_file, strerror(errno));
        return 0;
    }
    
    // Detect schema
    CSVSchema schema;
    if (!detect_csv_schema(csv_file, config->delimiter, &schema)) {
        fprintf(stderr, "Error: Could not detect CSV schema\n");
        fclose(csv_file);
        return 0;
    }
    
    printf("Detected schema with %zu columns:\n", schema.num_columns);
    for (size_t i = 0; i < schema.num_columns; i++) {
        printf("  %zu: %s (%s, nullable=%s)\n", i, schema.field_names[i], 
               schema.field_types[i], schema.nullable[i] ? "true" : "false");
    }
    
    // Create column definitions
    ColumnDef *column_defs = (ColumnDef *)calloc(schema.num_columns, sizeof(ColumnDef));
    if (!column_defs) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_csv_schema(&schema);
        fclose(csv_file);
        return 0;
    }
    
    for (size_t i = 0; i < schema.num_columns; i++) {
        column_defs[i] = create_column_def(schema.field_names[i], 
                                          schema.field_types[i], 
                                          schema.nullable[i]);
    }
    
    // Create writer
    StreamWriter *writer = create_writer_with_compression(
        config->output_file, config->batch_size, column_defs, schema.num_columns,
        config->compression);
    
    if (!writer) {
        fprintf(stderr, "Error: Could not create Parquet writer\n");
        free(column_defs);
        free_csv_schema(&schema);
        fclose(csv_file);
        return 0;
    }
    
    printf("Writing CSV data to Parquet file '%s'...\n", config->output_file);
    
    // Skip header if present
    char line[MAX_LINE_LENGTH];
    if (config->has_header) {
        if (!fgets(line, sizeof(line), csv_file)) {
            fprintf(stderr, "Error: Could not skip header line\n");
            free_writer(writer);
            free(column_defs);
            free_csv_schema(&schema);
            fclose(csv_file);
            return 0;
        }
    }
    
    // Process data rows
    int64_t row_count = 0;
    CSVRow csv_row;
    
    while (fgets(line, sizeof(line), csv_file)) {
        if (!parse_csv_line(line, config->delimiter, &csv_row)) {
            fprintf(stderr, "Warning: Could not parse line %lld, skipping\n", 
                    (long long)(row_count + 1));
            continue;
        }
        
        if (csv_row.num_fields != schema.num_columns) {
            fprintf(stderr, "Warning: Line %lld has %zu fields, expected %zu, skipping\n",
                    (long long)(row_count + 1), csv_row.num_fields, schema.num_columns);
            free_csv_row(&csv_row);
            continue;
        }
        
        // Convert row data
        const void *values[schema.num_columns];
        bool nulls[schema.num_columns];
        size_t sizes[schema.num_columns];
        
        for (size_t i = 0; i < schema.num_columns; i++) {
            if (!convert_field_value(csv_row.fields[i], schema.field_types[i],
                                   (void **)&values[i], &nulls[i], &sizes[i])) {
                fprintf(stderr, "Error: Could not convert field %zu in row %lld\n",
                        i, (long long)(row_count + 1));
                free_csv_row(&csv_row);
                goto cleanup;
            }
        }
        
        // Add row to writer
        if (!add_row(writer, values, nulls, sizes)) {
            fprintf(stderr, "Error: Could not add row %lld to Parquet file\n",
                    (long long)(row_count + 1));
            free_csv_row(&csv_row);
            goto cleanup;
        }
        
        free_csv_row(&csv_row);
        row_count++;
        
        if (row_count % 10000 == 0) {
            printf("Processed %lld rows...\n", (long long)row_count);
        }
    }
    
    printf("Successfully processed %lld rows\n", (long long)row_count);
    
    // Close writer
    if (!close_writer(writer)) {
        fprintf(stderr, "Error: Could not close Parquet writer\n");
        goto cleanup;
    }
    
    free_writer(writer);
    free(column_defs);
    free_csv_schema(&schema);
    fclose(csv_file);
    
    printf("Conversion completed successfully!\n");
    return 1;
    
cleanup:
    free_writer(writer);
    free(column_defs);
    free_csv_schema(&schema);
    fclose(csv_file);
    return 0;
}

// =============================================================================
// Configuration and Main
// =============================================================================

static void print_usage(const char *program_name) {
    printf("Usage: %s [options] <input.csv> <output.parquet>\n", program_name);
    printf("\nOptions:\n");
    printf("  -b, --batch-size SIZE    Batch size for writing (default: 10000)\n");
    printf("  -d, --delimiter CHAR     CSV delimiter (default: ',')\n");
    printf("  -H, --no-header          CSV file has no header row\n");
    printf("  -c, --compression TYPE   Compression type: none, snappy, gzip, lz4, zstd (default: snappy)\n");
    printf("  -h, --help               Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s data.csv data.parquet\n", program_name);
    printf("  %s -b 50000 -c zstd data.csv data.parquet\n", program_name);
    printf("  %s -d ';' -H data.csv data.parquet\n", program_name);
}

static ParquetStreamCompression parse_compression(const char *str) {
    if (strcasecmp(str, "none") == 0 || strcasecmp(str, "uncompressed") == 0) {
        return PARQUET_STREAM_COMPRESSION_UNCOMPRESSED;
    } else if (strcasecmp(str, "snappy") == 0) {
        return PARQUET_STREAM_COMPRESSION_SNAPPY;
    } else if (strcasecmp(str, "gzip") == 0) {
        return PARQUET_STREAM_COMPRESSION_GZIP;
    } else if (strcasecmp(str, "lz4") == 0) {
        return PARQUET_STREAM_COMPRESSION_LZ4_RAW;
    } else if (strcasecmp(str, "zstd") == 0) {
        return PARQUET_STREAM_COMPRESSION_ZSTD;
    } else {
        return PARQUET_STREAM_COMPRESSION_SNAPPY;  // Default
    }
}

static int parse_args(int argc, char *argv[], Config *config) {
    // Set defaults
    config->batch_size = 10000;
    config->delimiter = ',';
    config->has_header = true;
    config->compression = PARQUET_STREAM_COMPRESSION_SNAPPY;
    config->input_file = NULL;
    config->output_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--batch-size") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --batch-size requires an argument\n");
                return -1;
            }
            config->batch_size = (size_t)atoll(argv[i]);
            if (config->batch_size == 0) {
                fprintf(stderr, "Error: Invalid batch size\n");
                return -1;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delimiter") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --delimiter requires an argument\n");
                return -1;
            }
            config->delimiter = argv[i][0];
        } else if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--no-header") == 0) {
            config->has_header = false;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compression") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --compression requires an argument\n");
                return -1;
            }
            config->compression = parse_compression(argv[i]);
        } else if (argv[i][0] != '-') {
            if (!config->input_file) {
                config->input_file = argv[i];
            } else if (!config->output_file) {
                config->output_file = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                return -1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return -1;
        }
    }
    
    if (!config->input_file || !config->output_file) {
        fprintf(stderr, "Error: Both input and output files are required\n");
        print_usage(argv[0]);
        return -1;
    }
    
    return 1;
}

int main(int argc, char *argv[]) {
    parquet_ffi_init_tracing();
    
    Config config;
    int parse_result = parse_args(argc, argv, &config);
    
    if (parse_result == 0) {
        return 0;  // Help was shown
    } else if (parse_result < 0) {
        return 1;  // Error in arguments
    }
    
    printf("CSV to Parquet Converter\n");
    printf("Input:  %s\n", config.input_file);
    printf("Output: %s\n", config.output_file);
    printf("Batch size: %zu\n", config.batch_size);
    printf("Delimiter: '%c'\n", config.delimiter);
    printf("Has header: %s\n", config.has_header ? "yes" : "no");
    printf("\n");
    
    clock_t start = clock();
    
    if (!convert_csv_to_parquet(&config)) {
        fprintf(stderr, "Conversion failed!\n");
        return 1;
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Conversion completed in %.2f seconds\n", elapsed);
    
    return 0;
} 