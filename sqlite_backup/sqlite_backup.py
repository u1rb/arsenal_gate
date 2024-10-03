import sqlite3
import os
import sys
import tempfile
import zstandard as zstd

def backup_trading_status_paths(source_db_path, backup_db_path, source_table, compression_level=3):
    """
    Copies files specified in the tradingStatusPath column from the source SQLite database
    into a new backup SQLite database as compressed BLOBs using Zstandard.

    Args:
        source_db_path (str): Path to the source SQLite database (e.g., 'job_index.sqlite').
        backup_db_path (str): Path to the backup SQLite database to create (e.g., 'backup.sqlite').
        source_table (str): Name of the table in the source database containing tradingStatusPath.
        compression_level (int): Zstandard compression level (1-22). Default is 3.
    """
    # Check if zstandard is installed
    try:
        import zstandard as zstd
    except ImportError:
        print("The 'zstandard' library is not installed. Please install it using 'pip install zstandard'.")
        sys.exit(1)

    # Validate compression_level
    if not (1 <= compression_level <= 22):
        print("Invalid compression level. Please choose a level between 1 (fastest) and 22 (maximum compression).")
        sys.exit(1)

    # Check if source database exists
    if not os.path.exists(source_db_path):
        print(f"Source database '{source_db_path}' does not exist.")
        sys.exit(1)

    # Connect to the source database
    try:
        conn_src = sqlite3.connect(source_db_path)
        cursor_src = conn_src.cursor()
    except sqlite3.Error as e:
        print(f"Error connecting to source database: {e}")
        sys.exit(1)

    # Connect to the backup database (it will be created if it doesn't exist)
    try:
        conn_bak = sqlite3.connect(backup_db_path)
        cursor_bak = conn_bak.cursor()
    except sqlite3.Error as e:
        print(f"Error connecting to backup database: {e}")
        conn_src.close()
        sys.exit(1)

    # Create the backup table
    try:
        cursor_bak.execute("""
            CREATE TABLE IF NOT EXISTS backup (
                tradingStatusPath TEXT PRIMARY KEY,
                data BLOB
            )
        """)
        conn_bak.commit()
    except sqlite3.Error as e:
        print(f"Error creating backup table: {e}")
        conn_src.close()
        conn_bak.close()
        sys.exit(1)

    # Retrieve all tradingStatusPath entries from the source table
    try:
        cursor_src.execute(f"SELECT tradingStatusPath FROM {source_table}")
        paths = cursor_src.fetchall()
    except sqlite3.Error as e:
        print(f"Error querying source database: {e}")
        conn_src.close()
        conn_bak.close()
        sys.exit(1)

    total_paths = len(paths)
    print(f"Found {total_paths} tradingStatusPath entries to backup.")

    # Initialize Zstandard compressor
    compressor = zstd.ZstdCompressor(level=compression_level)

    for idx, (path,) in enumerate(paths, start=1):
        print(f"Processing {idx}/{total_paths}: {path}")

        if not path:
            print(f"  [Warning] Empty path encountered. Skipping.")
            continue

        if not os.path.isabs(path):
            print(f"  [Warning] Path is not absolute: {path}. Skipping.")
            continue

        if not os.path.exists(path):
            print(f"  [Warning] File does not exist: {path}. Skipping.")
            continue

        try:
            with open(path, 'rb') as infile, tempfile.NamedTemporaryFile(delete=True) as tmpfile:
                # Create a write buffer to the temporary file
                with compressor.stream_writer(tmpfile) as compressor_writer:
                    while True:
                        chunk = infile.read(65536)  # Read in 64KB chunks
                        if not chunk:
                            break
                        compressor_writer.write(chunk)
                # After compression, read the compressed data from the temp file
                tmpfile.flush()
                tmpfile.seek(0)
                compressed_data = tmpfile.read()
        except IOError as e:
            print(f"  [Error] Failed to read or compress file '{path}': {e}. Skipping.")
            continue
        except zstd.ZstdError as e:
            print(f"  [Error] Zstandard compression failed for '{path}': {e}. Skipping.")
            continue

        try:
            cursor_bak.execute("""
                INSERT OR REPLACE INTO backup (tradingStatusPath, data)
                VALUES (?, ?)
            """, (path, compressed_data))
        except sqlite3.Error as e:
            print(f"  [Error] Failed to insert data for '{path}': {e}. Skipping.")
            continue

        # Commit after each insert to ensure data is written incrementally
        try:
            conn_bak.commit()
        except sqlite3.Error as e:
            print(f"  [Error] Failed to commit data for '{path}': {e}.")
            continue

    print("Backup process completed.")

    # Close the database connections
    conn_src.close()
    conn_bak.close()

if __name__ == "__main__":
    # Example usage:
    # python backup_script_compressed.py job_index.sqlite backup_compressed.sqlite jobs

    if len(sys.argv) not in [4, 5]:
        print("Usage: python backup_script_compressed.py <source_db> <backup_db> <source_table> [<compression_level>]")
        print("  <compression_level> is optional (1-22). Default is 3.")
        sys.exit(1)

    source_db = sys.argv[1]
    backup_db = sys.argv[2]
    source_table = sys.argv[3]
    compression_level = 3  # Default compression level

    if len(sys.argv) == 5:
        try:
            compression_level = int(sys.argv[4])
        except ValueError:
            print("Compression level must be an integer between 1 and 22.")
            sys.exit(1)

    backup_trading_status_paths(source_db, backup_db, source_table, compression_level)