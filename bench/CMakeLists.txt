# Graph benchmarks
# add_executable(bench_graph graph_bench.cpp)
# target_link_libraries(bench_graph PRIVATE benchmark::benchmark opflow::headers)

# S2 Exponentially Weighted Statistics benchmark
add_executable(bench_s2_exp_weighted s2_exp_weighted_benchmark.cpp)
target_link_libraries(bench_s2_exp_weighted PRIVATE benchmark::benchmark opflow::headers private_headers)

# Flat Set benchmark
add_executable(bench_flat_set flat_set_benchmark.cpp)
target_link_libraries(bench_flat_set PRIVATE benchmark::benchmark opflow::headers)

# Simple test for s2_exp_weighted_sta (no benchmark dependency)
add_executable(s2_simple_test s2_simple_test.cpp)
target_link_libraries(s2_simple_test PRIVATE opflow::headers private_headers)

# Test for inlined traits compared to virtual function call
add_executable(inlined_trait inlined_trait.cpp)
target_link_libraries(inlined_trait PRIVATE benchmark::benchmark)

# Test for sorted container access patterns
add_executable(bench_sorted_lookup sorted_lookup.cpp)
target_link_libraries(bench_sorted_lookup PRIVATE benchmark::benchmark)

add_executable(bench_vtbl vtbl_benchmark.cpp)
target_link_libraries(bench_vtbl PRIVATE benchmark::benchmark opflow::headers)

add_executable(bench_rolling_min rolling_min.cpp)
target_link_libraries(bench_rolling_min PRIVATE benchmark::benchmark opflow::headers)

# agg model benchmark
add_executable(bench_agg_model agg_model.cpp)
target_link_libraries(bench_agg_model PRIVATE benchmark::benchmark )
