add_executable(test_main main.cpp)
target_link_libraries(test_main GTest::gtest_main private_headers opflow::headers)

# Engine tests
add_executable(test_engine engine_test.cpp)
target_link_libraries(test_engine GTest::gtest_main opflow::headers)

# Flat graph tests
add_executable(test_flat_graph flat_graph_test.cpp)
target_link_libraries(test_flat_graph GTest::gtest_main opflow::headers)

# Topo sorter tests
add_executable(test_topo topo_test.cpp)
target_link_libraries(test_topo GTest::gtest_main opflow::headers)

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

# OHLC operator tests
add_executable(test_ohlc ohlc_test.cpp)
target_link_libraries(test_ohlc GTest::gtest_main opflow::headers)

include(GoogleTest)
gtest_discover_tests(test_main)
gtest_discover_tests(test_engine)
gtest_discover_tests(test_flat_graph)
gtest_discover_tests(test_topo)
gtest_discover_tests(test_topo_bfs)
gtest_discover_tests(test_sorted_graph)
gtest_discover_tests(test_history_deque)
gtest_discover_tests(test_history_ringbuf)
gtest_discover_tests(test_history_comparison)
gtest_discover_tests(test_iterator)
gtest_discover_tests(test_flat_multivect)
gtest_discover_tests(test_ohlc)
