#ifndef PERFMAP_INDIRECT_HASH_MAP_H_
#define PERFMAP_INDIRECT_HASH_MAP_H_

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "perfmap/hash_map.h"
#include "perfmap/memory_metrics.h"
#include "perfmap/slot.h"

namespace perfmap {

// Hash map specialized for large payloads and lookup-heavy workloads.
//
// Core idea:
// - Keep the probe table small: state + key + payload index.
// - Store the actual values in a separate contiguous vector.
//
// This is a hot/cold split:
// - Hot path: probing touches only compact metadata and keys.
// - Cold path: the value is accessed only after we know the key matched.
//
// That trade-off is especially useful when values are large structs, cached
// blobs, or other payloads that would otherwise inflate every probe step.
template <typename K, typename V, typename Hash = std::hash<K>>
class IndirectHashMap {
 public:
  static constexpr std::string_view kBenchmarkName =
      "perfmap_indirect_large_value";
  static constexpr std::string_view kTestName = "PerfMapIndirectLargeValue";
  static constexpr std::string_view kDisplayName =
      "perfmap::IndirectHashMap";

  explicit IndirectHashMap(size_t initial_capacity = 16)
      : table_(RoundUpPow2(initial_capacity)),
        mask_(table_.size() - 1),
        size_(0),
        tombstone_count_(0) {}

  absl::Status Insert(const K& key, const V& value) {
    if (table_.empty()) {
      return absl::InternalError("table has zero capacity");
    }
    if (ShouldRehash()) {
      GrowOrCompact();
    }

    const size_t idx = FindSlotIndex(key);
    if (table_[idx].state == SlotState::kOccupied) {
      values_[table_[idx].value_index] = value;
      return absl::OkStatus();
    }

    if (table_[idx].state == SlotState::kDeleted) {
      --tombstone_count_;
    }

    table_[idx].state = SlotState::kOccupied;
    table_[idx].key = key;
    table_[idx].value_index = AllocateValueSlot(value);
    ++size_;
    return absl::OkStatus();
  }

  const V* FindPtr(const K& key) const {
    if (table_.empty()) return nullptr;
    const size_t idx = FindSlotIndexForLookup(key);
    if (idx == kNotFound) return nullptr;
    return &*values_[table_[idx].value_index];
  }

  bool Contains(const K& key) const { return FindPtr(key) != nullptr; }

  absl::Status Erase(const K& key) {
    if (table_.empty()) {
      return absl::NotFoundError("empty table");
    }
    const size_t idx = FindSlotIndexForLookup(key);
    if (idx == kNotFound) {
      return absl::NotFoundError("key not found");
    }

    free_value_indices_.push_back(table_[idx].value_index);
    values_[table_[idx].value_index].reset();
    table_[idx].state = SlotState::kDeleted;
    --size_;
    ++tombstone_count_;
    return absl::OkStatus();
  }

  void Reserve(size_t expected_size) {
    if (expected_size == 0) return;
    // Lookup-heavy specialization: keep the probe table at <= 50% load.
    const size_t min_capacity = RoundUpPow2(expected_size * 2);
    if (min_capacity > table_.size()) {
      Rehash(min_capacity);
    }
    values_.reserve(expected_size);
    free_value_indices_.reserve(expected_size / 8 + 1);
  }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  size_t capacity() const { return table_.size(); }
  size_t tombstone_count() const { return tombstone_count_; }

  MemoryMetrics memory_metrics() const {
    MemoryMetrics metrics;
    metrics.live_entries = size_;
    metrics.reserved_capacity = table_.size();
    metrics.tombstone_count = tombstone_count_;
    metrics.hot_table_bytes = table_.capacity() * sizeof(IndexSlot);
    metrics.payload_storage_bytes =
        values_.capacity() * sizeof(typename decltype(values_)::value_type);
    metrics.live_payload_bytes = size_ * sizeof(V);
    metrics.effective_load_factor =
        table_.empty() ? 1.0
                       : static_cast<double>(size_ + tombstone_count_) /
                             static_cast<double>(table_.size());
    metrics.capacity_precision = MetricPrecision::kExact;
    metrics.bytes_precision = MetricPrecision::kExact;
    return metrics;
  }

  void Rehash(size_t new_capacity) {
    new_capacity = RoundUpPow2(new_capacity);
    std::vector<IndexSlot> old_table = std::move(table_);
    table_.assign(new_capacity, IndexSlot{});
    mask_ = new_capacity - 1;
    size_ = 0;
    tombstone_count_ = 0;

    for (auto& slot : old_table) {
      if (slot.state != SlotState::kOccupied) continue;
      const size_t idx = FindSlotIndex(slot.key);
      table_[idx] = std::move(slot);
      ++size_;
    }
  }

 private:
  struct IndexSlot {
    SlotState state = SlotState::kEmpty;
    K key{};
    size_t value_index = 0;
  };

  static constexpr size_t kNotFound = std::numeric_limits<size_t>::max();

  size_t HashKey(const K& key) const { return MixHash(hasher_(key)); }

  bool ShouldRehash() const {
    return (size_ + tombstone_count_) * 2 > table_.size();
  }

  void GrowOrCompact() {
    // Compact aggressively when tombstones build up on this lookup-heavy map.
    if (tombstone_count_ * 8 > table_.size()) {
      Rehash(table_.size());
    } else {
      Rehash(table_.size() * 2);
    }
  }

  size_t AllocateValueSlot(const V& value) {
    if (!free_value_indices_.empty()) {
      const size_t idx = free_value_indices_.back();
      free_value_indices_.pop_back();
      values_[idx] = value;
      return idx;
    }
    values_.push_back(value);
    return values_.size() - 1;
  }

  size_t FindSlotIndex(const K& key) const {
    size_t index = HashKey(key) & mask_;
    size_t first_deleted = kNotFound;

    for (size_t i = 0; i < table_.size(); ++i) {
      const size_t next = (index + 1) & mask_;
      __builtin_prefetch(&table_[next], 0 /* read */, 1 /* low locality */);

      const auto& slot = table_[index];
      if (slot.state == SlotState::kEmpty) {
        return (first_deleted != kNotFound) ? first_deleted : index;
      }
      if (slot.state == SlotState::kOccupied && slot.key == key) {
        return index;
      }
      if (slot.state == SlotState::kDeleted && first_deleted == kNotFound) {
        first_deleted = index;
      }
      index = next;
    }

    return (first_deleted != kNotFound) ? first_deleted : 0;
  }

  size_t FindSlotIndexForLookup(const K& key) const {
    size_t index = HashKey(key) & mask_;

    for (size_t i = 0; i < table_.size(); ++i) {
      const size_t next = (index + 1) & mask_;
      __builtin_prefetch(&table_[next], 0 /* read */, 1 /* low locality */);

      const auto& slot = table_[index];
      if (slot.state == SlotState::kEmpty) {
        return kNotFound;
      }
      if (slot.state == SlotState::kOccupied && slot.key == key) {
        return index;
      }
      index = next;
    }

    return kNotFound;
  }

  std::vector<IndexSlot> table_;
  size_t mask_;
  size_t size_;
  size_t tombstone_count_;
  std::vector<std::optional<V>> values_;
  std::vector<size_t> free_value_indices_;
  Hash hasher_;
};

}  // namespace perfmap

#endif  // PERFMAP_INDIRECT_HASH_MAP_H_
