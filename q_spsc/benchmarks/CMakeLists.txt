add_executable(latency_benchmark latency_benchmark.cpp)
target_link_libraries(latency_benchmark PRIVATE q_spsc::q_spsc)
target_compile_options(latency_benchmark PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O3 -march=native>
)

add_executable(throughput_benchmark throughput_benchmark.cpp)
target_link_libraries(throughput_benchmark PRIVATE q_spsc::q_spsc)
target_compile_options(throughput_benchmark PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O3 -march=native>
)