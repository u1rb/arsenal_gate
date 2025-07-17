add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example PRIVATE q_spsc::q_spsc)
target_compile_options(basic_example PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(hft_example hft_example.cpp)
target_link_libraries(hft_example PRIVATE q_spsc::q_spsc)
target_compile_options(hft_example PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O3>
)

add_executable(opaque_data_example opaque_data_example.cpp)
target_link_libraries(opaque_data_example PRIVATE q_spsc::q_spsc)
target_compile_options(opaque_data_example PRIVATE 
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)