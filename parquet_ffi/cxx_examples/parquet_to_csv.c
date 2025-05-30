#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <parquet_ffi/parquet_reader_stream.h>

// =============================================================================
// Configuration and Data Structures
// =============================================================================

typedef struct {
    const char *input_file;
    const char *output_file;
    char delimiter;
    bool include_header;
    bool quote_strings;
    size_t max_rows;
} Config;

// =============================================================================
// CSV Writing Functions
// =============================================================================

static void write_csv_header(FILE *csv_file, PacketStream *stream, char delimiter) {
    size_t column_count = packet_stream_get_column_count(stream);
    
    for (size_t i = 0; i < column_count; i++) {
        const char *column_name = packet_stream_get_column_name(stream, i);
        if (i > 0) {
            fputc(delimiter, csv_file);
        }
        fprintf(csv_file, "%s", column_name);
    }
    fputc('\n', csv_file);
}

static void write_csv_value(FILE *csv_file, PacketStream *stream, size_t column_index, 
                           bool quote_strings, bool is_last_column, char delimiter) {
    const char *format = packet_stream_get_column_format(stream, column_index);
    
    if (packet_stream_is_null(stream, column_index)) {
        // Write empty value for nulls
        if (!is_last_column) {
            fputc(delimiter, csv_file);
        }
        return;
    }
    
    // Handle different data types based on Arrow format
    if (strcmp(format, "b") == 0) {
        // Boolean
        bool val = packet_stream_get_value_bool(stream, column_index);
        fprintf(csv_file, "%s", val ? "true" : "false");
    } else if (strcmp(format, "c") == 0) {
        // int8
        int8_t val = packet_stream_get_value_int8(stream, column_index);
        fprintf(csv_file, "%d", val);
    } else if (strcmp(format, "C") == 0) {
        // uint8
        uint8_t val = packet_stream_get_value_uint8(stream, column_index);
        fprintf(csv_file, "%u", val);
    } else if (strcmp(format, "s") == 0) {
        // int16
        int16_t val = packet_stream_get_value_int16(stream, column_index);
        fprintf(csv_file, "%d", val);
    } else if (strcmp(format, "S") == 0) {
        // uint16
        uint16_t val = packet_stream_get_value_uint16(stream, column_index);
        fprintf(csv_file, "%u", val);
    } else if (strcmp(format, "i") == 0) {
        // int32
        int32_t val = packet_stream_get_value_int32(stream, column_index);
        fprintf(csv_file, "%d", val);
    } else if (strcmp(format, "I") == 0) {
        // uint32
        uint32_t val = packet_stream_get_value_uint32(stream, column_index);
        fprintf(csv_file, "%u", val);
    } else if (strcmp(format, "l") == 0) {
        // int64
        int64_t val = packet_stream_get_value_int64(stream, column_index);
        fprintf(csv_file, "%lld", (long long)val);
    } else if (strcmp(format, "L") == 0) {
        // uint64
        uint64_t val = packet_stream_get_value_uint64(stream, column_index);
        fprintf(csv_file, "%llu", (unsigned long long)val);
    } else if (strcmp(format, "f") == 0) {
        // float
        float val = packet_stream_get_value_float(stream, column_index);
        fprintf(csv_file, "%.6g", val);
    } else if (strcmp(format, "g") == 0) {
        // double
        double val = packet_stream_get_value_double(stream, column_index);
        fprintf(csv_file, "%.15g", val);
    } else if (strcmp(format, "u") == 0) {
        // string
        const char *str_data;
        size_t str_len;
        if (packet_stream_get_string_zerocopy(stream, column_index, &str_data, &str_len)) {
            if (quote_strings) {
                fputc('"', csv_file);
                // Write string, escaping quotes
                for (size_t i = 0; i < str_len; i++) {
                    if (str_data[i] == '"') {
                        fprintf(csv_file, "\"\"");  // Escape quote with double quote
                    } else {
                        fputc(str_data[i], csv_file);
                    }
                }
                fputc('"', csv_file);
            } else {
                fwrite(str_data, 1, str_len, csv_file);
            }
        }
    } else if (strcmp(format, "z") == 0) {
        // binary
        const uint8_t *binary_data;
        size_t binary_len;
        if (packet_stream_get_binary_zerocopy(stream, column_index, &binary_data, &binary_len)) {
            // Write binary as hex string
            if (quote_strings) {
                fputc('"', csv_file);
            }
            for (size_t i = 0; i < binary_len; i++) {
                fprintf(csv_file, "%02x", binary_data[i]);
            }
            if (quote_strings) {
                fputc('"', csv_file);
            }
        }
    } else {
        // Unknown format, try to get as string
        const char *str_data;
        size_t str_len;
        if (packet_stream_get_string_zerocopy(stream, column_index, &str_data, &str_len)) {
            if (quote_strings) {
                fputc('"', csv_file);
                fwrite(str_data, 1, str_len, csv_file);
                fputc('"', csv_file);
            } else {
                fwrite(str_data, 1, str_len, csv_file);
            }
        }
    }
    
    if (!is_last_column) {
        fputc(delimiter, csv_file);
    }
}

// =============================================================================
// Main Conversion Logic
// =============================================================================

static int convert_parquet_to_csv(const Config *config) {
    // Initialize Parquet reader
    PacketStream *stream = parquet_reader_init_stream(config->input_file);
    if (!stream) {
        fprintf(stderr, "Error: Could not open Parquet file '%s'\n", config->input_file);
        return 0;
    }
    
    // Open CSV output file
    FILE *csv_file = fopen(config->output_file, "w");
    if (!csv_file) {
        fprintf(stderr, "Error: Could not create CSV file '%s': %s\n", 
                config->output_file, strerror(errno));
        packet_stream_release(stream);
        return 0;
    }
    
    printf("Converting Parquet to CSV...\n");
    printf("Input:  %s\n", config->input_file);
    printf("Output: %s\n", config->output_file);
    
    // Get schema information
    size_t column_count = packet_stream_get_column_count(stream);
    printf("Schema: %zu columns\n", column_count);
    
    for (size_t i = 0; i < column_count; i++) {
        const char *name = packet_stream_get_column_name(stream, i);
        const char *format = packet_stream_get_column_format(stream, i);
        printf("  %zu: %s (%s)\n", i, name, format);
    }
    
    // Write CSV header if requested
    if (config->include_header) {
        write_csv_header(csv_file, stream, config->delimiter);
    }
    
    // Process data rows
    size_t row_count = 0;
    while (packet_stream_next(stream)) {
        // Check max rows limit
        if (config->max_rows > 0 && row_count >= config->max_rows) {
            printf("Reached maximum row limit (%zu), stopping\n", config->max_rows);
            break;
        }
        
        // Write row data
        for (size_t i = 0; i < column_count; i++) {
            bool is_last_column = (i == column_count - 1);
            write_csv_value(csv_file, stream, i, config->quote_strings, 
                          is_last_column, config->delimiter);
        }
        fputc('\n', csv_file);
        
        row_count++;
        
        if (row_count % 10000 == 0) {
            printf("Processed %zu rows...\n", row_count);
        }
    }
    
    printf("Successfully converted %zu rows\n", row_count);
    
    // Cleanup
    fclose(csv_file);
    packet_stream_release(stream);
    
    return 1;
}

// =============================================================================
// Configuration and Main
// =============================================================================

static void print_usage(const char *program_name) {
    printf("Usage: %s [options] <input.parquet> <output.csv>\n", program_name);
    printf("\nOptions:\n");
    printf("  -d, --delimiter CHAR     CSV delimiter (default: ',')\n");
    printf("  -H, --no-header          Don't include header row in CSV\n");
    printf("  -q, --quote-strings      Quote string values\n");
    printf("  -n, --max-rows NUM       Maximum number of rows to convert\n");
    printf("  -h, --help               Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s data.parquet data.csv\n", program_name);
    printf("  %s -d ';' -q data.parquet data.csv\n", program_name);
    printf("  %s -n 1000 --no-header data.parquet data.csv\n", program_name);
}

static int parse_args(int argc, char *argv[], Config *config) {
    // Set defaults
    config->delimiter = ',';
    config->include_header = true;
    config->quote_strings = false;
    config->max_rows = 0;  // 0 means no limit
    config->input_file = NULL;
    config->output_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delimiter") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --delimiter requires an argument\n");
                return -1;
            }
            config->delimiter = argv[i][0];
        } else if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--no-header") == 0) {
            config->include_header = false;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quote-strings") == 0) {
            config->quote_strings = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--max-rows") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --max-rows requires an argument\n");
                return -1;
            }
            config->max_rows = (size_t)atoll(argv[i]);
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
    
    printf("Parquet to CSV Converter\n");
    printf("Input:  %s\n", config.input_file);
    printf("Output: %s\n", config.output_file);
    printf("Delimiter: '%c'\n", config.delimiter);
    printf("Include header: %s\n", config.include_header ? "yes" : "no");
    printf("Quote strings: %s\n", config.quote_strings ? "yes" : "no");
    if (config.max_rows > 0) {
        printf("Max rows: %zu\n", config.max_rows);
    }
    printf("\n");
    
    clock_t start = clock();
    
    if (!convert_parquet_to_csv(&config)) {
        fprintf(stderr, "Conversion failed!\n");
        return 1;
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Conversion completed in %.2f seconds\n", elapsed);
    
    return 0;
} 