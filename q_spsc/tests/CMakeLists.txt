enable_testing()

# Unit tests
add_executable(test_spsc test_spsc.cpp)
target_link_libraries(test_spsc PRIVATE q_spsc::q_spsc)
target_compile_options(test_spsc PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -pthread>
)

# Performance tests
add_executable(test_performance test_performance.cpp)
target_link_libraries(test_performance PRIVATE q_spsc::q_spsc)
target_compile_options(test_performance PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O3 -pthread>
)

# Add tests
add_test(NAME unit_tests COMMAND test_spsc)
add_test(NAME performance_tests COMMAND test_performance)