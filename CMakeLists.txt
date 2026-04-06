cmake_minimum_required(VERSION 3.14)
project(perfmap
  VERSION 1.0.0
  LANGUAGES CXX
  DESCRIPTION "PerfMap: A Cache-Aware Hash Map Performance Engineering Lab"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Dependencies via FetchContent
# =============================================================================
include(FetchContent)

# Google Test
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

# Google Benchmark
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)

# Abseil
FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20240722.0
)

# Prevent GoogleTest from overriding compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Don't build Google Benchmark's tests
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

# Abseil propagates its PIC setting; match it
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_MakeAvailable(googletest googlebenchmark abseil-cpp)

# =============================================================================
# PerfMap Library (header-only)
# =============================================================================
add_library(perfmap INTERFACE)
target_include_directories(perfmap INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(perfmap INTERFACE
  absl::flat_hash_map
  absl::node_hash_map
  absl::status
  absl::statusor
)

# =============================================================================
# Tests
# =============================================================================
enable_testing()

add_executable(perfmap_tests
  tests/hash_map_contract_test.cc
  tests/scratch_hash_map_test.cc
  tests/scratch_indirect_hash_map_test.cc
  tests/hash_map_test.cc
  tests/memory_metrics_test.cc
)
target_link_libraries(perfmap_tests PRIVATE
  perfmap
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(perfmap_tests)

# =============================================================================
# Benchmarks
# =============================================================================
add_executable(perfmap_bench
  bench/hash_map_bench.cc
)
target_link_libraries(perfmap_bench PRIVATE
  perfmap
  benchmark::benchmark
  benchmark::benchmark_main
)

# "Where We Lose" — adversarial benchmarks showing STL's strengths
add_executable(perfmap_tradeoffs
  bench/tradeoffs_bench.cc
)
target_link_libraries(perfmap_tradeoffs PRIVATE
  perfmap
  benchmark::benchmark
  benchmark::benchmark_main
)

add_executable(perfmap_trace_bench
  bench/trace_workload_bench.cc
)
target_link_libraries(perfmap_trace_bench PRIVATE
  perfmap
  benchmark::benchmark
  benchmark::benchmark_main
)

target_compile_definitions(perfmap_bench PRIVATE
  PERFMAP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(perfmap_trace_bench PRIVATE
  PERFMAP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Optimize benchmarks for release by default.
# -march=native lets the compiler use all available instructions on this CPU.
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  target_compile_options(perfmap_bench PRIVATE
    $<$<CONFIG:Release>:-O2 -march=native>
  )
  target_compile_options(perfmap_tradeoffs PRIVATE
    $<$<CONFIG:Release>:-O2 -march=native>
  )
  target_compile_options(perfmap_trace_bench PRIVATE
    $<$<CONFIG:Release>:-O2 -march=native>
  )
else()
  target_compile_options(perfmap_bench PRIVATE
    $<$<CONFIG:Release>:-O2 -mcpu=native>
  )
  target_compile_options(perfmap_tradeoffs PRIVATE
    $<$<CONFIG:Release>:-O2 -mcpu=native>
  )
  target_compile_options(perfmap_trace_bench PRIVATE
    $<$<CONFIG:Release>:-O2 -mcpu=native>
  )
endif()
