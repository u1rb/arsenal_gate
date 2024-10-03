import sqlite3
import os
import sys

def backup_trading_status_paths(source_db_path, backup_db_path, source_table):
    """
    Copies files specified in the tradingStatusPath column from the source SQLite database
    into a new backup SQLite database as BLOBs.

    Args:
        source_db_path (str): Path to the source SQLite database (e.g., 'job_index.sqlite').
        backup_db_path (str): Path to the backup SQLite database to create (e.g., 'backup.sqlite').
        source_table (str): Name of the table in the source database containing tradingStatusPath.
    """
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
            with open(path, 'rb') as file:
                # Read the file in chunks to handle large files
                # However, sqlite3 requires the full BLOB to insert
                # This approach reads the entire file at once
                # Ensure that individual files are not larger than available memory
                data = file.read()
        except IOError as e:
            print(f"  [Error] Failed to read file '{path}': {e}. Skipping.")
            continue

        try:
            cursor_bak.execute("""
                INSERT OR REPLACE INTO backup (tradingStatusPath, data)
                VALUES (?, ?)
            """, (path, data))
        except sqlite3.Error as e:
            print(f"  [Error] Failed to insert data for '{path}': {e}. Skipping.")
            continue

        # Commit after each insert to ensure data is written incrementally
        # This reduces memory usage by not holding a large transaction
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
    # python backup_script.py job_index.sqlite backup.sqlite jobs

    if len(sys.argv) != 4:
        print("Usage: python backup_script.py <source_db> <backup_db> <source_table>")
        sys.exit(1)

    source_db = sys.argv[1]
    backup_db = sys.argv[2]
    source_table = sys.argv[3]

    backup_trading_status_paths(source_db, backup_db, source_table)