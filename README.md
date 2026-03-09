# PerfMap — A Cache-Aware Hash Map Performance Engineering Lab

> SFU GDSC Workshop | March 10, 2026
>
> Build, test, benchmark, and optimize a custom hash map using Google's
> open-source toolchain — then prove it's faster than `std::unordered_map`
> with hard numbers.

---

## Quick Start

```bash
# Clone
git clone <repo-url>
cd perfmap

# Build (Release mode for meaningful benchmarks)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Run tests (correctness first!)
./perfmap_tests

# Run benchmarks
./perfmap_bench
```

## What You'll Learn

- **Hash table internals:** open addressing, linear probing, tombstones, rehashing
- **Performance engineering:** why cache locality beats algorithmic cleverness on modern CPUs
- **Google-style C++:** `absl::StatusOr`, naming conventions, code structure
- **Google tooling:** Google Test for correctness, Google Benchmark for measurement
- **Engineering discipline:** "if you didn't measure it, you didn't optimize it"

## Project Structure

```
perfmap/
├── CMakeLists.txt              # Build system — FetchContent pulls all deps
├── .clang-format               # Google style auto-formatting
├── .clang-tidy                 # Static analysis config
├── include/
│   └── perfmap/
│       ├── slot.h              # SlotState enum + Slot<K,V> struct
│       └── hash_map.h          # The hash map implementation
├── bench/
│   └── hash_map_bench.cc       # Benchmarks: PerfMap vs std::unordered_map
├── tests/
│   └── hash_map_test.cc        # Google Test correctness suite
└── docs/
    ├── DESIGN.md               # Your design rationale (fill this in!)
    └── RESULTS.md              # Your benchmark results (fill this in!)
```

## Dependencies (auto-fetched by CMake)

| Library | What it does | Google? |
|---------|-------------|---------|
| [Google Test](https://github.com/google/googletest) | Unit testing framework | Yes |
| [Google Benchmark](https://github.com/google/benchmark) | Microbenchmark harness | Yes |
| [Abseil](https://github.com/abseil/abseil-cpp) | C++ standard library extensions | Yes |

## Requirements

- **C++17** compiler (GCC 7+, Clang 5+, MSVC 2019+)
- **CMake** 3.14+
- **Git** (for FetchContent to pull dependencies)
- Internet connection on first build (to download dependencies)

## Resume Bullet (Adapt This)

> Engineered a cache-aware open-addressing hash map in C++, achieving X%
> faster lookups than std::unordered_map — benchmarked with Google Benchmark,
> tested with Google Test, built with Google-style conventions (absl::StatusOr,
> linear probing, tombstone-aware deletion).

## Extension Ideas

After the workshop, pick a direction and make it yours:

| Track | Ideas | Difficulty |
|-------|-------|-----------|
| **Performance** | Robin Hood hashing, SIMD probing, prefetch hints, custom allocator | Hard |
| **Concurrency** | Mutex-guarded, sharded locks, lock-free SPSC | Hard |
| **Engineering** | GitHub Actions CI, benchmark regression detection, ASan/TSan | Medium |
| **Visualization** | Python script to plot benchmark JSON as charts | Medium |
| **Distributed** | Wrap as a gRPC key-value store service | Advanced |
| **Data Structures** | Extend to LRU cache, skip list, or B-tree | Medium |

## Google Style C++ Cheat Sheet

| Convention | Example |
|-----------|---------|
| 2-space indent | `if (x) {∙∙...` |
| Types: CamelCase | `class HashMap`, `struct Slot` |
| Functions: CamelCase | `void Insert(...)`, `size_t FindSlotIndex(...)` |
| Variables: snake_case | `size_t table_size`, `int load_count` |
| Private members: trailing _ | `size_t size_;`, `Hash hasher_;` |
| Constants: k-prefix | `static constexpr double kMaxLoadFactor` |
| Error handling | `absl::StatusOr<V>` not exceptions |

## License

Educational project — SFU GDSC 2026. Use freely.
