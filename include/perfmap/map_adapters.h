#ifndef PERFMAP_MAP_ADAPTERS_H_
#define PERFMAP_MAP_ADAPTERS_H_

#include <cstddef>
#include <string_view>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "perfmap/hash_map.h"
#include "perfmap/indirect_hash_map.h"
#include "perfmap/scratch_hash_map.h"
#include "perfmap/scratch_indirect_hash_map.h"

namespace perfmap::adapters {

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

 private:
  map_type map_;
};

}  // namespace perfmap::adapters

#endif  // PERFMAP_MAP_ADAPTERS_H_
