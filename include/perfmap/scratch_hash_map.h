#ifndef PERFMAP_SCRATCH_HASH_MAP_H_
#define PERFMAP_SCRATCH_HASH_MAP_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "perfmap/hash_map.h"

namespace perfmap {

// Fixed-capacity-ish scratch map for repeated batch rebuilds.
//
// Real use cases:
// - per-request dedup tables
// - graph traversal visited maps
// - compiler/interpreter temporary symbol maps
// - batch analytics scratch state
//
// The specialization is O(1) Clear():
// each slot stores the "generation" in which it is valid. Clearing the map
// just increments the current generation instead of walking the table.
template <typename K, typename V, typename Hash = std::hash<K>>
class ScratchHashMap {
 public:
  static constexpr std::string_view kBenchmarkName = "perfmap_scratch";
  static constexpr std::string_view kTestName = "PerfMapScratch";
  static constexpr std::string_view kDisplayName =
      "perfmap::ScratchHashMap";

  explicit ScratchHashMap(size_t initial_capacity = 16)
      : table_(RoundUpPow2(initial_capacity)),
        mask_(table_.size() - 1),
        size_(0),
        generation_(1) {}

  void Reserve(size_t expected_size) {
    if (expected_size == 0) return;
    const size_t min_capacity = RoundUpPow2((expected_size * 10 + 6) / 7);
    if (min_capacity > table_.size()) {
      Rehash(min_capacity);
    }
  }

  void Clear() {
    ++generation_;
    size_ = 0;
    if (generation_ == 0) {
      // Wrap-around is extremely unlikely, but make it correct.
      for (auto& slot : table_) {
        slot.generation = 0;
      }
      generation_ = 1;
    }
  }

  absl::Status Insert(const K& key, const V& value) {
    if ((size_ + 1) * 10 > table_.size() * 7) {
      Rehash(table_.size() * 2);
    }

    const size_t idx = FindSlotIndex(key);
    Slot& slot = table_[idx];
    if (slot.generation != generation_) {
      slot.generation = generation_;
      slot.key = key;
      slot.value = value;
      ++size_;
      return absl::OkStatus();
    }

    slot.value = value;
    return absl::OkStatus();
  }

  const V* FindPtr(const K& key) const {
    const size_t idx = FindSlotIndexForLookup(key);
    if (idx == kNotFound) return nullptr;
    return &table_[idx].value;
  }

  bool Contains(const K& key) const { return FindPtr(key) != nullptr; }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

 private:
  struct Slot {
    uint32_t generation = 0;
    K key{};
    V value{};
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
      (void)Insert(slot.key, slot.value);
    }
  }

  std::vector<Slot> table_;
  size_t mask_;
  size_t size_;
  uint32_t generation_;
  Hash hasher_;
};

}  // namespace perfmap

#endif  // PERFMAP_SCRATCH_HASH_MAP_H_
