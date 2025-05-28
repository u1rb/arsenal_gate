#!/bin/bash
set -ueo pipefail

cd `dirname $0`

cargo b 
cargo test 

mkdir -p build
cd build

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .