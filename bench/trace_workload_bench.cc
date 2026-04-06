#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>
#include <string>

#include "benchmark_support.h"
#include "perfmap/map_adapters.h"
#include "workload_trace.h"

namespace perfmap {
namespace {

using benchmarks::AttachAdapterCounters;
using benchmarks::LargeValue;
using benchmarks::MakeLargeValue;
using benchmarks::ReadValueForChecksum;
using benchmarks::TraceEvent;
using benchmarks::TraceOperation;
using benchmarks::WorkloadTrace;

template <typename Value>
struct TraceValueFactory;

template <>
struct TraceValueFactory<int> {
  static int Make(uint64_t seed) {
    return static_cast<int>(seed & 0x7fffffffU);
  }
};

template <>
struct TraceValueFactory<LargeValue> {
  static LargeValue Make(uint64_t seed) {
    return MakeLargeValue(seed, 0x71A6E00DULL);
  }
};

template <typename Adapter, typename Value>
uint64_t ReplayTraceOnce(Adapter& map, const WorkloadTrace& trace) {
  uint64_t checksum = 0;
  for (const TraceEvent& event : trace.events) {
    switch (event.operation) {
      case TraceOperation::kClear:
        map.Clear();
        break;
      case TraceOperation::kInsert:
        map.InsertOrAssign(event.key, TraceValueFactory<Value>::Make(
                                          event.value_seed));
        break;
      case TraceOperation::kFind: {
        const Value* value = map.FindPtr(event.key);
        benchmark::DoNotOptimize(value);
        checksum += ReadValueForChecksum(value);
        break;
      }
    }
  }
  return checksum;
}

template <typename Adapter, typename Value>
void RunTraceReplay(benchmark::State& state, const WorkloadTrace& trace) {
  uint64_t checksum = 0;

  for (auto _ : state) {
    Adapter map;
    if (trace.peak_live_entries != 0) {
      map.Reserve(trace.peak_live_entries);
    }
    checksum += ReplayTraceOnce<Adapter, Value>(map, trace);
    benchmark::DoNotOptimize(map.Size());
  }

  Adapter metrics_map;
  if (trace.peak_live_entries != 0) {
    metrics_map.Reserve(trace.peak_live_entries);
  }
  benchmark::DoNotOptimize(ReplayTraceOnce<Adapter, Value>(metrics_map, trace));
  AttachAdapterCounters(state, metrics_map);
  state.counters["trace_insert_ops"] = static_cast<double>(trace.insert_ops);
  state.counters["trace_find_ops"] = static_cast<double>(trace.find_ops);
  state.counters["trace_clear_ops"] = static_cast<double>(trace.clear_ops);
  state.counters["trace_peak_live_entries"] =
      static_cast<double>(trace.peak_live_entries);
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * trace.events.size());
}

template <typename Adapter, typename Value>
void RegisterTraceBenchmark(const std::string& trace_name,
                            const std::string& fixture_name) {
  auto trace =
      std::make_shared<WorkloadTrace>(benchmarks::LoadTraceFixture(fixture_name));
  const std::string benchmark_name =
      std::string(Adapter::kBenchmarkName) + "/trace_" + trace_name;
  benchmark::RegisterBenchmark(
      benchmark_name.c_str(),
      [trace](benchmark::State& state) {
        RunTraceReplay<Adapter, Value>(state, *trace);
      });
}

using StdAdapter = adapters::StdUnorderedMapAdapter<int, int>;
using AbslAdapter = adapters::AbslFlatHashMapAdapter<int, int>;
using PerfMapBalancedAdapter = adapters::PerfMapAdapter<int, int>;
using PerfMapReadHeavyAdapter =
    adapters::PerfMapAdapter<int, int, ReadHeavyPolicy>;
using PerfMapSpaceEfficientAdapter =
    adapters::PerfMapAdapter<int, int, SpaceEfficientPolicy>;
using ScratchAdapter = adapters::PerfMapScratchAdapter<int, int>;
using StdLargeValueAdapter = adapters::StdUnorderedMapAdapter<int, LargeValue>;
using AbslLargeValueAdapter =
    adapters::AbslFlatHashMapAdapter<int, LargeValue>;
using AbslNodeLargeValueAdapter =
    adapters::AbslNodeHashMapAdapter<int, LargeValue>;
using PerfMapIndirectLargeValueAdapter =
    adapters::PerfMapIndirectAdapter<int, LargeValue>;
using ScratchIndirectLargeValueAdapter =
    adapters::PerfMapScratchIndirectAdapter<int, LargeValue>;

const bool kRegistered = [] {
  RegisterTraceBenchmark<StdAdapter, int>("request_dedup_enrichment",
                                          "request_dedup_enrichment.csv");
  RegisterTraceBenchmark<AbslAdapter, int>("request_dedup_enrichment",
                                           "request_dedup_enrichment.csv");
  RegisterTraceBenchmark<PerfMapBalancedAdapter, int>(
      "request_dedup_enrichment", "request_dedup_enrichment.csv");
  RegisterTraceBenchmark<ScratchAdapter, int>("request_dedup_enrichment",
                                              "request_dedup_enrichment.csv");

  RegisterTraceBenchmark<StdAdapter, int>("graph_traversal_scratch",
                                          "graph_traversal_scratch.csv");
  RegisterTraceBenchmark<AbslAdapter, int>("graph_traversal_scratch",
                                           "graph_traversal_scratch.csv");
  RegisterTraceBenchmark<ScratchAdapter, int>("graph_traversal_scratch",
                                              "graph_traversal_scratch.csv");

  RegisterTraceBenchmark<StdAdapter, int>("hotset_phase_shift",
                                          "hotset_phase_shift.csv");
  RegisterTraceBenchmark<AbslAdapter, int>("hotset_phase_shift",
                                           "hotset_phase_shift.csv");
  RegisterTraceBenchmark<PerfMapBalancedAdapter, int>(
      "hotset_phase_shift", "hotset_phase_shift.csv");
  RegisterTraceBenchmark<PerfMapReadHeavyAdapter, int>(
      "hotset_phase_shift", "hotset_phase_shift.csv");
  RegisterTraceBenchmark<PerfMapSpaceEfficientAdapter, int>(
      "hotset_phase_shift", "hotset_phase_shift.csv");

  RegisterTraceBenchmark<StdLargeValueAdapter, LargeValue>(
      "batch_metadata_enrichment", "batch_metadata_enrichment.csv");
  RegisterTraceBenchmark<AbslLargeValueAdapter, LargeValue>(
      "batch_metadata_enrichment", "batch_metadata_enrichment.csv");
  RegisterTraceBenchmark<AbslNodeLargeValueAdapter, LargeValue>(
      "batch_metadata_enrichment", "batch_metadata_enrichment.csv");
  RegisterTraceBenchmark<PerfMapIndirectLargeValueAdapter, LargeValue>(
      "batch_metadata_enrichment", "batch_metadata_enrichment.csv");
  RegisterTraceBenchmark<ScratchIndirectLargeValueAdapter, LargeValue>(
      "batch_metadata_enrichment", "batch_metadata_enrichment.csv");
  return true;
}();

}  // namespace
}  // namespace perfmap
