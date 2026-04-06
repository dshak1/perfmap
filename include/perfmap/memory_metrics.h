#ifndef PERFMAP_MEMORY_METRICS_H_
#define PERFMAP_MEMORY_METRICS_H_

#include <cstddef>

namespace perfmap {

enum class MetricPrecision {
  kExact,
  kEstimated,
};

struct MemoryMetrics {
  size_t live_entries = 0;
  size_t reserved_capacity = 0;
  size_t tombstone_count = 0;
  size_t hot_table_bytes = 0;
  size_t payload_storage_bytes = 0;
  size_t live_payload_bytes = 0;
  double effective_load_factor = 0.0;
  MetricPrecision capacity_precision = MetricPrecision::kExact;
  MetricPrecision bytes_precision = MetricPrecision::kExact;
};

inline size_t TotalReservedBytes(const MemoryMetrics& metrics) {
  return metrics.hot_table_bytes + metrics.payload_storage_bytes;
}

inline double BytesPerLiveEntry(const MemoryMetrics& metrics) {
  if (metrics.live_entries == 0) return 0.0;
  return static_cast<double>(TotalReservedBytes(metrics)) /
         static_cast<double>(metrics.live_entries);
}

}  // namespace perfmap

#endif  // PERFMAP_MEMORY_METRICS_H_
