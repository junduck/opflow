add_executable(dependency_map_example dependency_map_example.cpp)
target_link_libraries(dependency_map_example opflow::headers)

add_executable(engine_integration_test engine_integration_test.cpp)
target_link_libraries(engine_integration_test opflow::headers)

add_executable(engine_builder_example engine_builder_example.cpp)
target_link_libraries(engine_builder_example opflow::headers)
