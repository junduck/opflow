add_executable(test_main main.cpp)
target_link_libraries(test_main GTest::gtest_main private_headers opflow::headers)

# Engine tests
add_executable(test_engine engine_test.cpp)
target_link_libraries(test_engine GTest::gtest_main opflow::headers)

# Dependency map tests
add_executable(test_dependency_map dependency_map_test.cpp)
target_link_libraries(test_dependency_map GTest::gtest_main opflow::headers)

# Topo sorter tests
add_executable(test_topo topo_test.cpp)
target_link_libraries(test_topo GTest::gtest_main opflow::headers)

# Topo graph tests
add_executable(test_topo_graph topo_graph_test.cpp)
target_link_libraries(test_topo_graph GTest::gtest_main opflow::headers)

add_executable(test_topo_bfs topo_bfs_test.cpp)
target_link_libraries(test_topo_bfs GTest::gtest_main opflow::headers)

# Sorted graph tests
add_executable(test_sorted_graph sorted_graph_test.cpp)
target_link_libraries(test_sorted_graph GTest::gtest_main opflow::headers)

# History tests
add_executable(test_history_deque history_deque_test.cpp)
target_link_libraries(test_history_deque GTest::gtest_main opflow::headers)

add_executable(test_history_ringbuf history_ringbuf_test.cpp)
target_link_libraries(test_history_ringbuf GTest::gtest_main opflow::headers)

add_executable(test_history_comparison history_comparison_test.cpp)
target_link_libraries(test_history_comparison GTest::gtest_main opflow::headers)

# Iterator tests
add_executable(test_iterator iterator_test.cpp)
target_link_libraries(test_iterator GTest::gtest_main opflow::headers)

# Flat multivect tests
add_executable(test_flat_multivect flat_multivect_test.cpp)
target_link_libraries(test_flat_multivect GTest::gtest_main opflow::headers)

# Flat set tests
add_executable(test_flat_set flat_set_test.cpp)
target_link_libraries(test_flat_set GTest::gtest_main opflow::headers)

# Flat set edge cases tests
add_executable(test_flat_set_edge_cases flat_set_edge_cases_test.cpp)
target_link_libraries(test_flat_set_edge_cases GTest::gtest_main opflow::headers)

# Flat set corner cases tests
add_executable(test_flat_set_corner_cases flat_set_corner_cases_test.cpp)
target_link_libraries(test_flat_set_corner_cases GTest::gtest_main opflow::headers)

# OHLC operator tests
add_executable(test_ohlc ohlc_test.cpp)
target_link_libraries(test_ohlc GTest::gtest_main opflow::headers)

# Chrono tests
add_executable(test_chrono chrono_test.cpp)
target_link_libraries(test_chrono GTest::gtest_main opflow::headers)

# Pipeline tests
add_executable(test_pipeline pipeline_test.cpp)
target_link_libraries(test_pipeline GTest::gtest_main opflow::headers)

# Pipeline comprehensive tests
add_executable(test_pipeline_comprehensive pipeline_comprehensive_test.cpp)
target_link_libraries(test_pipeline_comprehensive GTest::gtest_main opflow::headers)

include(GoogleTest)
gtest_discover_tests(test_main)
gtest_discover_tests(test_engine)
gtest_discover_tests(test_dependency_map)
gtest_discover_tests(test_topo)
gtest_discover_tests(test_topo_graph)
gtest_discover_tests(test_topo_bfs)
gtest_discover_tests(test_sorted_graph)
gtest_discover_tests(test_history_deque)
gtest_discover_tests(test_history_ringbuf)
gtest_discover_tests(test_history_comparison)
gtest_discover_tests(test_iterator)
gtest_discover_tests(test_flat_multivect)
gtest_discover_tests(test_flat_set)
gtest_discover_tests(test_flat_set_edge_cases)
gtest_discover_tests(test_flat_set_corner_cases)
gtest_discover_tests(test_ohlc)
gtest_discover_tests(test_chrono)
gtest_discover_tests(test_pipeline)
gtest_discover_tests(test_pipeline_comprehensive)
