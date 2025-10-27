cmake_minimum_required(VERSION 3.22)
project(opflow)

# Options to control building tests and benchmarks
option(BUILD_TESTING "Build the testing tree" ON)
option(BUILD_BENCHMARKS "Build the benchmarks" ON)
option(BUILD_EXAMPLES "Build the examples" ON)
option(ERROR_ON_WARNING "Treat warnings as errors" OFF)

# Set compiler warning flags based on compiler
if(MSVC)
  # MSVC warning flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  if(ERROR_ON_WARNING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
  endif()
else()
  # GCC/Clang warning flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wconversion")
  if(ERROR_ON_WARNING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(opflow_hdr INTERFACE)
target_include_directories(opflow_hdr INTERFACE ${PROJECT_SOURCE_DIR}/include)

# For header library
add_library(opflow::headers ALIAS opflow_hdr)


if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  # Dummy TU to make clangd happy
  add_executable(_clangd_tu src/_clangd_tu.cpp)
  target_link_libraries(_clangd_tu PRIVATE opflow::headers)
endif()

# Fetch Google test and benchmark if needed
if(BUILD_TESTING OR BUILD_BENCHMARKS)
  include(cmake/GoogleBenchAndTest.cmake)

  # Add private dev headers
  add_library(private_headers INTERFACE)
  target_include_directories(private_headers INTERFACE ${PROJECT_SOURCE_DIR}/private)
endif()

# Enable testing
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

# Enable benchmarking
if(BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

# Enable examples
IF(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
