# Trace-Like Workloads

PerfMap now includes deterministic trace-like workloads in addition to the
uniform synthetic microbenchmarks.

These are not production traces. They are documented scenario-derived fixtures
checked into `bench/workloads/`.

## Current Workload Families

### Request-Scoped Dedup / Enrichment

Fixture:

- `bench/workloads/request_dedup_enrichment.csv`

Pattern:

- clear per request
- repeated IDs inside a request
- find before insert to model reuse detection

What it reveals:

- reset cost
- benefit of scratch-style reuse
- lookup cost under repeated local reuse

### Graph Traversal Scratch State

Fixture:

- `bench/workloads/graph_traversal_scratch.csv`

Pattern:

- fresh visited map per traversal
- repeated neighbor checks
- many membership tests against temporary state

What it reveals:

- why O(1) clear matters
- why temporary-state workloads differ from long-lived mutable maps

### Hotset Phase Shift

Fixture:

- `bench/workloads/hotset_phase_shift.csv`

Pattern:

- long-lived inserted set
- highly skewed hot lookups
- warm accesses
- miss bursts

What it reveals:

- how read-heavy or space-efficient tuning reacts to skew and misses

### Batch Metadata Enrichment

Fixture:

- `bench/workloads/batch_metadata_enrichment.csv`

Pattern:

- clear per batch
- insert large values
- repeatedly hit a hot subset plus some broad lookups and misses

What it reveals:

- why `ScratchIndirectHashMap` is the strongest current niche
- why hot/cold storage matters for large-value scratch caches

## Regeneration

Fixtures are generated deterministically by:

```bash
python3 scripts/generate_workload_fixtures.py
```
