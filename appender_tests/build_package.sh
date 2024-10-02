#!/bin/bash
set -e

mkdir -p duckdb_package_py/duckdb_package/lib
mkdir -p duckdb_package_py/duckdb_package/bin

echo "Copying libduckdb.so from build/_deps/duckdb-src/ to package directory..."
cp build/_deps/duckdb-src/libduckdb.so duckdb_package_py/duckdb_package/lib/

echo "Copying main executable to package bin directory..."
cp build/main duckdb_package_py/duckdb_package/bin/

echo "Building Python package..."
cd duckdb_package_py

uv venv
source .venv/bin/activate

uv pip install build
echo "Building Python package using modern build tool..."
uv build