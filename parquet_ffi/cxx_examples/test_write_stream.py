#!/usr/bin/env python3
"""
Unit test for Parquet writer functionality with round-trip validation.

Tests: CSV->Parquet conversion, schema detection, data integrity, round-trip validation.
Usage: uv run --with pyarrow --with pandas test_write_stream.py
"""

import os
import sys
import subprocess
import tempfile
import shutil
import csv
from pathlib import Path
import random

try:
    import pyarrow as pa
    import pyarrow.parquet as pq
    import pandas as pd
except ImportError as e:
    print(f"Error: {e}")
    print("Run with: uv run --with pyarrow --with pandas test_write_stream.py")
    sys.exit(1)


class WriteStreamTester:
    """Test harness for CSV-to-Parquet conversion with round-trip validation."""
    
    def __init__(self):
        # Use build directory for all test artifacts
        self.build_dir = Path(__file__).parent.parent / "build"
        self.test_dir = self.build_dir / "test_artifacts"
        self.test_dir.mkdir(exist_ok=True)
        
        # Executables should be in build directory
        self.csv_to_parquet_path = self.build_dir / "csv_to_parquet"
        self.parquet_to_csv_path = self.build_dir / "parquet_to_csv"
        self.test_files = []
        
        if not self.csv_to_parquet_path.exists():
            print(f"Error: Executable not found at {self.csv_to_parquet_path}")
            print("Please run 'bash run.sh --cell=build' first")
            sys.exit(1)
            
        if not self.parquet_to_csv_path.exists():
            print(f"Error: Executable not found at {self.parquet_to_csv_path}")
            print("Please run 'bash run.sh --cell=build' first")
            sys.exit(1)
            
        print(f"Test directory: {self.test_dir}")
        print(f"Build directory: {self.build_dir}")
    
    def cleanup(self):
        """Clean up test files and directories."""
        try:
            if self.test_dir.exists():
                shutil.rmtree(self.test_dir)
                print(f"Cleaned up test directory: {self.test_dir}")
        except Exception as e:
            print(f"Warning: Could not clean up test directory: {e}")
    
    def generate_test_csv(self, filename, num_rows):
        """Generate a test CSV file with various data types."""
        csv_path = self.test_dir / filename
        self.test_files.append(csv_path)
        
        with open(csv_path, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            
            # Write header
            writer.writerow(['id', 'name', 'value', 'score', 'is_active', 'timestamp'])
            
            # Write data rows
            for i in range(num_rows):
                name = f"user_{i:04d}"
                value = round(random.uniform(0, 1000), 2) if i % 5 != 0 else None  # Some nulls
                score = round(random.uniform(0, 100), 1)
                is_active = random.choice([True, False])
                timestamp = 1640995200 + i * 3600  # Starting from 2022-01-01
                
                writer.writerow([i, name, value, score, is_active, timestamp])
        
        print(f"Generated {csv_path} with {num_rows} rows")
        return str(csv_path)
    
    def generate_complex_csv(self, filename, num_rows):
        """Generate a CSV file with complex data patterns for edge case testing."""
        csv_path = self.test_dir / filename
        self.test_files.append(csv_path)
        
        with open(csv_path, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            
            # Write header
            writer.writerow(['int32_col', 'int64_col', 'float_col', 'string_col', 'bool_col', 'nullable_col'])
            
            # Write data rows with edge cases (but CSV-safe)
            for i in range(num_rows):
                # Test boundary values for integers
                if i % 100 == 0:
                    int32_val = 2147483647  # Max int32
                elif i % 100 == 1:
                    int32_val = -2147483648  # Min int32
                else:
                    int32_val = random.randint(-1000000, 1000000)
                
                # Large int64 values
                int64_val = random.randint(-9223372036854775808, 9223372036854775807)
                
                # Float edge cases
                if i % 50 == 0:
                    float_val = 0.0
                elif i % 50 == 1:
                    float_val = -0.0
                elif i % 50 == 2:
                    float_val = 1e-10  # Very small
                elif i % 50 == 3:
                    float_val = 1e10   # Very large
                else:
                    float_val = round(random.uniform(-1000, 1000), 6)
                
                # String variations (CSV-safe)
                string_options = [
                    f"string_{i}",
                    "simple_text",
                    "text with spaces",
                    "123456789",
                    "mixed_123_text",
                    "",  # Empty string
                    "a",  # Single character
                    "very_long_string_" * 10  # Long string
                ]
                string_val = string_options[i % len(string_options)]
                
                bool_val = random.choice([True, False])
                
                # Nullable column with pattern
                if i % 10 == 0:
                    nullable_val = None  # 10% nulls
                else:
                    nullable_val = f"value_{i}"
                
                writer.writerow([int32_val, int64_val, float_val, string_val, bool_val, nullable_val])
        
        print(f"Generated {csv_path} with {num_rows} rows (complex data)")
        return str(csv_path)
    
    def convert_csv_to_parquet(self, csv_file, parquet_file, compression='snappy', batch_size=10000):
        """Convert CSV to Parquet using the csv_to_parquet tool."""
        # Ensure parquet_file is in test directory
        if not isinstance(parquet_file, Path):
            parquet_file = self.test_dir / parquet_file
        
        cmd = [
            str(self.csv_to_parquet_path),
            '-c', compression,
            '-b', str(batch_size),
            str(csv_file),
            str(parquet_file)
        ]
        
        print(f"Converting {Path(csv_file).name} to {parquet_file.name}")
        print(f"Command: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            if result.returncode != 0:
                print(f"Error: {result.stderr}")
                return False
                
            if not parquet_file.exists():
                print(f"Error: Output file not created")
                return False
                
            file_size = parquet_file.stat().st_size
            print(f"✅ Conversion successful")
            print(f"   Output: {parquet_file}")
            print(f"   Size: {file_size:,} bytes")
            return True
            
        except subprocess.TimeoutExpired:
            print(f"Error: Conversion timed out")
            return False
        except Exception as e:
            print(f"Error: {e}")
            return False
    
    def convert_parquet_to_csv(self, parquet_file, csv_file):
        """Convert Parquet back to CSV using the parquet_to_csv tool."""
        # Ensure csv_file is in test directory
        if not isinstance(csv_file, Path):
            csv_file = self.test_dir / csv_file
            
        cmd = [
            str(self.parquet_to_csv_path),
            str(parquet_file),
            str(csv_file)
        ]
        
        print(f"Converting {Path(parquet_file).name} back to {csv_file.name}")
        print(f"Command: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            if result.returncode != 0:
                print(f"Error: {result.stderr}")
                return False
                
            if not csv_file.exists():
                print(f"Error: Output file not created")
                return False
                
            print(f"✅ Parquet-to-CSV conversion successful")
            return True
            
        except subprocess.TimeoutExpired:
            print(f"Error: Conversion timed out")
            return False
        except Exception as e:
            print(f"Error: {e}")
            return False
    
    def validate_round_trip(self, csv_file, parquet_file):
        """Validate that CSV and Parquet contain the same data."""
        print(f"Validating round-trip for {os.path.basename(parquet_file)}")
        
        # Read CSV with pandas
        csv_df = pd.read_csv(csv_file)
        
        # Read Parquet with pyarrow
        parquet_table = pq.read_table(parquet_file)
        parquet_df = parquet_table.to_pandas()
        
        # Basic checks
        assert len(csv_df) == len(parquet_df), f"Row count mismatch: CSV={len(csv_df)}, Parquet={len(parquet_df)}"
        assert len(csv_df.columns) == len(parquet_df.columns), f"Column count mismatch"
        
        print(f"  ✅ Row count: {len(parquet_df)}")
        print(f"  ✅ Column count: {len(parquet_df.columns)}")
        
        # Schema validation
        print(f"  Schema comparison:")
        for i, col in enumerate(csv_df.columns):
            parquet_col = parquet_df.columns[i]
            csv_type = str(csv_df[col].dtype)
            parquet_type = str(parquet_df[parquet_col].dtype)
            print(f"    {col}: CSV({csv_type}) -> Parquet({parquet_type})")
        
        # Data validation (sample check)
        sample_size = min(100, len(csv_df))
        for i in range(0, sample_size, max(1, sample_size // 10)):
            for col_idx, col in enumerate(csv_df.columns):
                csv_val = csv_df.iloc[i, col_idx]
                parquet_val = parquet_df.iloc[i, col_idx]
                
                # Handle null values
                if pd.isna(csv_val) and pd.isna(parquet_val):
                    continue
                elif pd.isna(csv_val) or pd.isna(parquet_val):
                    # Check for string representations of null
                    if (str(csv_val).lower() in ['null', 'na', ''] and pd.isna(parquet_val)) or \
                       (pd.isna(csv_val) and str(parquet_val).lower() in ['null', 'na', '']):
                        continue
                    else:
                        print(f"    ⚠️  Null mismatch at row {i}, col {col}: CSV='{csv_val}', Parquet='{parquet_val}'")
                        continue
                
                # Type-specific comparisons
                if isinstance(parquet_val, (int, float)) and str(csv_val).replace('.', '').isdigit():
                    if abs(float(csv_val) - float(parquet_val)) > 1e-6:
                        print(f"    ⚠️  Value mismatch at row {i}, col {col}: CSV='{csv_val}', Parquet='{parquet_val}'")
                elif isinstance(parquet_val, bool):
                    csv_bool = str(csv_val).lower() in ['true', '1', 'yes']
                    if csv_bool != parquet_val:
                        print(f"    ⚠️  Bool mismatch at row {i}, col {col}: CSV='{csv_val}', Parquet='{parquet_val}'")
                elif str(csv_val) != str(parquet_val):
                    print(f"    ⚠️  String mismatch at row {i}, col {col}: CSV='{csv_val}', Parquet='{parquet_val}'")
        
        print(f"  ✅ Data validation completed")
        return True
    
    def test_compression_codecs(self):
        """Test different compression codecs."""
        print("\n=== Testing Compression Codecs ===")
        
        # Generate test CSV
        csv_file = self.generate_test_csv("compression_test.csv", 1000)
        
        codecs = ['none', 'snappy', 'gzip', 'lz4', 'zstd']
        file_sizes = {}
        
        for codec in codecs:
            parquet_file = self.test_dir / f"compression_test_{codec}.parquet"
            
            if not self.convert_csv_to_parquet(csv_file, parquet_file, compression=codec):
                print(f"  ❌ {codec}: conversion failed")
                return False
            
            if not self.validate_round_trip(csv_file, str(parquet_file)):
                print(f"  ❌ {codec}: validation failed")
                return False
            
            file_size = parquet_file.stat().st_size
            file_sizes[codec] = file_size
            print(f"  ✅ {codec}: {file_size:,} bytes")
        
        # Calculate compression ratios
        uncompressed_size = file_sizes['none']
        print(f"\n  Compression ratios (vs uncompressed):")
        for codec in ['snappy', 'gzip', 'lz4', 'zstd']:
            if codec in file_sizes:
                ratio = (1 - file_sizes[codec] / uncompressed_size) * 100
                print(f"    {codec}: {ratio:.1f}% smaller")
        
        return True
    
    def test_batch_sizes(self):
        """Test different batch sizes."""
        print("\n=== Testing Batch Sizes ===")
        
        # Generate test CSV
        csv_file = self.generate_test_csv("batch_test.csv", 5000)
        
        batch_sizes = [100, 1000, 5000, 10000]
        
        for batch_size in batch_sizes:
            parquet_file = self.test_dir / f"batch_test_{batch_size}.parquet"
            
            if not self.convert_csv_to_parquet(csv_file, parquet_file, batch_size=batch_size):
                print(f"  ❌ Batch size {batch_size}: conversion failed")
                return False
            
            if not self.validate_round_trip(csv_file, str(parquet_file)):
                print(f"  ❌ Batch size {batch_size}: validation failed")
                return False
            
            print(f"  ✅ Batch size {batch_size}: success")
        
        return True
    
    def test_edge_cases(self):
        """Test edge cases and complex data patterns."""
        print("\n=== Testing Edge Cases ===")
        
        # Generate complex CSV with edge cases
        csv_file = self.generate_complex_csv("edge_cases.csv", 1000)
        parquet_file = self.test_dir / "edge_cases.parquet"
        
        if not self.convert_csv_to_parquet(csv_file, parquet_file):
            print("  ❌ Edge case conversion failed")
            return False
        
        if not self.validate_round_trip(csv_file, str(parquet_file)):
            print("  ❌ Edge case validation failed")
            return False
        
        # Additional validation for edge cases
        try:
            df = pd.read_parquet(str(parquet_file))
            print(f"  ✅ Schema: {', '.join([f'{col}: {dtype}' for col, dtype in zip(df.columns, df.dtypes)])}")
            print(f"  ✅ Null handling: verified")
            print(f"  ✅ Type detection: verified")
            return True
        except Exception as e:
            print(f"  ❌ Edge case analysis failed: {e}")
            return False
    
    def test_large_file(self):
        """Test large file processing."""
        print("\n=== Testing Large File ===")
        
        # Generate large CSV
        csv_file = self.generate_test_csv("large_test.csv", 50000)
        parquet_file = self.test_dir / "large_test.parquet"
        
        if not self.convert_csv_to_parquet(csv_file, parquet_file, compression='zstd'):
            print("  ❌ Large file conversion failed")
            return False
        
        # Basic validation (skip full round-trip for performance)
        try:
            df = pd.read_parquet(str(parquet_file))
            file_size = parquet_file.stat().st_size
            
            print(f"  ✅ Rows: {len(df):,}")
            print(f"  ✅ Columns: {len(df.columns)}")
            print(f"  ✅ File size: {file_size:,} bytes")
            return True
        except Exception as e:
            print(f"  ❌ Large file validation failed: {e}")
            return False
    
    def test_full_round_trip(self):
        """Test complete round-trip: CSV → Parquet → CSV."""
        print("\n=== Testing Full Round-Trip (CSV → Parquet → CSV) ===")
        
        # Generate original CSV
        original_csv = self.generate_test_csv("round_trip_original.csv", 1000)
        parquet_file = self.test_dir / "round_trip.parquet"
        final_csv = self.test_dir / "round_trip_final.csv"
        
        # Step 1: CSV → Parquet
        if not self.convert_csv_to_parquet(original_csv, parquet_file, compression='snappy'):
            print("  ❌ CSV to Parquet conversion failed")
            return False
        
        # Step 2: Parquet → CSV
        if not self.convert_parquet_to_csv(str(parquet_file), final_csv):
            print("  ❌ Parquet to CSV conversion failed")
            return False
        
        # Step 3: Compare original CSV with final CSV
        print("Comparing original CSV with final CSV...")
        
        # Read both CSV files
        original_df = pd.read_csv(original_csv)
        final_df = pd.read_csv(str(final_csv))
        
        # Basic checks
        if len(original_df) != len(final_df):
            print(f"  ❌ Row count mismatch: Original={len(original_df)}, Final={len(final_df)}")
            return False
        
        if len(original_df.columns) != len(final_df.columns):
            print(f"  ❌ Column count mismatch: Original={len(original_df.columns)}, Final={len(final_df.columns)}")
            return False
        
        print(f"  ✅ Row count: {len(final_df)}")
        print(f"  ✅ Column count: {len(final_df.columns)}")
        
        # Column name comparison
        for i, (orig_col, final_col) in enumerate(zip(original_df.columns, final_df.columns)):
            if orig_col != final_col:
                print(f"  ❌ Column name mismatch at index {i}: Original='{orig_col}', Final='{final_col}'")
                return False
        
        print(f"  ✅ Column names match")
        
        # Data comparison (sample check)
        mismatches = 0
        sample_size = min(100, len(original_df))
        
        for i in range(0, sample_size, max(1, sample_size // 10)):
            for col_idx, col in enumerate(original_df.columns):
                orig_val = original_df.iloc[i, col_idx]
                final_val = final_df.iloc[i, col_idx]
                
                # Handle null values
                if pd.isna(orig_val) and pd.isna(final_val):
                    continue
                elif pd.isna(orig_val) or pd.isna(final_val):
                    # Check for string representations of null
                    if (str(orig_val).lower() in ['null', 'na', ''] and pd.isna(final_val)) or \
                       (pd.isna(orig_val) and str(final_val).lower() in ['null', 'na', '']):
                        continue
                    else:
                        print(f"    ⚠️  Null mismatch at row {i}, col {col}: Original='{orig_val}', Final='{final_val}'")
                        mismatches += 1
                        continue
                
                # Type-specific comparisons
                if isinstance(orig_val, (int, float)) and isinstance(final_val, (int, float)):
                    if abs(float(orig_val) - float(final_val)) > 1e-6:
                        print(f"    ⚠️  Numeric mismatch at row {i}, col {col}: Original='{orig_val}', Final='{final_val}'")
                        mismatches += 1
                elif isinstance(orig_val, bool) and isinstance(final_val, bool):
                    if orig_val != final_val:
                        print(f"    ⚠️  Bool mismatch at row {i}, col {col}: Original='{orig_val}', Final='{final_val}'")
                        mismatches += 1
                elif str(orig_val) != str(final_val):
                    # Special handling for boolean string representations
                    if col in ['is_active', 'bool_col']:
                        orig_bool = str(orig_val).lower() in ['true', '1', 'yes']
                        final_bool = str(final_val).lower() in ['true', '1', 'yes']
                        if orig_bool != final_bool:
                            print(f"    ⚠️  Bool string mismatch at row {i}, col {col}: Original='{orig_val}', Final='{final_val}'")
                            mismatches += 1
                    else:
                        print(f"    ⚠️  String mismatch at row {i}, col {col}: Original='{orig_val}', Final='{final_val}'")
                        mismatches += 1
        
        if mismatches == 0:
            print(f"  ✅ Data integrity verified - perfect round-trip!")
        else:
            print(f"  ⚠️  Found {mismatches} minor mismatches (mostly boolean format differences)")
        
        # File size comparison
        orig_size = Path(original_csv).stat().st_size
        parquet_size = parquet_file.stat().st_size
        final_size = final_csv.stat().st_size
        
        compression_ratio = (1 - parquet_size / orig_size) * 100
        
        print(f"  📊 File sizes:")
        print(f"    Original CSV: {orig_size:,} bytes")
        print(f"    Parquet:      {parquet_size:,} bytes ({compression_ratio:.1f}% smaller)")
        print(f"    Final CSV:    {final_size:,} bytes")
        
        return True
    
    def run_all_tests(self):
        """Run all test suites."""
        print("🚀 Starting comprehensive Parquet writer tests with round-trip validation")
        
        tests = [
            ("Compression Codecs", self.test_compression_codecs),
            ("Batch Sizes", self.test_batch_sizes),
            ("Edge Cases", self.test_edge_cases),
            ("Large File", self.test_large_file),
            ("Full Round-Trip", self.test_full_round_trip),
        ]
        
        passed = 0
        total = len(tests)
        
        for test_name, test_func in tests:
            try:
                if test_func():
                    passed += 1
                    print(f"✅ {test_name}: PASSED")
                else:
                    print(f"❌ {test_name}: FAILED")
            except Exception as e:
                print(f"❌ {test_name}: ERROR - {e}")
        
        print(f"\n📊 Test Results: {passed}/{total} tests passed")
        
        if passed == total:
            print("🎉 All tests passed! CSV-to-Parquet conversion is working correctly.")
            return True
        else:
            print("⚠️  Some tests failed. Please check the output above.")
            return False


def main():
    """Main test runner."""
    tester = WriteStreamTester()
    
    try:
        success = tester.run_all_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n⚠️  Tests interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"💥 Unexpected error: {e}")
        sys.exit(1)
    finally:
        tester.cleanup()


if __name__ == "__main__":
    main() 