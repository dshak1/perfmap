// Copyright 2026 SFU GDSC — PerfMap Workshop
//
// Implementation-agnostic hash map benchmarks.
//
// Fairness rules:
//   1. Every implementation sees the exact same deterministic key sets.
//   2. Insert benchmarks use unique keys only (no accidental upserts).
//   3. Lookup benchmarks use a pointer-returning fast path for every map.
//   4. Setup is excluded from steady-state lookup / erase timing.
//   5. "reserved" benchmarks pre-size every map through the same adapter API.

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "benchmark_support.h"
#include "perfmap/map_adapters.h"

namespace perfmap {
namespace {

using benchmarks::AttachAdapterCounters;
using benchmarks::FillIntMap;
using benchmarks::LargeValue;
using benchmarks::MakeLargeValue;
using benchmarks::ReadValueForChecksum;

std::vector<int> GenerateUniqueKeys(size_t count, uint32_t seed, int base) {
  std::vector<int> keys(count);
  std::iota(keys.begin(), keys.end(), base);
  std::mt19937 rng(seed);
  std::shuffle(keys.begin(), keys.end(), rng);
  return keys;
}

struct WorkloadData {
  std::vector<int> insert_keys;
  std::vector<int> hit_keys;
  std::vector<int> miss_keys;
  std::vector<int> mixed_lookup_keys;
  std::vector<int> mixed_erase_keys;
  std::vector<int> churn_replacement_keys;
};

WorkloadData MakeWorkloadData(size_t size) {
  WorkloadData data;
  data.insert_keys = GenerateUniqueKeys(size, /*seed=*/42, /*base=*/0);
  data.hit_keys = data.insert_keys;
  std::mt19937 hit_rng(77);
  std::shuffle(data.hit_keys.begin(), data.hit_keys.end(), hit_rng);

  data.miss_keys = GenerateUniqueKeys(size, /*seed=*/999,
                                      /*base=*/static_cast<int>(size * 4));

  const size_t initial_count = size / 2;
  data.mixed_lookup_keys.assign(data.hit_keys.begin(),
                                data.hit_keys.begin() + initial_count);
  data.mixed_erase_keys.assign(data.insert_keys.begin(),
                               data.insert_keys.begin() + size / 10);
  data.churn_replacement_keys =
      GenerateUniqueKeys(size * 3, /*seed=*/2024,
                         /*base=*/static_cast<int>(size * 8));
  return data;
}

template <typename Adapter>
void RunInsertGrow(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  for (auto _ : state) {
    Adapter map;
    for (size_t i = 0; i < data.insert_keys.size(); ++i) {
      map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
    }
    benchmark::DoNotOptimize(map.Size());
    benchmark::ClobberMemory();
  }

  Adapter metrics_map;
  for (size_t i = 0; i < data.insert_keys.size(); ++i) {
    metrics_map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
  }
  AttachAdapterCounters(state, metrics_map);
  state.SetItemsProcessed(state.iterations() * data.insert_keys.size());
}

template <typename Adapter>
void RunInsertReserved(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  for (auto _ : state) {
    state.PauseTiming();
    Adapter map;
    map.Reserve(data.insert_keys.size());
    state.ResumeTiming();

    for (size_t i = 0; i < data.insert_keys.size(); ++i) {
      map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
    }
    benchmark::DoNotOptimize(map.Size());
    benchmark::ClobberMemory();
  }

  Adapter metrics_map;
  metrics_map.Reserve(data.insert_keys.size());
  for (size_t i = 0; i < data.insert_keys.size(); ++i) {
    metrics_map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
  }
  AttachAdapterCounters(state, metrics_map);
  state.SetItemsProcessed(state.iterations() * data.insert_keys.size());
}

template <typename Adapter>
void RunFindHit(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  Adapter map;
  FillIntMap(map, data.insert_keys);

  uint64_t value_sum = 0;
  for (auto _ : state) {
    for (const int key : data.hit_keys) {
      const int* value = map.FindPtr(key);
      benchmark::DoNotOptimize(value);
      value_sum += ReadValueForChecksum(value);
    }
  }

  benchmark::DoNotOptimize(value_sum);
  AttachAdapterCounters(state, map);
  state.SetItemsProcessed(state.iterations() * data.hit_keys.size());
}

template <typename Adapter>
void RunFindMiss(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  Adapter map;
  FillIntMap(map, data.insert_keys);

  uint64_t miss_count = 0;
  for (auto _ : state) {
    for (const int key : data.miss_keys) {
      const int* value = map.FindPtr(key);
      benchmark::DoNotOptimize(value);
      miss_count += (value == nullptr) ? 1u : 0u;
    }
  }

  benchmark::DoNotOptimize(miss_count);
  AttachAdapterCounters(state, map);
  state.SetItemsProcessed(state.iterations() * data.miss_keys.size());
}

template <typename Adapter>
void RunErase(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  for (auto _ : state) {
    state.PauseTiming();
    Adapter map;
    FillIntMap(map, data.insert_keys);
    state.ResumeTiming();

    size_t erased = 0;
    for (const int key : data.insert_keys) {
      erased += map.Erase(key) ? 1u : 0u;
    }
    benchmark::DoNotOptimize(erased);
    benchmark::DoNotOptimize(map.Size());
  }

  Adapter metrics_map;
  FillIntMap(metrics_map, data.insert_keys);
  for (const int key : data.insert_keys) {
    benchmark::DoNotOptimize(metrics_map.Erase(key));
  }
  AttachAdapterCounters(state, metrics_map);
  state.SetItemsProcessed(state.iterations() * data.insert_keys.size());
}

template <typename Adapter>
void RunMixed(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  const auto data = MakeWorkloadData(size);
  const size_t initial_count = size / 2;
  const size_t lookup_count = size * 2 / 5;
  const size_t erase_count = size / 10;

  for (auto _ : state) {
    Adapter map;
    map.Reserve(size);

    for (size_t i = 0; i < initial_count; ++i) {
      map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
    }

    uint64_t value_sum = 0;
    for (size_t i = 0; i < lookup_count; ++i) {
      const int* value =
          map.FindPtr(data.mixed_lookup_keys[i % data.mixed_lookup_keys.size()]);
      benchmark::DoNotOptimize(value);
      value_sum += ReadValueForChecksum(value);
    }

    size_t erased = 0;
    for (size_t i = 0; i < erase_count; ++i) {
      erased += map.Erase(data.mixed_erase_keys[i]) ? 1u : 0u;
    }

    benchmark::DoNotOptimize(value_sum);
    benchmark::DoNotOptimize(erased);
    benchmark::DoNotOptimize(map.Size());
  }

  Adapter metrics_map;
  metrics_map.Reserve(size);
  for (size_t i = 0; i < initial_count; ++i) {
    metrics_map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
  }
  for (size_t i = 0; i < lookup_count; ++i) {
    benchmark::DoNotOptimize(
        metrics_map.FindPtr(data.mixed_lookup_keys[i % data.mixed_lookup_keys.size()]));
  }
  for (size_t i = 0; i < erase_count; ++i) {
    benchmark::DoNotOptimize(metrics_map.Erase(data.mixed_erase_keys[i]));
  }
  AttachAdapterCounters(state, metrics_map);
  state.SetItemsProcessed(
      state.iterations() * static_cast<int64_t>(initial_count + lookup_count +
                                                erase_count));
}

template <typename Adapter>
void RunChurn(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  const auto data = MakeWorkloadData(size);

  for (auto _ : state) {
    Adapter map;
    map.Reserve(size);
    std::vector<int> live_keys = data.insert_keys;

    for (size_t i = 0; i < size; ++i) {
      map.InsertOrAssign(live_keys[i], static_cast<int>(i));
    }

    uint64_t value_sum = 0;
    for (size_t round = 0; round < 3; ++round) {
      for (size_t i = 0; i < size; ++i) {
        const int old_key = live_keys[i];
        const int new_key = data.churn_replacement_keys[round * size + i];

        benchmark::DoNotOptimize(map.Erase(old_key));
        map.InsertOrAssign(new_key, static_cast<int>(round * size + i));

        const int* value = map.FindPtr(new_key);
        benchmark::DoNotOptimize(value);
        value_sum += ReadValueForChecksum(value);
        live_keys[i] = new_key;
      }
    }

    benchmark::DoNotOptimize(value_sum);
    benchmark::DoNotOptimize(map.Size());
  }

  Adapter metrics_map;
  metrics_map.Reserve(size);
  std::vector<int> live_keys = data.insert_keys;
  for (size_t i = 0; i < size; ++i) {
    metrics_map.InsertOrAssign(live_keys[i], static_cast<int>(i));
  }
  for (size_t round = 0; round < 3; ++round) {
    for (size_t i = 0; i < size; ++i) {
      const int old_key = live_keys[i];
      const int new_key = data.churn_replacement_keys[round * size + i];
      benchmark::DoNotOptimize(metrics_map.Erase(old_key));
      metrics_map.InsertOrAssign(new_key, static_cast<int>(round * size + i));
      benchmark::DoNotOptimize(metrics_map.FindPtr(new_key));
      live_keys[i] = new_key;
    }
  }
  AttachAdapterCounters(state, metrics_map);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(size * 3 * 3));
}

template <typename Adapter>
void RegisterBenchmarksForAdapter() {
  const std::string prefix(Adapter::kBenchmarkName);

  benchmark::RegisterBenchmark((prefix + "/insert_grow").c_str(),
                               &RunInsertGrow<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 18);

  benchmark::RegisterBenchmark((prefix + "/insert_reserved").c_str(),
                               &RunInsertReserved<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 18);

  benchmark::RegisterBenchmark((prefix + "/find_hit").c_str(),
                               &RunFindHit<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 18);

  benchmark::RegisterBenchmark((prefix + "/find_miss").c_str(),
                               &RunFindMiss<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 18);

  benchmark::RegisterBenchmark((prefix + "/erase").c_str(), &RunErase<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 16);

  benchmark::RegisterBenchmark((prefix + "/mixed").c_str(), &RunMixed<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 17);

  benchmark::RegisterBenchmark((prefix + "/churn").c_str(), &RunChurn<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 16);
}

template <typename Adapter>
void RunLargeValueFindHit(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  Adapter map;
  map.Reserve(data.insert_keys.size());

  for (size_t i = 0; i < data.insert_keys.size(); ++i) {
    map.InsertOrAssign(data.insert_keys[i],
                       MakeLargeValue(static_cast<uint64_t>(i), 0xA11CEu));
  }

  uint64_t checksum = 0;
  for (auto _ : state) {
    for (const int key : data.hit_keys) {
      const LargeValue* result = map.FindPtr(key);
      benchmark::DoNotOptimize(result);
      checksum += ReadValueForChecksum(result);
    }
  }

  benchmark::DoNotOptimize(checksum);
  AttachAdapterCounters(state, map);
  state.SetItemsProcessed(state.iterations() * data.hit_keys.size());
}

template <typename Adapter>
void RunLargeValueFindMiss(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  Adapter map;
  map.Reserve(data.insert_keys.size());

  for (size_t i = 0; i < data.insert_keys.size(); ++i) {
    map.InsertOrAssign(data.insert_keys[i],
                       MakeLargeValue(static_cast<uint64_t>(i), 0xBEEFu));
  }

  uint64_t misses = 0;
  for (auto _ : state) {
    for (const int key : data.miss_keys) {
      const LargeValue* result = map.FindPtr(key);
      benchmark::DoNotOptimize(result);
      misses += (result == nullptr) ? 1u : 0u;
    }
  }

  benchmark::DoNotOptimize(misses);
  AttachAdapterCounters(state, map);
  state.SetItemsProcessed(state.iterations() * data.miss_keys.size());
}

template <typename Adapter>
void RegisterLargeValueBenchmarksForAdapter() {
  const std::string prefix(Adapter::kBenchmarkName);

  benchmark::RegisterBenchmark((prefix + "/large_value_find_hit").c_str(),
                               &RunLargeValueFindHit<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 16);

  benchmark::RegisterBenchmark((prefix + "/large_value_find_miss").c_str(),
                               &RunLargeValueFindMiss<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 16);
}

template <typename Adapter>
void RunScratchCycle(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  Adapter map;
  map.Reserve(data.insert_keys.size());

  uint64_t checksum = 0;
  for (auto _ : state) {
    map.Clear();

    for (size_t i = 0; i < data.insert_keys.size(); ++i) {
      map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
    }

    for (const int key : data.hit_keys) {
      const int* value = map.FindPtr(key);
      benchmark::DoNotOptimize(value);
      checksum += ReadValueForChecksum(value);
    }
  }

  benchmark::DoNotOptimize(checksum);
  map.Clear();
  for (size_t i = 0; i < data.insert_keys.size(); ++i) {
    map.InsertOrAssign(data.insert_keys[i], static_cast<int>(i));
  }
  AttachAdapterCounters(state, map);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(data.insert_keys.size() * 2));
}

template <typename Adapter>
void RegisterScratchBenchmarksForAdapter() {
  const std::string prefix(Adapter::kBenchmarkName);
  benchmark::RegisterBenchmark((prefix + "/scratch_cycle").c_str(),
                               &RunScratchCycle<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 17);
}

template <typename Adapter>
void RunLargeValueScratchCycle(benchmark::State& state) {
  const auto data = MakeWorkloadData(static_cast<size_t>(state.range(0)));
  Adapter map;
  map.Reserve(data.insert_keys.size());

  uint64_t checksum = 0;
  for (auto _ : state) {
    map.Clear();
    for (size_t i = 0; i < data.insert_keys.size(); ++i) {
      map.InsertOrAssign(data.insert_keys[i],
                         MakeLargeValue(static_cast<uint64_t>(i), 0x5CA7C4u));
    }

    for (const int key : data.hit_keys) {
      const LargeValue* result = map.FindPtr(key);
      benchmark::DoNotOptimize(result);
      checksum += ReadValueForChecksum(result);
    }
  }

  benchmark::DoNotOptimize(checksum);
  map.Clear();
  for (size_t i = 0; i < data.insert_keys.size(); ++i) {
    map.InsertOrAssign(data.insert_keys[i],
                       MakeLargeValue(static_cast<uint64_t>(i), 0x5CA7C4u));
  }
  AttachAdapterCounters(state, map);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(data.insert_keys.size() * 2));
}

template <typename Adapter>
void RegisterLargeValueScratchBenchmarksForAdapter() {
  const std::string prefix(Adapter::kBenchmarkName);
  benchmark::RegisterBenchmark((prefix + "/large_value_scratch_cycle").c_str(),
                               &RunLargeValueScratchCycle<Adapter>)
      ->RangeMultiplier(2)
      ->Range(1 << 10, 1 << 16);
}

using StdAdapter = adapters::StdUnorderedMapAdapter<int, int>;
using AbslAdapter = adapters::AbslFlatHashMapAdapter<int, int>;
using PerfMapBalancedAdapter = adapters::PerfMapAdapter<int, int>;
using PerfMapReadHeavyAdapter =
    adapters::PerfMapAdapter<int, int, ReadHeavyPolicy>;
using PerfMapChurnHeavyAdapter =
    adapters::PerfMapAdapter<int, int, ChurnHeavyPolicy>;
using PerfMapSpaceEfficientAdapter =
    adapters::PerfMapAdapter<int, int, SpaceEfficientPolicy>;
using StdLargeValueAdapter = adapters::StdUnorderedMapAdapter<int, LargeValue>;
using AbslLargeValueAdapter =
    adapters::AbslFlatHashMapAdapter<int, LargeValue>;
using AbslNodeLargeValueAdapter =
    adapters::AbslNodeHashMapAdapter<int, LargeValue>;
using PerfMapLargeValueAdapter =
    adapters::PerfMapAdapter<int, LargeValue>;
using PerfMapIndirectLargeValueAdapter =
    adapters::PerfMapIndirectAdapter<int, LargeValue>;
using ScratchAdapter = adapters::PerfMapScratchAdapter<int, int>;
using ScratchIndirectLargeValueAdapter =
    adapters::PerfMapScratchIndirectAdapter<int, LargeValue>;

const bool kRegistered = [] {
  RegisterBenchmarksForAdapter<StdAdapter>();
  RegisterBenchmarksForAdapter<AbslAdapter>();
  RegisterBenchmarksForAdapter<PerfMapBalancedAdapter>();
  RegisterBenchmarksForAdapter<PerfMapReadHeavyAdapter>();
  RegisterBenchmarksForAdapter<PerfMapChurnHeavyAdapter>();
  RegisterBenchmarksForAdapter<PerfMapSpaceEfficientAdapter>();
  RegisterLargeValueBenchmarksForAdapter<StdLargeValueAdapter>();
  RegisterLargeValueBenchmarksForAdapter<AbslLargeValueAdapter>();
  RegisterLargeValueBenchmarksForAdapter<AbslNodeLargeValueAdapter>();
  RegisterLargeValueBenchmarksForAdapter<PerfMapLargeValueAdapter>();
  RegisterLargeValueBenchmarksForAdapter<PerfMapIndirectLargeValueAdapter>();
  RegisterScratchBenchmarksForAdapter<StdAdapter>();
  RegisterScratchBenchmarksForAdapter<AbslAdapter>();
  RegisterScratchBenchmarksForAdapter<ScratchAdapter>();
  RegisterLargeValueScratchBenchmarksForAdapter<StdLargeValueAdapter>();
  RegisterLargeValueScratchBenchmarksForAdapter<AbslLargeValueAdapter>();
  RegisterLargeValueScratchBenchmarksForAdapter<AbslNodeLargeValueAdapter>();
  RegisterLargeValueScratchBenchmarksForAdapter<
      ScratchIndirectLargeValueAdapter>();
  return true;
}();

}  // namespace
}  // namespace perfmap
