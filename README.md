# PerfMap — A Cache-Aware Hash Map Performance Engineering Lab

> SFU GDSC Workshop | March 31, 2026
>
> Build, test, benchmark, and extend a custom hash map in C++17, then
> explain exactly where it wins, where it loses, and why.

## Quick Start

```bash
git clone <repo-url>
cd perfmap

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

ctest --test-dir build --output-on-failure
./build/perfmap_bench
```

## What This Repo Contains

PerfMap is not just one container. The repo includes:

- `perfmap::HashMap` as the balanced open-addressing baseline
- workload-tuned variants for read-heavy, churn-heavy, and space-efficient use
- `perfmap::IndirectHashMap` for large payloads
- `perfmap::ScratchHashMap` for repeated clear/rebuild cycles
- `perfmap::ScratchIndirectHashMap` for the strongest current niche:
  large-value scratch/rebuild workloads

The benchmark harness compares those variants against:

- `std::unordered_map`
- `absl::flat_hash_map`
- `absl::node_hash_map`

## Why This Project Exists

PerfMap came from two overlapping goals:

- build something closer to what C++ systems and performance roles
  actually care about
- turn a familiar data structure into a project with measurable,
  workload-specific evidence

The idea was not just "write a hash map from scratch." The idea was to
study how hash map design changes under real usage patterns like request
dedup, batch-local caches, and other temporary lookup-heavy workloads.

Project story and motivation:

- [`docs/PROJECT_STORY.md`](./docs/PROJECT_STORY.md)

## The Honest Performance Story

The repo is intentionally honest:

- the generic map can beat `std::unordered_map` on some locality-friendly
  workloads
- miss-heavy and broad large-value lookup cases still favor stronger baselines
- the strongest current niche is large-value scratch/rebuild work, where
  `ScratchIndirectHashMap` wins by a wide margin

That is the point of the project. Good performance engineering means knowing
when a design helps and when it does not.

## Project Structure

```text
perfmap/
├── CMakeLists.txt
├── README.md
├── include/perfmap/
│   ├── slot.h
│   ├── hash_map.h
│   ├── indirect_hash_map.h
│   ├── scratch_hash_map.h
│   ├── scratch_indirect_hash_map.h
│   └── map_adapters.h
├── tests/
│   ├── hash_map_contract_test.cc
│   ├── hash_map_test.cc
│   ├── scratch_hash_map_test.cc
│   └── scratch_indirect_hash_map_test.cc
├── bench/
│   ├── hash_map_bench.cc
│   └── tradeoffs_bench.cc
├── docs/
│   ├── DESIGN.md
│   ├── RESULTS.md
│   └── IDEAS.md
├── workshop/
│   ├── perfmap_workshop_deck.md
│   ├── presentation_script.md
│   ├── WORKSHOP_STATUS.md
│   └── assets/
└── starter/
    └── perfmap-10d/
```

## What You'll Learn

- open addressing, linear probing, and tombstone-aware deletion
- why memory layout beats theory-only reasoning on modern CPUs
- how tiny API choices affect benchmark results
- how to build fair performance experiments
- how to talk about a systems project in interviews without hand-waving

## Workshop Assets

- Presentation deck: [`workshop/perfmap_workshop_deck.md`](./workshop/perfmap_workshop_deck.md)
- Speaker script: [`workshop/presentation_script.md`](./workshop/presentation_script.md)
- Workshop audit/status sheet: [`workshop/WORKSHOP_STATUS.md`](./workshop/WORKSHOP_STATUS.md)
- Attendee starter project: [`starter/perfmap-10d`](./starter/perfmap-10d)

## Workshop-Friendly Workloads

These are the three easiest real-world stories to explain when presenting
the repo:

- request-scoped dedup / enrichment
- per-batch document or metadata enrichment cache
- graph traversal visited state

The strongest current win is the second one, because large values plus
repeated clear/rebuild cycles are exactly where `ScratchIndirectHashMap`
has a structural advantage.

To rebuild the `.pptx` deck:

```bash
./workshop/build_deck.sh
```

## Dependencies

- C++17 compiler
- CMake 3.14+
- Git
- Internet access on first configure for `FetchContent`

Dependencies are fetched automatically:

- Google Test
- Google Benchmark
- Abseil

## CI

GitHub Actions now runs:

- configure + build
- full test suite
- benchmark smoke checks for both the main benchmark harness and the
  explicit tradeoff benchmarks

Workflow:

- [`.github/workflows/ci.yml`](./.github/workflows/ci.yml)

## Good Extensions After The Workshop

- automated benchmark reporting from JSON output
- memory and probe-length instrumentation
- benchmark regression history and dashboards
- real trace-like workloads
- Robin Hood hashing or metadata-byte probing
- cloud or service wrappers only if they preserve the measurement story

## Resume Framing

Use language like this:

> Built and benchmarked workload-aware hash map variants in C++17, including
> indirect-storage and scratch-map specializations, and used Google Benchmark
> plus Google Test to prove where the design outperformed standard baselines
> and where it did not.

## License

Educational project for SFU GDSC 2026. Use freely.
