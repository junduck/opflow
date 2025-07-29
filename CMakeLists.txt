cmake_minimum_required(VERSION 3.22)
project(opflow)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wconversion")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(opflow_hdr INTERFACE)
target_include_directories(opflow_hdr INTERFACE ${PROJECT_SOURCE_DIR}/include)

# For header library
add_library(opflow::headers ALIAS opflow_hdr)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  # Fetch Google test and benchmark
  include(cmake/GoogleBenchAndTest.cmake)

  # Dummy TU to make clangd happy
  add_executable(_clangd_tu src/_clangd_tu.cpp)
  target_link_libraries(_clangd_tu PRIVATE opflow::headers)

  # Add private dev headers
  add_library(private_headers INTERFACE)
  target_include_directories(private_headers INTERFACE ${PROJECT_SOURCE_DIR}/private)

  # enable testing
  enable_testing()
  add_subdirectory(test)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  # Fetch Google test and benchmark
  include(cmake/GoogleBenchAndTest.cmake)

  # Add private dev headers
  add_library(private_headers INTERFACE)
  target_include_directories(private_headers INTERFACE ${PROJECT_SOURCE_DIR}/private)

  # enable benchmarking
  add_subdirectory(bench)
endif()

# add examples
add_subdirectory(examples)
