# Graph benchmarks
add_executable(bench_graph graph_bench.cpp)
target_link_libraries(bench_graph PRIVATE benchmark::benchmark opflow::headers)
