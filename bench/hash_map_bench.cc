// Copyright 2026 SFU GDSC — PerfMap Workshop
//
// Benchmarks comparing PerfMap's HashMap against std::unordered_map.
//
// Run:
//   ./perfmap_bench                          # console output
//   ./perfmap_bench --benchmark_out=results.json --benchmark_out_format=json
//
// Key insight for students:
//   Our open-addressing map stores everything in a flat array.
//   std::unordered_map uses separate chaining (linked-list buckets).
//   On modern CPUs, cache locality from contiguous storage can dominate
//   the algorithmic cost — that's what we're measuring here.

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

#include "perfmap/hash_map.h"

// Suppress [[nodiscard]] warnings in benchmark setup code.
#define PERFMAP_IGNORE_STATUS(expr) (void)(expr)

// =============================================================================
// Helpers
// =============================================================================

// Deterministic sequence of keys so benchmarks are reproducible.
static std::vector<int> GenerateKeys(int count) {
  std::mt19937 rng(42);  // Fixed seed for reproducibility
  std::vector<int> keys(count);
  for (int i = 0; i < count; ++i) {
    keys[i] = static_cast<int>(rng());
  }
  return keys;
}

// =============================================================================
// INSERT benchmarks
// =============================================================================

static void BM_StdUnorderedMap_Insert(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    std::unordered_map<int, int> map;
    map.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
      map[keys[i]] = static_cast<int>(i);
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdUnorderedMap_Insert)->Range(1 << 10, 1 << 18);

static void BM_PerfMap_Insert(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    perfmap::HashMap<int, int> map;
    for (size_t i = 0; i < keys.size(); ++i) {
      PERFMAP_IGNORE_STATUS(map.Insert(keys[i], static_cast<int>(i)));
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PerfMap_Insert)->Range(1 << 10, 1 << 18);

// =============================================================================
// LOOKUP benchmarks  (search for keys that exist)
// =============================================================================

static void BM_StdUnorderedMap_Find(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  std::unordered_map<int, int> map;
  map.reserve(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    map[keys[i]] = static_cast<int>(i);
  }
  // Lookup every key once per iteration
  for (auto _ : state) {
    for (const auto& key : keys) {
      benchmark::DoNotOptimize(map.find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdUnorderedMap_Find)->Range(1 << 10, 1 << 18);

static void BM_PerfMap_Find(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  perfmap::HashMap<int, int> map;
  for (size_t i = 0; i < keys.size(); ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(keys[i], static_cast<int>(i)));
  }
  for (auto _ : state) {
    for (const auto& key : keys) {
      benchmark::DoNotOptimize(map.Find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PerfMap_Find)->Range(1 << 10, 1 << 18);

// =============================================================================
// LOOKUP benchmarks  (search for keys that do NOT exist — miss path)
// =============================================================================

static void BM_StdUnorderedMap_FindMiss(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  std::unordered_map<int, int> map;
  map.reserve(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    map[keys[i]] = static_cast<int>(i);
  }
  // Lookup keys that are very unlikely to exist
  std::vector<int> miss_keys(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    miss_keys[i] = -static_cast<int>(i) - 1;
  }
  for (auto _ : state) {
    for (const auto& key : miss_keys) {
      benchmark::DoNotOptimize(map.find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdUnorderedMap_FindMiss)->Range(1 << 10, 1 << 18);

static void BM_PerfMap_FindMiss(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  perfmap::HashMap<int, int> map;
  for (size_t i = 0; i < keys.size(); ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(keys[i], static_cast<int>(i)));
  }
  std::vector<int> miss_keys(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    miss_keys[i] = -static_cast<int>(i) - 1;
  }
  for (auto _ : state) {
    for (const auto& key : miss_keys) {
      benchmark::DoNotOptimize(map.Find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PerfMap_FindMiss)->Range(1 << 10, 1 << 18);

// =============================================================================
// ERASE benchmarks
// =============================================================================

static void BM_StdUnorderedMap_Erase(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    state.PauseTiming();
    std::unordered_map<int, int> map;
    map.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
      map[keys[i]] = static_cast<int>(i);
    }
    state.ResumeTiming();
    for (const auto& key : keys) {
      map.erase(key);
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdUnorderedMap_Erase)->Range(1 << 10, 1 << 16);

static void BM_PerfMap_Erase(benchmark::State& state) {
  auto keys = GenerateKeys(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    state.PauseTiming();
    perfmap::HashMap<int, int> map;
    for (size_t i = 0; i < keys.size(); ++i) {
      PERFMAP_IGNORE_STATUS(map.Insert(keys[i], static_cast<int>(i)));
    }
    state.ResumeTiming();
    for (const auto& key : keys) {
      PERFMAP_IGNORE_STATUS(map.Erase(key));
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PerfMap_Erase)->Range(1 << 10, 1 << 16);

// =============================================================================
// MIXED WORKLOAD — realistic: 50% insert, 40% lookup, 10% erase
// =============================================================================

static void BM_StdUnorderedMap_Mixed(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n);

  for (auto _ : state) {
    std::unordered_map<int, int> map;
    map.reserve(n);

    // Insert half
    for (int i = 0; i < n / 2; ++i) {
      map[keys[i]] = i;
    }
    // Lookup 40%
    for (int i = 0; i < n * 2 / 5; ++i) {
      benchmark::DoNotOptimize(map.find(keys[i % (n / 2)]));
    }
    // Erase 10%
    for (int i = 0; i < n / 10; ++i) {
      map.erase(keys[i]);
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StdUnorderedMap_Mixed)->Range(1 << 10, 1 << 17);

static void BM_PerfMap_Mixed(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n);

  for (auto _ : state) {
    perfmap::HashMap<int, int> map;

    // Insert half
    for (int i = 0; i < n / 2; ++i) {
      PERFMAP_IGNORE_STATUS(map.Insert(keys[i], i));
    }
    // Lookup 40%
    for (int i = 0; i < n * 2 / 5; ++i) {
      benchmark::DoNotOptimize(map.Find(keys[i % (n / 2)]));
    }
    // Erase 10%
    for (int i = 0; i < n / 10; ++i) {
      PERFMAP_IGNORE_STATUS(map.Erase(keys[i]));
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_PerfMap_Mixed)->Range(1 << 10, 1 << 17);
