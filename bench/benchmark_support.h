#ifndef PERFMAP_BENCHMARK_SUPPORT_H_
#define PERFMAP_BENCHMARK_SUPPORT_H_

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "perfmap/memory_metrics.h"

namespace perfmap::benchmarks {

struct LargeValue {
  std::array<uint64_t, 128> words{};
};

inline LargeValue MakeLargeValue(uint64_t seed, uint64_t salt) {
  auto Next = [](uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
  };

  LargeValue value;
  uint64_t state = seed ^ salt;
  for (uint64_t& word : value.words) {
    state = Next(state);
    word = state;
  }
  return value;
}

template <typename Adapter>
void FillIntMap(Adapter& map, const std::vector<int>& keys) {
  map.Reserve(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    map.InsertOrAssign(keys[i], static_cast<int>(i));
  }
}

inline void AttachMemoryCounters(benchmark::State& state,
                                 const perfmap::MemoryMetrics& metrics) {
  state.counters["live_entries"] = static_cast<double>(metrics.live_entries);
  state.counters["reserved_capacity"] =
      static_cast<double>(metrics.reserved_capacity);
  state.counters["tombstones"] =
      static_cast<double>(metrics.tombstone_count);
  state.counters["hot_table_bytes"] =
      static_cast<double>(metrics.hot_table_bytes);
  state.counters["payload_storage_bytes"] =
      static_cast<double>(metrics.payload_storage_bytes);
  state.counters["total_reserved_bytes"] =
      static_cast<double>(perfmap::TotalReservedBytes(metrics));
  state.counters["live_payload_bytes"] =
      static_cast<double>(metrics.live_payload_bytes);
  state.counters["bytes_per_live_entry"] =
      perfmap::BytesPerLiveEntry(metrics);
  state.counters["effective_load_factor"] = metrics.effective_load_factor;
  state.counters["memory_bytes_exact"] =
      metrics.bytes_precision == MetricPrecision::kExact ? 1.0 : 0.0;
  state.counters["memory_capacity_exact"] =
      metrics.capacity_precision == MetricPrecision::kExact ? 1.0 : 0.0;
}

template <typename Adapter>
void AttachAdapterCounters(benchmark::State& state, const Adapter& map) {
  AttachMemoryCounters(state, map.Metrics());
}

inline uint64_t ReadValueForChecksum(const int* value) {
  return value == nullptr ? 0u : static_cast<uint64_t>(*value);
}

inline uint64_t ReadValueForChecksum(const LargeValue* value) {
  return value == nullptr ? 0u : value->words[0] ^ value->words[127];
}

}  // namespace perfmap::benchmarks

#endif  // PERFMAP_BENCHMARK_SUPPORT_H_
