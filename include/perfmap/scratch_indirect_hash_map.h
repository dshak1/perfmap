#ifndef PERFMAP_SCRATCH_INDIRECT_HASH_MAP_H_
#define PERFMAP_SCRATCH_INDIRECT_HASH_MAP_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "perfmap/hash_map.h"
#include "perfmap/memory_metrics.h"

namespace perfmap {

// Scratch map specialized for large values and repeated rebuild cycles.
//
// Niche:
// - request-scoped parsed-object caches
// - per-batch metadata maps
// - temporary large-record lookup tables in pipelines
//
// Structural idea:
// - generation-stamped slots give O(1) Clear()
// - slots store only key + payload index, not the payload itself
// - payload storage is reused linearly across generations
template <typename K, typename V, typename Hash = std::hash<K>>
class ScratchIndirectHashMap {
 public:
  static constexpr std::string_view kBenchmarkName =
      "perfmap_scratch_indirect_large_value";
  static constexpr std::string_view kTestName =
      "PerfMapScratchIndirectLargeValue";
  static constexpr std::string_view kDisplayName =
      "perfmap::ScratchIndirectHashMap";

  explicit ScratchIndirectHashMap(size_t initial_capacity = 16)
      : table_(RoundUpPow2(initial_capacity)),
        mask_(table_.size() - 1),
        size_(0),
        generation_(1),
        next_value_index_(0) {}

  void Reserve(size_t expected_size) {
    if (expected_size == 0) return;
    const size_t min_capacity = RoundUpPow2(expected_size * 2);
    if (min_capacity > table_.size()) {
      Rehash(min_capacity);
    }
    if (values_.size() < expected_size) {
      values_.resize(expected_size);
    }
  }

  void Clear() {
    ++generation_;
    size_ = 0;
    next_value_index_ = 0;
    if (generation_ == 0) {
      for (auto& slot : table_) {
        slot.generation = 0;
      }
      generation_ = 1;
    }
  }

  absl::Status Insert(const K& key, const V& value) {
    if ((size_ + 1) * 2 > table_.size()) {
      Rehash(table_.size() * 2);
    }

    const size_t idx = FindSlotIndex(key);
    Slot& slot = table_[idx];
    if (slot.generation == generation_) {
      values_[slot.value_index] = value;
      return absl::OkStatus();
    }

    if (next_value_index_ == values_.size()) {
      values_.emplace_back();
    }
    values_[next_value_index_] = value;
    slot.generation = generation_;
    slot.key = key;
    slot.value_index = next_value_index_;
    ++next_value_index_;
    ++size_;
    return absl::OkStatus();
  }

  const V* FindPtr(const K& key) const {
    const size_t idx = FindSlotIndexForLookup(key);
    if (idx == kNotFound) return nullptr;
    return &values_[table_[idx].value_index];
  }

  bool Contains(const K& key) const { return FindPtr(key) != nullptr; }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  size_t capacity() const { return table_.size(); }

  MemoryMetrics memory_metrics() const {
    MemoryMetrics metrics;
    metrics.live_entries = size_;
    metrics.reserved_capacity = table_.size();
    metrics.tombstone_count = 0;
    metrics.hot_table_bytes = table_.capacity() * sizeof(Slot);
    metrics.payload_storage_bytes = values_.capacity() * sizeof(V);
    metrics.live_payload_bytes = size_ * sizeof(V);
    metrics.effective_load_factor =
        table_.empty() ? 1.0
                       : static_cast<double>(size_) /
                             static_cast<double>(table_.size());
    metrics.capacity_precision = MetricPrecision::kExact;
    metrics.bytes_precision = MetricPrecision::kExact;
    return metrics;
  }

 private:
  struct Slot {
    uint32_t generation = 0;
    K key{};
    size_t value_index = 0;
  };

  static constexpr size_t kNotFound = std::numeric_limits<size_t>::max();

  size_t HashKey(const K& key) const { return MixHash(hasher_(key)); }

  size_t FindSlotIndex(const K& key) const {
    size_t index = HashKey(key) & mask_;
    for (size_t i = 0; i < table_.size(); ++i) {
      const size_t next = (index + 1) & mask_;
      __builtin_prefetch(&table_[next], 0 /* read */, 1 /* low locality */);

      const Slot& slot = table_[index];
      if (slot.generation != generation_) {
        return index;
      }
      if (slot.key == key) {
        return index;
      }
      index = next;
    }
    return 0;
  }

  size_t FindSlotIndexForLookup(const K& key) const {
    size_t index = HashKey(key) & mask_;
    for (size_t i = 0; i < table_.size(); ++i) {
      const size_t next = (index + 1) & mask_;
      __builtin_prefetch(&table_[next], 0 /* read */, 1 /* low locality */);

      const Slot& slot = table_[index];
      if (slot.generation != generation_) {
        return kNotFound;
      }
      if (slot.key == key) {
        return index;
      }
      index = next;
    }
    return kNotFound;
  }

  void Rehash(size_t new_capacity) {
    new_capacity = RoundUpPow2(new_capacity);
    std::vector<Slot> old_table = std::move(table_);
    table_.assign(new_capacity, Slot{});
    mask_ = new_capacity - 1;
    const uint32_t old_generation = generation_;
    generation_ = 1;
    size_ = 0;

    for (const auto& slot : old_table) {
      if (slot.generation != old_generation) continue;
      const size_t idx = FindSlotIndex(slot.key);
      table_[idx] = slot;
      ++size_;
    }
  }

  std::vector<Slot> table_;
  size_t mask_;
  size_t size_;
  uint32_t generation_;
  size_t next_value_index_;
  std::vector<V> values_;
  Hash hasher_;
};

}  // namespace perfmap

#endif  // PERFMAP_SCRATCH_INDIRECT_HASH_MAP_H_
