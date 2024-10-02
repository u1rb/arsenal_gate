#!/usr/bin/env python3
"""
Runtime environment manager for duckdb_package.
Sets up the correct environment variables for running duckdb executables.
"""

import os
import sys
import pathlib

def run_command():
    """
    Set up environment variables and output export statements for shell evaluation.
    When run without arguments, it outputs environment variable settings.
    """
    # Get the package directory
    package_dir = pathlib.Path(__file__).parent
    
    # Library path is in the lib subdirectory of the package
    lib_path = str(package_dir / "lib")
    bin_path = str(package_dir / "bin")
    
    # Format the LD_LIBRARY_PATH setting
    current_ld_path = os.environ.get("LD_LIBRARY_PATH", "")
    if current_ld_path:
        if lib_path not in current_ld_path.split(':'):
            new_ld_path = f"{lib_path}:{current_ld_path}"
        else:
            new_ld_path = current_ld_path
    else:
        new_ld_path = lib_path
    
    # Format the PATH setting
    current_path = os.environ.get("PATH", "")
    if current_path:
        if bin_path not in current_path.split(':'):
            new_path = f"{bin_path}:{current_path}"
        else:
            new_path = current_path
    else:
        new_path = bin_path
    
    # Output export statements for shell evaluation
    print(f"export LD_LIBRARY_PATH=\"{new_ld_path}\"")
    print(f"export PATH=\"{new_path}\"")
    print("# DuckDB environment variables set")

if __name__ == "__main__":
    run_command() 