#!/usr/bin/env python3

import argparse
import sqlite3
import sys
import os

from hashFiles import eval_key

def init_db(db_path):
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    conn = sqlite3.connect(db_path)
    
    # Enable Write-Ahead Logging for better concurrency
    conn.execute("PRAGMA journal_mode=WAL")
    # Set busy timeout to 5000ms (5 seconds)
    conn.execute("PRAGMA busy_timeout=5000")
    # Ensure data is synced to disk
    conn.execute("PRAGMA synchronous=NORMAL")
    
    c = conn.cursor()
    c.execute("""CREATE TABLE IF NOT EXISTS cache
                 (key TEXT PRIMARY KEY, value TEXT)""")
    conn.commit()
    return conn


def get_cache(conn, key):
    """Get a value from cache."""
    processed_key = eval_key(key)
    c = conn.cursor()
    c.execute("SELECT value FROM cache WHERE key = ?", (processed_key,))
    result = c.fetchone()
    return result is not None


def set_cache(conn, key, value="1"):
    """Set a value in cache."""
    processed_key = eval_key(key)
    c = conn.cursor()
    c.execute(
        "INSERT OR REPLACE INTO cache (key, value) VALUES (?, ?)",
        (processed_key, value),
    )
    conn.commit()


def main():
    parser = argparse.ArgumentParser(description="Command caching utility")
    parser.add_argument("--db", required=True, help="Path to SQLite database")
    parser.add_argument(
        "--action", required=True, choices=["get", "set"], help="Action to perform"
    )
    parser.add_argument("--key", required=True, help="Cache key")
    parser.add_argument("--value", default="1", help="Value to cache (for set action)")

    args = parser.parse_args()

    # Ensure database directory exists
    conn = init_db(args.db)

    try:
        if args.action == "get":
            exists = get_cache(conn, args.key)
            sys.exit(0 if exists else 1)
        elif args.action == "set":
            set_cache(conn, args.key, args.value)
            sys.exit(0)
    finally:
        conn.close()


if __name__ == "__main__":
    main()
