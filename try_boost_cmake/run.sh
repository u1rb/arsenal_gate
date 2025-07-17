#!/bin/bash
set -euo pipefail

clean() { 
    echo "Cleaning build..."; rm -rf build; 
}
build() {
    mkdir -p build
    cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build -j
}
run() { 
    echo -e "Running the program...\n===================="
    ./build/main
}

[[ "${1:-}" != --cell=* ]] && { echo "Usage: $0 --cell=clean,build,run"; exit 1; }
IFS=',' read -ra CELLS <<< "${1#--cell=}"
for cell in "${CELLS[@]}"; do
    type -t "$cell" &>/dev/null || { echo "Unknown: $cell"; exit 1; }
    $cell
done