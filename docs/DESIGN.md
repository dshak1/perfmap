# PerfMap Design Document

**Author:** _Your name here_
**Date:** _March 2026_

---

## Overview

PerfMap is a cache-aware, open-addressing hash map implemented in C++17 using
Google-style conventions and tooling.

## Design Decisions

### Why Open Addressing over Separate Chaining?

_TODO: Explain the cache locality argument. How does `std::unordered_map`'s
chaining approach interact with the CPU cache? Why does a flat array help?_

### Why Linear Probing?

_TODO: Discuss the tradeoff between linear probing (cache-friendly, clustering
risk) vs. quadratic probing or double hashing (less clustering, more cache
misses). What did you observe in benchmarks?_

### Load Factor Threshold

_TODO: Why 0.7? What happens at 0.5 vs 0.9? Did you experiment?_

### Tombstone Strategy

_TODO: Explain the kDeleted sentinel. Why can't we just mark slots as kEmpty
on erase? What invariant does the tombstone maintain?_

### Error Handling with absl::StatusOr

_TODO: Why does Google prefer explicit status returns over exceptions?
What are the tradeoffs?_

## Architecture

```
┌─────────────────────────────────────────────┐
│              HashMap<K, V, Hash>            │
│                                             │
│  ┌─────────┬─────────┬─────────┬────────┐  │
│  │ Slot[0] │ Slot[1] │ Slot[2] │  ...   │  │
│  │ state   │ state   │ state   │        │  │
│  │ key     │ key     │ key     │        │  │
│  │ value   │ value   │ value   │        │  │
│  └─────────┴─────────┴─────────┴────────┘  │
│            contiguous in memory             │
│                                             │
│  Operations:                                │
│    Insert(k, v) → absl::Status              │
│    Find(k)      → absl::StatusOr<V>         │
│    Erase(k)     → absl::Status              │
│    Rehash(n)    → void                      │
└─────────────────────────────────────────────┘
```

## Performance Analysis

_TODO: After running benchmarks, paste your results here and explain:_

1. At what size does PerfMap start beating `std::unordered_map`?
2. Why?
3. Where does PerfMap lose, if anywhere?
4. What would you improve next?

## References

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [Google Benchmark User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Abseil Status Guide](https://abseil.io/docs/cpp/guides/status)
