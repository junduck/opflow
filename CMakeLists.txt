cmake_minimum_required(VERSION 3.22)
project(opflow)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wconversion")

option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_EXAMPLES "Build examples" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(opflow_hdr INTERFACE)
target_include_directories(opflow_hdr INTERFACE ${PROJECT_SOURCE_DIR}/include)

# For header library
add_library(opflow::headers ALIAS opflow_hdr)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR BUILD_BENCHMARKS)
  # Fetch Google test and benchmark
  include(cmake/GoogleBenchAndTest.cmake)

  # Add private dev headers
  add_library(private_headers INTERFACE)
  target_include_directories(private_headers INTERFACE ${PROJECT_SOURCE_DIR}/private)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  # Dummy TU to make clangd happy
  add_executable(_clangd_tu src/_clangd_tu.cpp)
  target_link_libraries(_clangd_tu PRIVATE opflow::headers)

  # enable testing
  enable_testing()
  add_subdirectory(test)
endif()

if(BUILD_BENCHMARKS)
  # enable benchmarking
  add_subdirectory(bench)
endif()

IF(BUILD_EXAMPLES)
  # enable examples
  add_subdirectory(examples)
endif()
