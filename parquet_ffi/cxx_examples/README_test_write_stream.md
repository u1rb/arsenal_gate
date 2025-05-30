# Python Unit Tests for Parquet Writer

This directory contains comprehensive Python unit tests for the `a0_write_stream.c` Parquet writer functionality.

## Overview

The `test_write_stream.py` script provides thorough validation of the Parquet writer by:

1. **Schema Validation**: Verifies column names, types, and nullability match expectations
2. **Data Integrity**: Compares written data against expected generation patterns
3. **Null Handling**: Tests proper null value handling (every 5th value in 'value' column)
4. **Binary Data**: Validates binary data writing and reading
5. **Batch Processing**: Tests different batch sizes and row counts
6. **File Properties**: Examines Parquet metadata, compression, and encodings

## Usage

### Quick Start

```bash
# Build and run tests
bash run.sh --cell=build,test_write

# Run tests only (if already built)
bash run.sh --cell=test_write
```

### Direct Execution

```bash
# Using uv (recommended - automatically installs dependencies)
uv run --with pyarrow --with pandas test_write_stream.py

# Or with pre-installed dependencies
python test_write_stream.py
```

## Test Coverage

### Schema Testing
- ✅ Column count verification
- ✅ Column name validation
- ✅ Data type checking (int32, uint64, double, string, binary)
- ✅ Nullability constraints

### Data Integrity Testing
- ✅ Row count verification
- ✅ Value generation pattern matching
- ✅ Random string generation validation
- ✅ Binary data pattern verification
- ✅ Null value pattern checking (every 5th row)

### Configuration Testing
- ✅ Small files (100 rows, batch size 50)
- ✅ Medium files (1000 rows, batch size 100)
- ✅ Large batch processing (500 rows, batch size 500)
- ✅ Tiny batch processing (1000 rows, batch size 10)

### File Properties Testing
- ✅ File size and metadata
- ✅ Row group structure
- ✅ Compression verification (LZ4)
- ✅ Encoding validation

## Expected Data Pattern

The test replicates the exact data generation logic from `a0_write_stream.c`:

```python
# ID columns: simple row index
id = row_idx
id2 = row_idx

# Value column: null every 5th row, otherwise row_idx * 0.01
value = None if (row_idx % 5 == 0) else row_idx * 0.01

# Label: 10-character random string using simple_rand()
# Binary data: 100 bytes of random data using simple_rand()
```

## Dependencies

The test automatically installs required dependencies when run with `uv`:

- **pyarrow**: For reading and validating Parquet files
- **pandas**: For data manipulation and analysis

## Test Output

```
🚀 Starting Parquet Writer Unit Tests
==================================================

=== Testing Different Configurations ===

Testing small.parquet: 100 rows, batch size 50
✅ Schema validation passed
✅ Data integrity validation passed (20 rows tested)
✅ Null handling validation passed (20 nulls found)

[... similar output for other configurations ...]

=== Testing File Properties ===
✅ File size: 5,913 bytes
✅ Number of row groups: 1
✅ Total rows: 100
✅ Row group 0: 100 rows, 14,288 bytes
   Column 0: INT32, compression=LZ4, encodings=('PLAIN', 'RLE', 'RLE_DICTIONARY')

==================================================
🎉 All tests passed successfully!
```

## Integration with Build System

The test is integrated into the main test runner (`run.sh`) as the `test_write` cell:

```bash
# Available cells include test_write
bash run.sh --cell=format,build,write,read,test_write

# Common workflows
bash run.sh --cell=build,test_write          # Build and test
bash run.sh --cell=test_write                # Test only
```

## Error Handling

The test provides detailed error messages for failures:

- **Schema mismatches**: Shows expected vs actual column types
- **Data integrity errors**: Reports specific row/column mismatches
- **Null handling issues**: Identifies incorrect null patterns
- **Binary data problems**: Compares expected vs actual binary content

## Temporary Files

The test creates temporary directories for test files and automatically cleans up:

- Test files are created in `/tmp/parquet_test_*`
- All temporary files are removed after testing
- No persistent files are left behind

## Requirements

- **uv**: For automatic dependency management (recommended)
- **Python 3.7+**: For running the test script
- **Built executable**: `a0_write_stream` must be compiled first

## Troubleshooting

### Common Issues

1. **Missing executable**: Run `bash run.sh --cell=build` first
2. **Missing uv**: Install with `curl -LsSf https://astral.sh/uv/install.sh | sh`
3. **Permission errors**: Ensure the test script is executable (`chmod +x test_write_stream.py`)

### Debug Mode

For detailed debugging, modify the test script to:
- Increase the number of tested rows
- Add verbose output for specific test cases
- Preserve temporary files for manual inspection 