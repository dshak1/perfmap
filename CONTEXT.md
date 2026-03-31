# PerfMap LLM Context Document

Use this document as paste-in context for any LLM before asking it to teach,
explain, review, demo, or extend this repository.

The goal of this file is not to hype the project. The goal is to describe the
repo accurately enough that the model can help the builder understand it,
present it well, and improve it without inventing features that do not exist.

If you are an LLM reading this:

- Treat this repo as an educational C++ systems project, not a production
  database or a startup product.
- Prefer accurate tradeoffs over marketing language.
- Do not claim that PerfMap is universally faster than the STL.
- Do not ignore `absl::flat_hash_map`; in the current repo it is an important
  baseline and often the strongest general-purpose performer.
- Distinguish between what is implemented, what is benchmarked, and what is
  merely suggested as future work.

## 1. One-Sentence Summary

PerfMap is a small C++17 performance-engineering lab centered on a custom
open-addressing hash map, with tests, benchmarks, workload-tuned variants, and
workshop/demo material designed to teach memory layout, cache locality, and
benchmark-driven tradeoff analysis.

## 2. What This Project Actually Is

This repo is not a full application. It is a focused systems project built
around one data structure.

The real core is:

- a flat hash map implementation in `include/perfmap/hash_map.h`
- a slot representation in `include/perfmap/slot.h`
- adapters that let one benchmark/test harness compare multiple map
  implementations in `include/perfmap/map_adapters.h`
- correctness tests in `tests/`
- microbenchmarks in `bench/`
- workshop assets in `workshop/`

The repo touches these technical areas:

- data structures
- cache locality and memory layout
- open addressing and linear probing
- tombstone-based deletion
- benchmark methodology
- CMake-based C++ builds
- Google Test / Google Benchmark / Abseil usage
- demo-oriented technical storytelling

The project is best understood as:

"A student-friendly systems repo that turns a classic hash table topic into a
measurable engineering exercise."

## 3. What Is Implemented vs What Is Not

Implemented:

- a header-only templated hash map
- tombstone-aware erase
- rehash / growth / compaction
- hash mixing
- power-of-two capacities plus bitmask indexing
- pointer-returning zero-allocation lookup path
- several policy-tuned variants of the same core design
- benchmark adapters for `std::unordered_map`, `absl::flat_hash_map`, and
  PerfMap variants
- typed correctness contract tests that run against every adapter
- a second benchmark suite for adversarial or tradeoff-oriented workloads
- workshop slides, speaker notes, and generated visual assets

Not implemented:

- concurrency
- persistence
- networking / service layer
- a UI or app
- production-grade allocator strategy
- iterator support
- full standard-container semantics
- robust memory accounting / latency percentiles / perf-counter instrumentation
- CI / benchmark regression automation
- an actual end-user product

Important framing:

PerfMap is currently a teaching and demonstration repo, not an infrastructure
component you would drop into production unchanged.

## 4. Repository Map

Top-level files and what they matter for:

- `CMakeLists.txt`
  Build configuration. Uses FetchContent to pull GoogleTest,
  Google Benchmark, and Abseil. Builds `perfmap_tests`,
  `perfmap_bench`, and `perfmap_tradeoffs`.

- `README.md`
  Introductory project page. Good for quick orientation, but some parts are
  still partially templated and should not be treated as the final source of
  truth.

- `docs/DESIGN.md`
  Currently a partially unfinished design template. It is useful as a checklist
  of the questions the builder should be able to answer, but not as a finished
  rationale document.

- `docs/RESULTS.md`
  Current benchmark summary and the most honest high-level results doc in the
  repo.

- `include/perfmap/slot.h`
  Defines the slot state enum and the `Slot<K, V>` struct used by the table.

- `include/perfmap/hash_map.h`
  The core data structure implementation.

- `include/perfmap/map_adapters.h`
  Adapters that normalize operations across `std::unordered_map`,
  `absl::flat_hash_map`, and PerfMap.

- `tests/hash_map_test.cc`
  PerfMap-specific tests, including the tombstone probe-chain invariant.

- `tests/hash_map_contract_test.cc`
  Type-parameterized correctness contract run across every supported map.

- `bench/hash_map_bench.cc`
  Main comparative benchmark harness.

- `bench/tradeoffs_bench.cc`
  Tradeoff-focused benchmarks and negative-result scenarios.

- `workshop/perfmap_workshop_deck.md`
  Slide source for the workshop deck.

- `workshop/presentation_script.md`
  Speaker notes and the intended teaching flow.

- `workshop/generate_assets.py`
  Script that generates workshop diagrams and benchmark summary visuals.

## 5. Core Data Structure

PerfMap uses open addressing rather than separate chaining.

That means:

- all entries live in one contiguous array-like storage
- collisions are resolved by probing to the next candidate slot
- erase cannot simply make a slot "empty" because that would break search for
  later elements in the same probe chain

The physical storage concept is:

`std::vector<Slot<K, V>>`

where each slot contains:

- a state
- a key
- a value

The three states are:

- `kEmpty`: never used; search can stop here
- `kOccupied`: currently holds a live entry
- `kDeleted`: tombstone; keep probing past it

This is the most important invariant in the whole project:

"A deleted slot must not terminate a probe chain."

If you erase by turning `kOccupied` directly into `kEmpty`, you can make valid
elements behind it unreachable.

## 6. Important Implementation Concepts

### 6.1 Open Addressing

Open addressing stores entries inline in one table instead of in separately
allocated nodes. This is often cache-friendlier than linked-node structures
because nearby probes touch nearby memory.

Tradeoff:

- better locality on many workloads
- more sensitivity to load factor and clustering
- erase is trickier
- miss paths can be slower because search may continue across many occupied or
  deleted slots before finding `kEmpty`

### 6.2 Linear Probing

PerfMap uses linear probing. If the desired slot is occupied by a different
key, it checks the next slot, then the next, wrapping around with a bitmask.

Why linear probing is attractive here:

- conceptually simple
- sequential memory access
- very teachable
- easy to benchmark

Downside:

- clustering can become severe if hashing or load management is weak

### 6.3 Tombstones

Erase marks a slot `kDeleted`.

Why:

- lookup must continue probing past deleted slots
- insert can often reuse the first deleted slot it sees

PerfMap also tracks tombstone count separately, which matters because a table
with few live entries can still behave like a "full" table if many slots are
tombstones.

### 6.4 Effective Load Factor

Standard load factor:

`size / capacity`

Effective load factor in this repo:

`(size + tombstones) / capacity`

This matters because probe-chain length is driven by how many slots are
unavailable, not just how many are currently live.

### 6.5 Rehashing and Compaction

PerfMap rehashes in two cases:

- the table genuinely needs to grow
- the table has accumulated enough tombstones that compaction is worth it

This is one of the more thoughtful parts of the implementation because it
recognizes that tombstones are both necessary and performance-dangerous.

### 6.6 Hash Mixing

The repo adds a `MixHash()` step after the user hash function.

Motivation:

- integer hashes can be weak or highly patterned
- power-of-two table sizes are convenient for fast indexing, but they make poor
  low bits especially dangerous

The mix step aims to spread entropy better before masking.

### 6.7 Power-of-Two Capacity and Bitmask Indexing

PerfMap rounds capacity up to a power of two and then computes bucket indices
with:

`hash & (capacity - 1)`

instead of:

`hash % capacity`

Why it exists:

- bitmasking is cheaper than modulo
- it keeps the probing logic simple

Tradeoff:

- you become more sensitive to poor low-bit distribution, so mixing matters

### 6.8 Zero-Allocation Lookup

The repo exposes two lookup flavors:

- `Find(key) -> absl::StatusOr<V>`
- `FindPtr(key) -> const V*`

The second one exists because the first has higher overhead on misses and on
benchmark hot paths. `FindPtr()` is the benchmark-friendly fast path and is one
of the most demo-worthy design choices in the repo because it shows how a small
API decision can materially change performance characteristics.

## 7. Workload Policies and Variants

PerfMap is no longer presented as one universal winner. Instead, the repo
contains one core design with multiple policies:

- `BalancedWorkloadPolicy`
- `ReadHeavyPolicy`
- `ChurnHeavyPolicy`
- `SpaceEfficientPolicy`

These policies mainly change:

- target max load
- tombstone compaction threshold
- whether prefetching is enabled

Why this matters educationally:

- it turns the project from "we built a hashmap" into
  "we explored workload-driven tradeoffs"
- it shows that a tuning choice can be good for one workload and bad for
  another

What not to say:

- do not pretend these policies are a substitute for fundamentally different
  algorithms like Robin Hood hashing or SwissTable metadata
- they are meaningful tuning variations, not a new family of state-of-the-art
  hash tables

## 8. Testing Strategy

There are two layers of testing.

### 8.1 PerfMap-Specific Tests

`tests/hash_map_test.cc` checks:

- basic insert/find behavior
- update/upsert behavior
- erase semantics
- tombstone correctness
- rehash preservation
- load factor expectations
- reserve behavior
- string key support
- repeated insert/erase cycles
- larger entry counts

Most important test to understand:

- `DeleteDoesNotBreakProbeChain`

That test captures the data-structure invariant that makes tombstones necessary.
If a student can explain that test clearly, they understand the heart of the
project.

### 8.2 Implementation-Agnostic Contract Tests

`tests/hash_map_contract_test.cc` runs the same behavioral contract against:

- `std::unordered_map`
- `absl::flat_hash_map`
- PerfMap balanced
- PerfMap read-heavy
- PerfMap churn-heavy
- PerfMap space-efficient

This is a strong engineering choice because it checks that the benchmark
targets are not being compared through incompatible semantics.

## 9. Benchmark Strategy

The benchmark harness in `bench/hash_map_bench.cc` uses adapters so all map
implementations share a common shape:

- reserve
- insert or assign
- find pointer
- contains
- erase
- size
- empty

Benchmark workloads include:

- `insert_grow`
- `insert_reserved`
- `find_hit`
- `find_miss`
- `erase`
- `mixed`
- `churn`

Important methodology points:

- key sets are deterministic
- inserts use unique keys
- lookup hot path uses pointer-returning find for every implementation
- setup is excluded from some measured regions
- all implementations go through the same adapter contract

This is better than most student benchmark setups because it tries to compare
like with like instead of benchmarking one API's slow path against another API's
fast path.

## 10. Tradeoff Benchmarks

The second benchmark file, `bench/tradeoffs_bench.cc`, exists to show where the
story gets less flattering.

Scenarios include:

- miss-heavy workload
- large values
- erase-heavy churn
- reference-stability pattern
- string-key workload

This is one of the best teaching features in the repo because it helps avoid
the fake-student-project pattern of "our thing wins everything."

Important caveat:

some comments in `tradeoffs_bench.cc` describe expected outcomes more strongly
than the current measurements justify. In other words, this file is useful, but
some explanatory comments should be treated as hypotheses rather than final
truth.

## 11. What the Current Results Actually Say

The honest current story from `docs/RESULTS.md` is:

- `absl::flat_hash_map` is the strongest general baseline in the measured
  sample
- PerfMap variants are competitive or strong in some workload categories,
  especially erase and some mixed cases
- PerfMap is weak on miss-heavy lookup paths
- tuning one policy to help a workload can hurt another

This is the most defensible high-level summary:

"PerfMap is a credible educational systems project because it demonstrates real
tradeoffs, not because it replaced industrial hash maps."

Do not reduce the repo to:

"We beat `std::unordered_map`."

That pitch is too shallow and no longer captures the repo's strongest lesson.

## 12. Performance Story You Can Defend

If you need a concise explanation during a demo or interview, say something
close to this:

"PerfMap stores slots contiguously, so some operations benefit from better
cache locality and fewer allocations than node-based hash tables. But open
addressing also suffers on miss-heavy paths because it may need to probe
through many slots before it can conclude a key is absent. The repo's benchmark
story is not that PerfMap wins universally. The point is that layout, API
design, and workload assumptions all matter."

That is the right level of honesty.

## 13. Core Talking Points for a Demo

If you are demoing this repo live, the strongest teaching arc is:

1. Explain the memory layout difference between chained and flat hash tables.
2. Explain why erase is tricky in open addressing.
3. Show the tombstone test.
4. Show why `FindPtr()` exists.
5. Run the benchmarks.
6. Emphasize the wins and the losses.
7. Close on "measure before claiming."

The best "aha" moments are:

- why deleted cannot mean empty
- why API shape affects benchmark results
- why cache locality is not the same thing as universal speed
- why benchmark-backed tradeoffs are more impressive than generic features

## 14. Concepts the Builder Should Understand Well

If the builder wants to speak confidently about this repo, they should be able
to explain all of the following in plain language:

- what a hash map is
- separate chaining vs open addressing
- linear probing
- clustering
- tombstones
- load factor vs effective load factor
- rehashing
- why misses can be expensive in open addressing
- cache locality
- pointer chasing
- contiguous memory
- why `Reserve()` matters
- why a benchmark should normalize APIs across competitors
- why `StatusOr` may be slower than a pointer-returning fast path in a hot loop
- why a benchmark result is workload-dependent
- why `absl::flat_hash_map` is a serious baseline, not an afterthought

## 15. Questions a Strong Reviewer Might Ask

Be prepared for these:

- Why did you choose linear probing over Robin Hood hashing?
- Why are capacities powers of two?
- Why mix the hash if `std::hash` already exists?
- What happens to performance as tombstones accumulate?
- Why not benchmark equal-memory budgets?
- Why does `FindPtr()` exist if `Find()` already exists?
- Why does `absl::flat_hash_map` still beat you on many workloads?
- What production semantics are missing?
- How would large values or string keys change the story?
- What would you build next if you wanted this to become genuinely standout?

## 16. Honest Weaknesses of the Current Repo

These are real weaknesses and should be admitted, not hidden:

- the design doc is unfinished
- some workshop claims are stronger than the current evidence supports
- the benchmark summary asset is hardcoded rather than generated from benchmark
  JSON
- the repo does not yet prove memory overhead or latency distribution
- there is no CI / perf-regression pipeline
- this is still a classic data-structure project, not a novel research system
- erased slots are tombstoned but object lifetime semantics are not as robust as
  a production-quality generic container design

Admitting these weaknesses makes the project more credible.

## 17. Why This Project Still Matters in the Age of Strong Coding Models

An LLM can help generate:

- a first-pass hash map
- a CMake skeleton
- a test scaffold
- a benchmark scaffold
- slide copy

What still requires judgment:

- deciding what workloads to measure
- noticing which claims are actually supported
- interpreting negative results honestly
- identifying the real teaching value
- turning "lots of code" into a coherent story

So the project's value is not pure code volume. It is the combination of:

- systems topic choice
- runnable evidence
- tradeoff reasoning
- teachable structure

## 18. Best Ways to Talk About It on a Resume

Weak version:

"Built a hashmap in C++."

Better version:

"Engineered and benchmarked a custom open-addressing hash map in C++17 with
tombstone-aware deletion, workload-tuned variants, and a shared correctness/
benchmark harness against STL and Abseil baselines."

Even better if supported by current metrics:

"Built and benchmarked a custom open-addressing hash map in C++17, showing
strong erase and mixed-workload performance while documenting miss-path
tradeoffs against `std::unordered_map` and `absl::flat_hash_map`."

The key is to describe:

- what was built
- what was measured
- what tradeoff was found

## 19. Best Ways to Teach It to Students

For first-year students:

- focus on the idea of "data layout changes speed"
- avoid overloading them with Abseil and style-guide details

For second-year students:

- focus on tombstones, rehashing, and benchmark methodology

For third-year students:

- focus on workload modeling, fairness, and what more advanced hash tables
  would add

## 20. If You Are Helping Extend This Repo

Prioritize changes that make the project:

- more measurable
- more reproducible
- more obviously non-generic
- harder to dismiss as "LLM-generated surface area"

Good directions:

- better benchmark rigor
- real-world workload traces
- automatic chart generation from JSON
- memory and latency metrics
- serious algorithmic upgrades like Robin Hood or metadata-byte probing

Weak directions:

- random GUI wrapping
- shallow cloud deployment with no performance story
- generic CRUD or chatbot features unrelated to the hash-map core

## 21. Short Prompt Template for an LLM

If you need a compact prompt after pasting this file, use something like:

"Teach me this repo as if I need to demo it to strong CS students. Focus on
the actual implementation, the data-structure invariants, the benchmark story,
the honest tradeoffs, and the strongest talking points. Do not overclaim or
invent features."

## 22. Final Ground Truth

The strongest honest summary of the repo is:

PerfMap is a focused student systems project with real educational value
because it turns a familiar data structure into a concrete exercise in memory
layout, benchmark design, and technical tradeoff explanation. Its credibility
comes from runnable tests and benchmarks, not from novelty or product scope.
