# Graph benchmarks
# add_executable(bench_graph graph_bench.cpp)
# target_link_libraries(bench_graph PRIVATE benchmark::benchmark opflow::headers)

# S2 Exponentially Weighted Statistics benchmark
add_executable(bench_s2_exp_weighted s2_exp_weighted_benchmark.cpp)
target_link_libraries(bench_s2_exp_weighted PRIVATE benchmark::benchmark opflow::headers private_headers)

# Simple test for s2_exp_weighted_sta (no benchmark dependency)
add_executable(s2_simple_test s2_simple_test.cpp)
target_link_libraries(s2_simple_test PRIVATE opflow::headers private_headers)
