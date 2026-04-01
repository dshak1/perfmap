# PerfMap Design Document

**Author:** Diar Shakimov
**Date:** March 31, 2026

## Overview

PerfMap is a cache-aware hash map lab built in C++17 for teaching and
benchmarking data-structure trade-offs. The base implementation uses
open addressing, linear probing, tombstone-aware deletion, and a
contiguous slot array. The repo then extends that baseline with
workload-tuned policies plus two specialization families:

- `IndirectHashMap` for large values
- `ScratchHashMap` and `ScratchIndirectHashMap` for repeated clear/rebuild
  workloads

The design goal is not "beat every other map everywhere." The goal is
to build a repo that teaches why memory layout, workload shape, and API
choices change performance.

## Design Decisions

### Why Open Addressing over Separate Chaining?

The core bet in PerfMap is that contiguous memory is often more valuable
than elegant pointer-heavy structure. A chained hash map like
`std::unordered_map` usually stores nodes separately on the heap. That
means a lookup can involve:

- hashing the key
- loading a bucket pointer
- following one or more node pointers
- touching key/value memory that may be far apart in RAM

PerfMap instead stores slots in one flat `std::vector`. Linear probing
then walks neighboring slots, which makes better use of cache lines and
hardware prefetching. That is why PerfMap can win on insertion, steady
hit lookups, and mixed workloads with compact values.

The trade-off is equally important: miss-heavy workloads can still favor
chaining, because a chain miss may terminate quickly while open
addressing keeps probing until it reaches an empty slot.

### Why Linear Probing?

Linear probing was chosen because it keeps the implementation teachable
and the memory-access pattern easy to reason about:

- consecutive probes stay close in memory
- prefetching one slot ahead is straightforward
- correctness invariants are visible in tests
- the benchmark story remains intuitive for a workshop audience

More advanced schemes like Robin Hood hashing or SwissTable-style
metadata can outperform plain linear probing, but they add complexity to
insertion, deletion, and invariants. For a workshop repo, linear probing
is the right starting point because it is simple enough to understand
and still good enough to produce defensible benchmark wins.

### Load Factor Thresholds and Workload Policies

The base map uses a 0.7 effective-load target. That is a pragmatic
middle ground:

- lower thresholds waste memory but keep probe chains shorter
- higher thresholds save memory but increase probe length and tail cost

PerfMap exposes that trade-off directly through policies:

- `BalancedWorkloadPolicy`: 70% max load
- `ReadHeavyPolicy`: 50% max load for shorter hit paths
- `ChurnHeavyPolicy`: 60% max load plus more aggressive tombstone cleanup
- `SpaceEfficientPolicy`: 85% max load with prefetch disabled

This matters pedagogically because the repo shows that "the best data
structure" depends on workload assumptions, not ideology.

### Why Tombstones Instead of Clearing on Erase?

Deleting an occupied slot by marking it empty would break probe chains.
In open addressing, a lookup stops when it reaches the first truly empty
slot. If an erased element in the middle of a cluster were turned back
into `kEmpty`, later elements in that cluster could become unreachable.

PerfMap therefore uses `kDeleted` tombstones:

- `Erase` marks a slot deleted instead of empty
- insertion may reclaim deleted slots
- rehashing compacts tombstones away

The test suite explicitly checks this invariant. That test is one of the
best teaching moments in the repo because it shows why deletion is the
subtle part of open addressing.

### Why `absl::StatusOr` plus `FindPtr()`?

The public API intentionally demonstrates Google-style explicit error
handling:

- `Find` returns `absl::StatusOr<V>`
- `Insert` and `Erase` return `absl::Status`

That makes failure states explicit and easy to discuss in a code review.
At the same time, the repo also teaches that API design affects
performance: `FindPtr()` exists for hot paths because it avoids the miss
path allocation cost associated with building `NotFound` statuses.

This dual API is useful in a workshop because it lets you contrast:

- readable, explicit interfaces for normal code
- zero-allocation fast paths for benchmarks and tight loops

## Architecture

### Core container family

- [`include/perfmap/hash_map.h`](../include/perfmap/hash_map.h)
  - base open-addressing map
  - policy-tuned aliases for balanced, read-heavy, churn-heavy, and
    space-efficient variants
- [`include/perfmap/indirect_hash_map.h`](../include/perfmap/indirect_hash_map.h)
  - hot/cold split for large payloads
- [`include/perfmap/scratch_hash_map.h`](../include/perfmap/scratch_hash_map.h)
  - generation-stamped scratch map for O(1) clear
- [`include/perfmap/scratch_indirect_hash_map.h`](../include/perfmap/scratch_indirect_hash_map.h)
  - large-value scratch specialization

### Benchmarks and fairness layer

- [`include/perfmap/map_adapters.h`](../include/perfmap/map_adapters.h)
  - shared adapter contract across PerfMap, `std::unordered_map`, and
    Abseil baselines
- [`bench/hash_map_bench.cc`](../bench/hash_map_bench.cc)
  - shared workloads and apples-to-apples benchmark registration
- [`bench/tradeoffs_bench.cc`](../bench/tradeoffs_bench.cc)
  - adversarial "where we lose" benchmarks

### Verification layer

- [`tests/hash_map_test.cc`](../tests/hash_map_test.cc)
  - PerfMap-specific invariants
- [`tests/hash_map_contract_test.cc`](../tests/hash_map_contract_test.cc)
  - implementation-agnostic contract coverage
- scratch-map tests for the generation-based variants

## Performance Analysis

The repo currently supports three distinct performance stories:

1. **Generic balanced PerfMap can beat `std::unordered_map` on some
   contiguous, hit-heavy, or mixed workloads.**
2. **PerfMap loses honest cases too, especially miss-heavy workloads and
   broader large-value lookup against `absl::flat_hash_map`.**
3. **The strongest niche is large-value scratch/rebuild workloads, where
   `ScratchIndirectHashMap` combines O(1) clear with indirect payload
   storage.**

The best current result is the large-value scratch/rebuild case reported
in [`docs/RESULTS.md`](./RESULTS.md). On the March 31, 2026 spot check at
16,384 entries, the repo still shows the same qualitative result:

- `std::unordered_map`: about `7.29M items/s`
- `absl::flat_hash_map`: about `3.20M items/s`
- `absl::node_hash_map`: about `7.38M items/s`
- `perfmap::ScratchIndirectHashMap`: about `52.98M items/s`

That is the right niche for the workshop because it is:

- believable
- measurable
- structurally different from a generic container
- stronger than claiming a universal win

The broad large-value lookup case remains an honest loss against
SwissTable-style designs. That makes the repo more credible, not less.

## References

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [Google Benchmark User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Abseil Status Guide](https://abseil.io/docs/cpp/guides/status)
