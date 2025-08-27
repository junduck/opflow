add_executable(test_main main.cpp)
target_link_libraries(test_main GTest::gtest_main private_headers opflow::headers)

# Topo graph tests
add_executable(test_graph_topo graph_topo_test.cpp)
target_link_libraries(test_graph_topo GTest::gtest_main opflow::headers)

# History tests
add_executable(test_history_deque history_deque_test.cpp)
target_link_libraries(test_history_deque GTest::gtest_main opflow::headers)

add_executable(test_history_ringbuf history_ringbuf_test.cpp)
target_link_libraries(test_history_ringbuf GTest::gtest_main opflow::headers)

add_executable(test_history_buffer history_buffer_test.cpp)
target_link_libraries(test_history_buffer GTest::gtest_main opflow::headers)

add_executable(test_history_comparison history_comparison_test.cpp)
target_link_libraries(test_history_comparison GTest::gtest_main opflow::headers)

# Iterator tests
add_executable(test_iterator iterator_test.cpp)
target_link_libraries(test_iterator GTest::gtest_main opflow::headers)

# Flat multivect tests
add_executable(test_flat_multivect flat_multivect_test.cpp flat_multivect_pmr_test.cpp)
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

# Chrono tests
add_executable(test_chrono chrono_test.cpp)
target_link_libraries(test_chrono GTest::gtest_main opflow::headers)

# Graph executor tests
add_executable(test_op_dag_exec op_dag_exec_test.cpp)
target_link_libraries(test_op_dag_exec GTest::gtest_main opflow::headers)

# sorted_vect tests
add_executable(test_sorted_vect sorted_vect_test.cpp)
target_link_libraries(test_sorted_vect GTest::gtest_main opflow::headers)

# functor tests
add_executable(test_functor functor_test.cpp)
target_link_libraries(test_functor GTest::gtest_main opflow::headers)

# ringbuf_vect tests
add_executable(test_ringbuf_vect ringbuf_vect_test.cpp)
target_link_libraries(test_ringbuf_vect GTest::gtest_main opflow::headers)

# tumbling window tests
add_executable(test_tumbling_window tumbling_win_test.cpp)
target_link_libraries(test_tumbling_window GTest::gtest_main opflow::headers)

# graph_node tests
add_executable(test_graph_node graph_node_test.cpp)
target_link_libraries(test_graph_node GTest::gtest_main opflow::headers)

# vector store tests
add_executable(test_vector_store vector_store_test.cpp)
target_link_libraries(test_vector_store GTest::gtest_main opflow::headers)

include(GoogleTest)
gtest_discover_tests(test_main)
gtest_discover_tests(test_graph_topo)
gtest_discover_tests(test_vector_store)
gtest_discover_tests(test_graph_node)
gtest_discover_tests(test_tumbling_window)
gtest_discover_tests(test_history_deque)
gtest_discover_tests(test_history_ringbuf)
gtest_discover_tests(test_history_buffer)
gtest_discover_tests(test_history_comparison)
gtest_discover_tests(test_iterator)
gtest_discover_tests(test_flat_multivect)
gtest_discover_tests(test_flat_set)
gtest_discover_tests(test_flat_set_edge_cases)
gtest_discover_tests(test_flat_set_corner_cases)
gtest_discover_tests(test_chrono)
gtest_discover_tests(test_op_dag_exec)
gtest_discover_tests(test_sorted_vect)
gtest_discover_tests(test_functor)
gtest_discover_tests(test_ringbuf_vect)
