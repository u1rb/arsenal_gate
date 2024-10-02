# Get CELL_LIST from the command line argument
CELL_LIST=""
# Extract cell list from arguments (supports both --cell=1,2,3 and --cell 1,2,3)
for arg in "$@"; do
    if [[ $arg == --cell=* ]]; then
        CELL_LIST="${arg#*=}"
        break
    elif [[ $prev == --cell ]]; then
        CELL_LIST="$arg"
        break
    fi
    prev="$arg"
done

echo "CELL_LIST: $CELL_LIST"

# Simplified function to check if a cell number exists in the list
has_cell() {
    [[ ",$CELL_LIST," == *",$1,"* ]]
}

if has_cell "clean"; then
    rm -rf duckdb_package_py/.venv
    rm -rf duckdb_package_py/duckdb_package/lib
    rm -rf duckdb_package_py/duckdb_package/bin
    rm -rf duckdb_package_py/dist
fi

# If CELL_LIST doesn't contain 1, skip the package build
if has_cell "1"; then
    bash ./build_package.sh
fi

if has_cell "2"; then
cat <<EOF > local_data/Dockerfile
FROM debian:sid

# Install Python and pip
RUN apt-get update && apt-get install -y python3 python3-pip

# Create app directory
WORKDIR /app

# Copy the wheel file
COPY ./duckdb_package_py/dist/*.tar.gz /app/
COPY --from=ghcr.io/astral-sh/uv:0.6.4 /uv /uvx /bin/

# Install the wheel
RUN bash -c "uv venv \
    && source .venv/bin/activate \
    && uv pip install *.tar.gz"

# Copy the build directory for application files
COPY ./build /app/build
EOF

docker build -t duckdb-test -f local_data/Dockerfile .

fi

if has_cell "3"; then
docker run -it \
        --rm \
        -w /app/build \
        duckdb-test \
        /bin/bash -c "source /app/.venv/bin/activate && eval \$(duckdb-setup-env) && main"
fi

if has_cell "3-debug"; then
docker run -it \
        --rm \
        -w /app/build \
        duckdb-test \
        /bin/bash
fi
