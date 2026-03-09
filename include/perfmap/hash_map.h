// Copyright 2026 SFU GDSC — PerfMap Workshop
//
// A cache-aware, open-addressing hash map built from scratch.
//
// Design choices (Google-style rationale):
//   - Flat storage via std::vector<Slot>  → cache-friendly linear probing
//   - absl::StatusOr / absl::Status       → explicit error handling (no exceptions)
//   - Tombstone-aware deletion             → correct probe-chain semantics
//   - Load-factor-triggered rehash at 0.7  → amortized O(1) ops
//
// This is NOT a production hash map. It is an educational implementation
// designed to teach performance engineering concepts and Google-style C++.

#ifndef PERFMAP_HASH_MAP_H_
#define PERFMAP_HASH_MAP_H_

#include <cstddef>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "perfmap/slot.h"

namespace perfmap {

// A simple open-addressing hash map with linear probing.
//
// Template parameters:
//   K    — key type   (must be equality-comparable and hashable)
//   V    — value type
//   Hash — hash functor (defaults to std::hash<K>)
//
// Example usage:
//   HashMap<int, std::string> map;
//   auto status = map.Insert(42, "answer");
//   auto result = map.Find(42);
//   if (result.ok()) {
//     std::cout << *result << std::endl;  // "answer"
//   }
template <typename K, typename V, typename Hash = std::hash<K>>
class HashMap {
 public:
  // Construct a map with the given initial bucket count.
  // Capacity must be > 0.  A power-of-two is ideal but not required.
  explicit HashMap(size_t initial_capacity = 16)
      : table_(initial_capacity), size_(0) {}

  // -----------------------------------------------------------------------
  // Core API
  // -----------------------------------------------------------------------

  // Insert a key-value pair.  If the key already exists, its value is
  // updated (upsert semantics).  May trigger a rehash.
  absl::Status Insert(const K& key, const V& value) {
    if (table_.empty()) {
      return absl::InternalError("table has zero capacity");
    }
    if (load_factor() > kMaxLoadFactor) {
      Rehash(table_.size() * 2);
    }
    size_t idx = FindSlotIndex(key);
    if (table_[idx].state == SlotState::kOccupied) {
      // Key already present — update value
      table_[idx].value = value;
      return absl::OkStatus();
    }
    // Insert into empty or tombstone slot
    table_[idx] = {SlotState::kOccupied, key, value};
    ++size_;
    return absl::OkStatus();
  }

  // Look up a key.  Returns the associated value or NOT_FOUND.
  absl::StatusOr<V> Find(const K& key) const {
    if (table_.empty()) {
      return absl::NotFoundError("empty table");
    }
    size_t idx = FindSlotIndexForLookup(key);
    if (idx == kNotFound) {
      return absl::NotFoundError("key not found");
    }
    return table_[idx].value;
  }

  // Erase a key by placing a tombstone.  Returns NOT_FOUND if absent.
  absl::Status Erase(const K& key) {
    if (table_.empty()) {
      return absl::NotFoundError("empty table");
    }
    size_t idx = FindSlotIndexForLookup(key);
    if (idx == kNotFound) {
      return absl::NotFoundError("key not found");
    }
    table_[idx].state = SlotState::kDeleted;
    --size_;
    return absl::OkStatus();
  }

  // -----------------------------------------------------------------------
  // Observers
  // -----------------------------------------------------------------------

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  size_t capacity() const { return table_.size(); }

  double load_factor() const {
    if (table_.empty()) return 1.0;
    return static_cast<double>(size_) / static_cast<double>(table_.size());
  }

  // -----------------------------------------------------------------------
  // Internals (public for educational / benchmarking access)
  // -----------------------------------------------------------------------

  // Force a rehash to a new capacity.  All occupied entries are
  // re-inserted; tombstones are discarded (table compaction).
  void Rehash(size_t new_capacity) {
    std::vector<Slot<K, V>> old_table = std::move(table_);
    table_.assign(new_capacity, Slot<K, V>{});
    size_ = 0;
    for (auto& slot : old_table) {
      if (slot.state == SlotState::kOccupied) {
        // Ignore status — we just resized, so inserts cannot fail.
        (void)Insert(std::move(slot.key), std::move(slot.value));
      }
    }
  }

 private:
  // Maximum load factor before rehash triggers.
  // 0.7 is a well-studied sweet spot for open addressing — high enough
  // to use memory efficiently, low enough to keep probe chains short.
  static constexpr double kMaxLoadFactor = 0.7;

  // Sentinel value meaning "not found" in lookup paths.
  static constexpr size_t kNotFound = std::numeric_limits<size_t>::max();

  // Find the appropriate slot index for a key (used by Insert).
  //
  // Returns:
  //   - The index of the existing occupied slot if the key is present.
  //   - Otherwise, the best index to insert into: the first tombstone
  //     encountered along the probe chain, or the first empty slot.
  //
  // Probing strategy: LINEAR.
  //   Why linear?  Because sequential memory access is cache-friendly.
  //   The CPU prefetcher loves predictable stride-1 patterns.  Quadratic
  //   or double-hashing scatter across memory and defeat the prefetcher.
  //   The downside — primary clustering — is managed by keeping load
  //   factor ≤ 0.7.
  size_t FindSlotIndex(const K& key) const {
    size_t index = hasher_(key) % table_.size();
    size_t first_deleted = kNotFound;

    // Probe until we hit an empty slot (end of chain).
    for (size_t i = 0; i < table_.size(); ++i) {
      const auto& slot = table_[index];

      if (slot.state == SlotState::kEmpty) {
        // End of probe chain — key is not in the table.
        // Return first tombstone if we passed one, else this empty slot.
        return (first_deleted != kNotFound) ? first_deleted : index;
      }

      if (slot.state == SlotState::kOccupied && slot.key == key) {
        // Found existing key.
        return index;
      }

      if (slot.state == SlotState::kDeleted && first_deleted == kNotFound) {
        // Remember the first tombstone — we can reuse it for insert.
        first_deleted = index;
      }

      index = (index + 1) % table_.size();  // Linear probe
    }

    // Full table traversal without finding empty — shouldn't happen if
    // load factor is managed, but return first_deleted as a fallback.
    return (first_deleted != kNotFound) ? first_deleted : 0;
  }

  // Find the slot containing an existing key (used by Find / Erase).
  // Returns kNotFound if the key is not present.
  size_t FindSlotIndexForLookup(const K& key) const {
    size_t index = hasher_(key) % table_.size();

    for (size_t i = 0; i < table_.size(); ++i) {
      const auto& slot = table_[index];

      if (slot.state == SlotState::kEmpty) {
        return kNotFound;  // End of probe chain
      }
      if (slot.state == SlotState::kOccupied && slot.key == key) {
        return index;      // Found it
      }
      index = (index + 1) % table_.size();
    }
    return kNotFound;
  }

  std::vector<Slot<K, V>> table_;
  size_t size_;
  Hash hasher_;
};

}  // namespace perfmap

#endif  // PERFMAP_HASH_MAP_H_
