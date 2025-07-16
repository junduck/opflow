add_executable(test_main main.cpp)
target_link_libraries(test_main GTest::gtest_main private_headers opflow::headers)

# Engine tests
add_executable(test_engine engine_test.cpp)
target_link_libraries(test_engine GTest::gtest_main opflow::headers)

# Dependency map tests
add_executable(test_dependency_map dependency_map_test.cpp)
target_link_libraries(test_dependency_map GTest::gtest_main opflow::headers)

# Graph tests
add_executable(test_topo topo_test.cpp)
target_link_libraries(test_topo GTest::gtest_main opflow::headers)

# Topo sorter tests
add_executable(test_history_deque history_deque_test.cpp)
target_link_libraries(test_history_deque GTest::gtest_main opflow::headers)

add_executable(test_history_ringbuf history_ringbuf_test.cpp)
target_link_libraries(test_history_ringbuf GTest::gtest_main opflow::headers)

add_executable(test_history_comparison history_comparison_test.cpp)
target_link_libraries(test_history_comparison GTest::gtest_main opflow::headers)

include(GoogleTest)
gtest_discover_tests(test_main)
gtest_discover_tests(test_engine)
gtest_discover_tests(test_dependency_map)
gtest_discover_tests(test_topo)
gtest_discover_tests(test_history_deque)
gtest_discover_tests(test_history_ringbuf)
gtest_discover_tests(test_history_comparison)
