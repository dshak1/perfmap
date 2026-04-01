# PerfMap 10D Starter

This folder is the **intentionally incomplete** workshop starter version of
PerfMap.

The goal is to give attendees something they can clone, build, and fix during
the session instead of just reading the finished repo.

## What Is Included

- a minimal open-addressing hash map skeleton
- a focused test suite
- TODOs around the two most important ideas:
  - tombstone-aware deletion
  - rehashing when load factor grows too high

## Workshop Goal

By the end of the session, attendees should be able to:

1. explain why open addressing is cache-friendly
2. implement or repair tombstone semantics
3. make the map survive growth and rehashing
4. compare their starter build against the completed main repo

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

## Three-Level Run Guide

### Level 1: First Run

Run this first:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

On the first run, the starter should:

- configure successfully
- build successfully
- pass the basic tests
- fail **exactly two** tests on purpose

Those two failing tests are:

- `HashMapStarterTest.DeleteShouldNotBreakProbeChain`
- `HashMapStarterTest.RehashPreservesEntries`

That is the intended workshop starting point.

### Level 2: Focused Fix Loop

Once you have seen the failures, use targeted reruns:

```bash
ctest --test-dir build -R DeleteShouldNotBreakProbeChain --output-on-failure
ctest --test-dir build -R RehashPreservesEntries --output-on-failure
```

Recommended order:

1. Fix tombstone-aware deletion.
2. Re-run the tombstone test until it turns green.
3. Fix growth / rehash behavior.
4. Re-run the rehash test until it turns green.

That is the fast workshop dopamine loop:

- one broken invariant
- one code change
- one test turning green

### Level 3: Final Check And Compare

When both targeted tests are green, run the full suite again:

```bash
ctest --test-dir build --output-on-failure
```

Then compare your understanding against the completed main repo.

## About Benchmarking

This starter is intentionally **correctness-first**. The quick reward here is
watching broken invariants turn into passing tests.

The bigger performance comparison lives in the completed main repo, which has:

- the full Google Benchmark harness
- workload-tuned map variants
- the large-value scratch/rebuild specialization
- direct comparisons against STL and Abseil baselines

## Important Notes

- The starter project is supposed to have failing tests at first.
- The full polished implementation lives in the main repo root.
- If you split this into a separate GitHub repo later, this folder is the one
  to publish.
