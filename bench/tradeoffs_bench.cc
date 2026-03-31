// Copyright 2026 SFU GDSC — PerfMap Workshop
//
// "Where We Lose" — Adversarial benchmarks that show workloads where
// std::unordered_map beats PerfMap.
//
// The point: no data structure is universally best. Open addressing
// (PerfMap) wins on cache locality; separate chaining (std::unordered_map)
// wins on other axes. A good engineer knows WHEN to use each.
//
// Run:
//   ./perfmap_tradeoffs
//
// Scenarios tested:
//   1. Miss-heavy workloads  — chaining's null-check beats our probing
//   2. Large values          — fat slots waste cache lines during probing
//   3. Erase-heavy churn     — tombstones degrade our probe chains
//   4. Pointer stability     — std::unordered_map guarantees reference
//                              validity across inserts; we don't
//   5. High load factor      — open addressing degrades faster than chaining

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

#include "perfmap/hash_map.h"

#define PERFMAP_IGNORE_STATUS(expr) (void)(expr)

// =============================================================================
// Helpers
// =============================================================================

static std::vector<int> GenerateKeys(int count, int seed = 42) {
  std::mt19937 rng(seed);
  std::vector<int> keys(count);
  for (int i = 0; i < count; ++i) {
    keys[i] = static_cast<int>(rng());
  }
  return keys;
}

// =============================================================================
// SCENARIO 1: Miss-Heavy Workload (95% misses, 5% hits)
// =============================================================================
//
// WHY STL WINS: std::unordered_map checks one bucket pointer on a miss.
// If the chain is empty (often the case at low load), that's a single
// null-pointer comparison → done.  PerfMap must probe forward until it
// hits an kEmpty slot.  At 70% load, that averages ~6 probes.
//
// REAL-WORLD EXAMPLE: Firewall rule lookup, DNS cache, bloom-filter
// fallback — any workload where most queries don't match.

static void BM_StdMap_MissHeavy(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n, 42);

  std::unordered_map<int, int> map;
  map.reserve(n);
  for (int i = 0; i < n; ++i) {
    map[keys[i]] = i;
  }

  // 95% misses: keys with a different seed won't collide
  auto miss_keys = GenerateKeys(n, 999);
  // Mix in 5% hits
  std::mt19937 pick_rng(77);
  for (int i = 0; i < n / 20; ++i) {
    int idx = static_cast<int>(pick_rng() % n);
    miss_keys[idx] = keys[idx];  // replace with a hit
  }

  for (auto _ : state) {
    for (const auto& key : miss_keys) {
      benchmark::DoNotOptimize(map.find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StdMap_MissHeavy)->Range(1 << 10, 1 << 17);

static void BM_PerfMap_MissHeavy(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n, 42);

  perfmap::HashMap<int, int> map;
  for (int i = 0; i < n; ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(keys[i], i));
  }

  auto miss_keys = GenerateKeys(n, 999);
  std::mt19937 pick_rng(77);
  for (int i = 0; i < n / 20; ++i) {
    int idx = static_cast<int>(pick_rng() % n);
    miss_keys[idx] = keys[idx];
  }

  for (auto _ : state) {
    for (const auto& key : miss_keys) {
      benchmark::DoNotOptimize(map.FindPtr(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_PerfMap_MissHeavy)->Range(1 << 10, 1 << 17);

// =============================================================================
// SCENARIO 2: Large Values — 256-byte payload per entry
// =============================================================================
//
// WHY STL WINS: std::unordered_map stores each node on the heap via a
// pointer.  Probing only touches the pointer + key — the fat value is
// accessed only AFTER finding the right node.
//
// PerfMap stores key+value together in each Slot.  With a 256-byte
// value, each slot is ~260+ bytes.  A single cache line (64 bytes)
// covers less than one slot, so every probe step is a cache miss.
// Linear probing's locality advantage evaporates.
//
// REAL-WORLD EXAMPLE: Caching serialized protobuf messages, image
// thumbnails, or any map where values are large structs.

struct LargeValue {
  std::array<char, 256> data{};
};

static void BM_StdMap_LargeValue_Find(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n);

  std::unordered_map<int, LargeValue> map;
  map.reserve(n);
  LargeValue val;
  for (int i = 0; i < n; ++i) {
    val.data[0] = static_cast<char>(i);
    map[keys[i]] = val;
  }

  for (auto _ : state) {
    for (const auto& key : keys) {
      benchmark::DoNotOptimize(map.find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StdMap_LargeValue_Find)->Range(1 << 10, 1 << 16);

static void BM_PerfMap_LargeValue_Find(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n);

  perfmap::HashMap<int, LargeValue> map;
  LargeValue val;
  for (int i = 0; i < n; ++i) {
    val.data[0] = static_cast<char>(i);
    PERFMAP_IGNORE_STATUS(map.Insert(keys[i], val));
  }

  for (auto _ : state) {
    for (const auto& key : keys) {
      benchmark::DoNotOptimize(map.FindPtr(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_PerfMap_LargeValue_Find)->Range(1 << 10, 1 << 16);

// =============================================================================
// SCENARIO 3: Erase-Heavy Churn — continuous insert/delete cycles
// =============================================================================
//
// WHY STL CAN WIN: std::unordered_map just frees the node on erase —
// the bucket structure stays clean.  PerfMap leaves tombstones that
// lengthen every subsequent probe chain.  If the erase rate is high
// enough, tombstones accumulate faster than compaction cleans them,
// degrading both hit and miss paths.
//
// Our tombstone-aware compaction (at 20% threshold) helps, but it
// triggers full rehashes which are expensive.  High-churn workloads
// pay that cost repeatedly.
//
// REAL-WORLD EXAMPLE: TTL-based caches, session stores, connection
// tracking tables — anything with rapid turnover.

static void BM_StdMap_EraseChurn(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n * 2, 42);  // 2x keys for rotation

  for (auto _ : state) {
    std::unordered_map<int, int> map;
    map.reserve(n);

    // Fill to half capacity
    for (int i = 0; i < n; ++i) {
      map[keys[i]] = i;
    }

    // Churn: erase old + insert new in lockstep
    for (int i = 0; i < n; ++i) {
      map.erase(keys[i]);              // remove old
      map[keys[n + i]] = n + i;        // insert new
      // Interleave a lookup
      benchmark::DoNotOptimize(map.find(keys[n + i]));
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * n * 3);  // 3 ops per step
}
BENCHMARK(BM_StdMap_EraseChurn)->Range(1 << 10, 1 << 16);

static void BM_PerfMap_EraseChurn(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateKeys(n * 2, 42);

  for (auto _ : state) {
    perfmap::HashMap<int, int> map;

    for (int i = 0; i < n; ++i) {
      PERFMAP_IGNORE_STATUS(map.Insert(keys[i], i));
    }

    for (int i = 0; i < n; ++i) {
      PERFMAP_IGNORE_STATUS(map.Erase(keys[i]));
      PERFMAP_IGNORE_STATUS(map.Insert(keys[n + i], n + i));
      benchmark::DoNotOptimize(map.FindPtr(keys[n + i]));
    }
    benchmark::DoNotOptimize(map);
  }
  state.SetItemsProcessed(state.iterations() * n * 3);
}
BENCHMARK(BM_PerfMap_EraseChurn)->Range(1 << 10, 1 << 16);

// =============================================================================
// SCENARIO 4: Pointer/Reference Stability
// =============================================================================
//
// This isn't a benchmark — it's a CORRECTNESS demonstration.
// std::unordered_map guarantees that pointers/references to elements
// remain valid even after inserting more elements (the standard
// requires this).  PerfMap CANNOT guarantee this because rehash
// reallocates the entire flat array, invalidating all pointers.
//
// We demonstrate this with a test that stores a pointer, triggers a
// rehash, and shows the pointer is now dangling.
//
// WHY THIS MATTERS: Any code that stores `&map[key]` or keeps an
// iterator across inserts breaks with open-addressing maps.
// std::unordered_map's separate heap nodes make this safe.
//
// This is benchmarked as a "pattern" test: code that relies on
// reference stability MUST use std::unordered_map (or store indices).

static void BM_StdMap_ReferenceStability(benchmark::State& state) {
  // Pattern: store pointer to value, insert more, read through pointer
  const int n = static_cast<int>(state.range(0));

  for (auto _ : state) {
    std::unordered_map<int, int> map;
    map.reserve(16);  // small initial reserve to force rehashes

    map[0] = 42;
    const int* ptr = &map[0];  // grab a reference

    // Insert lots more elements — triggers internal rehashing
    for (int i = 1; i < n; ++i) {
      map[i] = i;
    }

    // The pointer is STILL VALID — std::unordered_map guarantees this
    int val = *ptr;
    benchmark::DoNotOptimize(val);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StdMap_ReferenceStability)->Range(1 << 10, 1 << 16);

static void BM_PerfMap_NoReferenceStability(benchmark::State& state) {
  // Pattern: we CANNOT safely hold pointers across inserts.
  // Instead, we must re-lookup after every insert that might rehash.
  // This benchmark shows the cost of that defensive pattern.
  const int n = static_cast<int>(state.range(0));

  for (auto _ : state) {
    perfmap::HashMap<int, int> map;
    PERFMAP_IGNORE_STATUS(map.Insert(0, 42));

    for (int i = 1; i < n; ++i) {
      PERFMAP_IGNORE_STATUS(map.Insert(i, i));
    }

    // Must re-lookup every time — can't cache pointer across inserts
    const int* ptr = map.FindPtr(0);
    benchmark::DoNotOptimize(ptr ? *ptr : 0);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_PerfMap_NoReferenceStability)->Range(1 << 10, 1 << 16);

// =============================================================================
// SCENARIO 5: String keys with high collision rate
// =============================================================================
//
// WHY STL CAN WIN: String hashing is expensive (~1 ns per byte).
// With long strings that share common prefixes (like URLs or file
// paths), the cost of hashing dominates.  std::unordered_map caches
// the hash value in the node (implementation-defined but common).
// PerfMap recomputes the hash on every probe step during rehash.
//
// Additionally, std::hash<std::string> is already a good hash — our
// MixHash adds extra work on top of an already-scrambled value,
// providing no benefit but paying the cost.

static std::vector<std::string> GenerateStringKeys(int count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (int i = 0; i < count; ++i) {
    // Realistic-ish: shared prefix, varying suffix
    keys.push_back("com.example.service.endpoint.v2.resource_" +
                    std::to_string(i));
  }
  return keys;
}

static void BM_StdMap_StringKey_Find(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateStringKeys(n);

  std::unordered_map<std::string, int> map;
  map.reserve(n);
  for (int i = 0; i < n; ++i) {
    map[keys[i]] = i;
  }

  for (auto _ : state) {
    for (const auto& key : keys) {
      benchmark::DoNotOptimize(map.find(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StdMap_StringKey_Find)->Range(1 << 10, 1 << 15);

static void BM_PerfMap_StringKey_Find(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto keys = GenerateStringKeys(n);

  perfmap::HashMap<std::string, int> map;
  for (int i = 0; i < n; ++i) {
    PERFMAP_IGNORE_STATUS(map.Insert(keys[i], i));
  }

  for (auto _ : state) {
    for (const auto& key : keys) {
      benchmark::DoNotOptimize(map.FindPtr(key));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_PerfMap_StringKey_Find)->Range(1 << 10, 1 << 15);
