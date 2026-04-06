#ifndef PERFMAP_MAP_ADAPTERS_H_
#define PERFMAP_MAP_ADAPTERS_H_

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <string_view>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "perfmap/hash_map.h"
#include "perfmap/indirect_hash_map.h"
#include "perfmap/scratch_hash_map.h"
#include "perfmap/scratch_indirect_hash_map.h"

namespace perfmap::adapters {

namespace detail {

template <typename K, typename V>
using MapValueType = std::pair<const K, V>;

template <typename K, typename V>
constexpr size_t ApproxNodeOverheadBytes() {
  return sizeof(void*) * 2;
}

template <typename Map>
size_t BucketBackedReservedCapacity(const Map& map) {
  return static_cast<size_t>(std::floor(
      static_cast<double>(map.bucket_count()) * map.max_load_factor()));
}

template <typename K, typename V>
MemoryMetrics MakeStdNodeLikeMetrics(size_t live_entries, size_t bucket_count,
                                     size_t reserved_capacity) {
  const size_t bucket_bytes = bucket_count * sizeof(void*);
  const size_t node_bytes =
      live_entries *
      (sizeof(MapValueType<K, V>) + ApproxNodeOverheadBytes<K, V>());
  MemoryMetrics metrics;
  metrics.live_entries = live_entries;
  metrics.reserved_capacity = reserved_capacity;
  metrics.tombstone_count = 0;
  metrics.hot_table_bytes = bucket_bytes + node_bytes;
  metrics.payload_storage_bytes = 0;
  metrics.live_payload_bytes = live_entries * sizeof(V);
  metrics.effective_load_factor =
      reserved_capacity == 0
          ? 1.0
          : static_cast<double>(live_entries) /
                static_cast<double>(reserved_capacity);
  metrics.capacity_precision = MetricPrecision::kEstimated;
  metrics.bytes_precision = MetricPrecision::kEstimated;
  return metrics;
}

template <typename K, typename V>
MemoryMetrics MakeFlatHashMetrics(size_t live_entries, size_t bucket_count,
                                  size_t reserved_capacity) {
  const size_t slot_bytes = bucket_count * sizeof(MapValueType<K, V>);
  const size_t control_bytes = bucket_count + 16;
  MemoryMetrics metrics;
  metrics.live_entries = live_entries;
  metrics.reserved_capacity = reserved_capacity;
  metrics.tombstone_count = 0;
  metrics.hot_table_bytes = slot_bytes + control_bytes;
  metrics.payload_storage_bytes = 0;
  metrics.live_payload_bytes = live_entries * sizeof(V);
  metrics.effective_load_factor =
      reserved_capacity == 0
          ? 1.0
          : static_cast<double>(live_entries) /
                static_cast<double>(reserved_capacity);
  metrics.capacity_precision = MetricPrecision::kEstimated;
  metrics.bytes_precision = MetricPrecision::kEstimated;
  return metrics;
}

}  // namespace detail

template <typename K, typename V>
class StdUnorderedMapAdapter {
 public:
  using key_type = K;
  using mapped_type = V;

  static constexpr std::string_view kBenchmarkName = "std_unordered_map";
  static constexpr std::string_view kTestName = "StdUnorderedMap";
  static constexpr std::string_view kDisplayName = "std::unordered_map";

  void Reserve(size_t expected_size) { map_.reserve(expected_size); }
  void Clear() { map_.clear(); }

  void InsertOrAssign(const K& key, const V& value) {
    map_.insert_or_assign(key, value);
  }

  const V* FindPtr(const K& key) const {
    const auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
  }

  bool Contains(const K& key) const { return map_.find(key) != map_.end(); }

  bool Erase(const K& key) { return map_.erase(key) != 0; }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }

  MemoryMetrics Metrics() const {
    return detail::MakeStdNodeLikeMetrics<K, V>(
        map_.size(), map_.bucket_count(),
        detail::BucketBackedReservedCapacity(map_));
  }

 private:
  std::unordered_map<K, V> map_;
};

template <typename K, typename V>
class AbslFlatHashMapAdapter {
 public:
  using key_type = K;
  using mapped_type = V;

  static constexpr std::string_view kBenchmarkName = "absl_flat_hash_map";
  static constexpr std::string_view kTestName = "AbslFlatHashMap";
  static constexpr std::string_view kDisplayName = "absl::flat_hash_map";

  void Reserve(size_t expected_size) { map_.reserve(expected_size); }
  void Clear() { map_.clear(); }

  void InsertOrAssign(const K& key, const V& value) {
    map_.insert_or_assign(key, value);
  }

  const V* FindPtr(const K& key) const {
    const auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
  }

  bool Contains(const K& key) const { return map_.find(key) != map_.end(); }

  bool Erase(const K& key) { return map_.erase(key) != 0; }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }

  MemoryMetrics Metrics() const {
    return detail::MakeFlatHashMetrics<K, V>(map_.size(), map_.bucket_count(),
                                             map_.capacity());
  }

 private:
  absl::flat_hash_map<K, V> map_;
};

template <typename K, typename V>
class AbslNodeHashMapAdapter {
 public:
  using key_type = K;
  using mapped_type = V;

  static constexpr std::string_view kBenchmarkName = "absl_node_hash_map";
  static constexpr std::string_view kTestName = "AbslNodeHashMap";
  static constexpr std::string_view kDisplayName = "absl::node_hash_map";

  void Reserve(size_t expected_size) { map_.reserve(expected_size); }
  void Clear() { map_.clear(); }

  void InsertOrAssign(const K& key, const V& value) {
    map_.insert_or_assign(key, value);
  }

  const V* FindPtr(const K& key) const {
    const auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
  }

  bool Contains(const K& key) const { return map_.find(key) != map_.end(); }

  bool Erase(const K& key) { return map_.erase(key) != 0; }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }

  MemoryMetrics Metrics() const {
    return detail::MakeStdNodeLikeMetrics<K, V>(
        map_.size(), map_.bucket_count(),
        detail::BucketBackedReservedCapacity(map_));
  }

 private:
  absl::node_hash_map<K, V> map_;
};

template <typename K, typename V, typename Policy = BalancedWorkloadPolicy>
class PerfMapAdapter {
 public:
  using key_type = K;
  using mapped_type = V;
  using map_type = perfmap::HashMap<K, V, std::hash<K>, Policy>;

  static constexpr std::string_view kBenchmarkName = Policy::kBenchmarkName;
  static constexpr std::string_view kTestName = Policy::kTestName;
  static constexpr std::string_view kDisplayName = Policy::kDisplayName;

  void Reserve(size_t expected_size) { map_.Reserve(expected_size); }
  void Clear() { map_ = map_type(); }

  void InsertOrAssign(const K& key, const V& value) {
    (void)map_.Insert(key, value);
  }

  const V* FindPtr(const K& key) const { return map_.FindPtr(key); }

  bool Contains(const K& key) const { return map_.Contains(key); }

  bool Erase(const K& key) { return map_.Erase(key).ok(); }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }
  MemoryMetrics Metrics() const { return map_.memory_metrics(); }

 private:
  map_type map_;
};

template <typename K, typename V>
class PerfMapIndirectAdapter {
 public:
  using key_type = K;
  using mapped_type = V;
  using map_type = perfmap::IndirectHashMap<K, V>;

  static constexpr std::string_view kBenchmarkName = map_type::kBenchmarkName;
  static constexpr std::string_view kTestName = map_type::kTestName;
  static constexpr std::string_view kDisplayName = map_type::kDisplayName;

  void Reserve(size_t expected_size) { map_.Reserve(expected_size); }
  void Clear() { map_ = map_type(); }

  void InsertOrAssign(const K& key, const V& value) {
    (void)map_.Insert(key, value);
  }

  const V* FindPtr(const K& key) const { return map_.FindPtr(key); }

  bool Contains(const K& key) const { return map_.Contains(key); }

  bool Erase(const K& key) { return map_.Erase(key).ok(); }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }
  MemoryMetrics Metrics() const { return map_.memory_metrics(); }

 private:
  map_type map_;
};

template <typename K, typename V>
class PerfMapScratchAdapter {
 public:
  using key_type = K;
  using mapped_type = V;
  using map_type = perfmap::ScratchHashMap<K, V>;

  static constexpr std::string_view kBenchmarkName = map_type::kBenchmarkName;
  static constexpr std::string_view kTestName = map_type::kTestName;
  static constexpr std::string_view kDisplayName = map_type::kDisplayName;

  void Reserve(size_t expected_size) { map_.Reserve(expected_size); }
  void Clear() { map_.Clear(); }

  void InsertOrAssign(const K& key, const V& value) {
    (void)map_.Insert(key, value);
  }

  const V* FindPtr(const K& key) const { return map_.FindPtr(key); }

  bool Contains(const K& key) const { return map_.Contains(key); }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }
  MemoryMetrics Metrics() const { return map_.memory_metrics(); }

 private:
  map_type map_;
};

template <typename K, typename V>
class PerfMapScratchIndirectAdapter {
 public:
  using key_type = K;
  using mapped_type = V;
  using map_type = perfmap::ScratchIndirectHashMap<K, V>;

  static constexpr std::string_view kBenchmarkName = map_type::kBenchmarkName;
  static constexpr std::string_view kTestName = map_type::kTestName;
  static constexpr std::string_view kDisplayName = map_type::kDisplayName;

  void Reserve(size_t expected_size) { map_.Reserve(expected_size); }
  void Clear() { map_.Clear(); }

  void InsertOrAssign(const K& key, const V& value) {
    (void)map_.Insert(key, value);
  }

  const V* FindPtr(const K& key) const { return map_.FindPtr(key); }

  bool Contains(const K& key) const { return map_.Contains(key); }

  size_t Size() const { return map_.size(); }
  bool Empty() const { return map_.empty(); }
  MemoryMetrics Metrics() const { return map_.memory_metrics(); }

 private:
  map_type map_;
};

}  // namespace perfmap::adapters

#endif  // PERFMAP_MAP_ADAPTERS_H_
