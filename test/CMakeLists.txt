add_executable(test_main main.cpp)
target_link_libraries(test_main GTest::gtest_main private_headers opflow::headers)

# Topo graph tests
add_executable(test_topo_graph topo_graph_test.cpp)
target_link_libraries(test_topo_graph GTest::gtest_main opflow::headers)

add_executable(test_graph graph_test.cpp)
target_link_libraries(test_graph GTest::gtest_main opflow::headers)

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

# EWMA operator tests
add_executable(test_op_ewma op_ewma_test.cpp)
target_link_libraries(test_op_ewma GTest::gtest_main opflow::headers private_headers)

# sorted_vect tests
add_executable(test_sorted_vect sorted_vect_test.cpp)
target_link_libraries(test_sorted_vect GTest::gtest_main opflow::headers)

# ringbuf_vect tests
add_executable(test_ringbuf_vect ringbuf_vect_test.cpp)
target_link_libraries(test_ringbuf_vect GTest::gtest_main opflow::headers)

include(GoogleTest)
gtest_discover_tests(test_main)
gtest_discover_tests(test_topo_graph)
gtest_discover_tests(test_graph)
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
gtest_discover_tests(test_op_ewma)
gtest_discover_tests(test_sorted_vect)
gtest_discover_tests(test_ringbuf_vect)
