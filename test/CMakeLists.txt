add_executable(test_main main.cpp)
target_link_libraries(test_main GTest::gtest_main private_headers opflow::headers)

# Graph tests
add_executable(test_graph graph_test.cpp)
target_link_libraries(test_graph GTest::gtest_main opflow::headers)

include(GoogleTest)
gtest_discover_tests(test_main)
gtest_discover_tests(test_graph)
