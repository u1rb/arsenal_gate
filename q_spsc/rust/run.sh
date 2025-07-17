#!/bin/bash

cd `dirname $BASH_SOURCE[0]`

echo "Building and Testing SPSC Queue..."
echo "=================================="

# Build the project
echo -e "\nBuilding project..."
cargo build --release

# Run tests
echo -e "\nRunning tests..."
cargo test --release

# Check if tests passed
if [ $? -ne 0 ]; then
    echo "Tests failed! Exiting..."
    exit 1
fi

echo -e "\nRunning SPSC Queue Benchmarks..."
echo "================================"

# Single thread latency benchmarks
echo -e "\n1. Single Thread Latency (write + read in same thread):"
cargo bench --bench spsc_bench single_thread_latency 2>/dev/null | grep -E "time:|32|128|512"

# Producer hot path benchmarks  
echo -e "\n2. Producer Hot Path Latency:"
cargo bench --bench spsc_bench producer_hot_path/32 2>/dev/null | grep "time:"
cargo bench --bench spsc_bench producer_hot_path/128 2>/dev/null | grep "time:"
cargo bench --bench spsc_bench producer_hot_path/512 2>/dev/null | grep "time:"

# Ping pong benchmark
echo -e "\n3. Ping-Pong Inter-Thread Latency:"
cargo bench --bench spsc_bench ping_pong 2>/dev/null | grep "time:"

# Throughput benchmarks
echo -e "\n4. Throughput (messages/sec):"
for size in 16 64 256 1024; do
    echo -n "  ${size} bytes: "
    cargo bench --bench spsc_bench throughput/${size} 2>/dev/null | grep "time:" | awk -v size=$size '
    {
        # Extract time value and unit from format: throughput/16           time:   [7.6098 µs 8.6683 µs 10.116 µs]
        gsub(/.*\[/, "");
        gsub(/\].*/, "");
        
        # Split the values
        split($0, vals, " ");
        
        # Get the middle (typical) value and unit
        typical = vals[3];
        unit = vals[4];
        
        # Convert to nanoseconds
        if (unit == "µs") {
            ns_per_iter = typical * 1000;
        } else if (unit == "ms") {
            ns_per_iter = typical * 1000000;
        } else if (unit == "ns") {
            ns_per_iter = typical;
        } else {
            ns_per_iter = typical * 1000000000;
        }
        
        # Calculate throughput (1000 messages per iteration)
        msgs_per_iter = 1000;
        ns_per_msg = ns_per_iter / msgs_per_iter;
        msgs_per_sec = 1000000000 / ns_per_msg;
        throughput_mb = (msgs_per_sec * size) / 1048576;
        
        printf "%.2f M msgs/sec (%.2f MB/s) - %.1f ns/msg\n", msgs_per_sec/1000000, throughput_mb, ns_per_msg;
    }'
done