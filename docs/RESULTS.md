# PerfMap Benchmark Results

## Ranked Niche Analysis

Before changing the implementation again, we ranked the most plausible niches
 for this repo:

1. **Large-value scratch / rebuild cache**
   - Real-world scenario: per-request parsed-object cache, per-batch metadata
     map, temporary large-record lookup table in a pipeline.
   - Why generic maps are not ideal: they must clear/destroy large values every
     cycle and still carry general-purpose container semantics.
   - Structural change: O(1) generation-based `Clear()` plus indirect payload
     storage so probe slots stay compact.
   - Likelihood of beating `std::unordered_map`: high
   - Likelihood of beating `absl::node_hash_map`: high
   - Likelihood of beating `absl::flat_hash_map`: medium to high

2. **Broad large-value lookup cache**
   - Real-world scenario: long-lived cache of large metadata records, parsed
     protos, or JSON blobs.
   - Why generic maps are not ideal: inline payload drag or pointer chasing.
   - Structural change: hot/cold split via indirect payload storage.
   - Likelihood of beating `std::unordered_map`: moderate
   - Likelihood of beating `absl::node_hash_map`: moderate
   - Likelihood of beating `absl::flat_hash_map`: low

3. **Scratch / rebuild map with small values**
   - Real-world scenario: visited sets, per-request dedup, batch analytics
     scratch state.
   - Why generic maps are not ideal: repeated `clear -> refill -> query`.
   - Structural change: generation-stamped slots for O(1) `Clear()`.
   - Likelihood of beating `std::unordered_map`: high
   - Likelihood of beating `absl::flat_hash_map`: moderate to high

4. **Generic erase-heavy or mixed open-addressing map**
   - Real-world scenario: long-lived mutable container.
   - Why generic maps are not ideal: tombstones, probe growth, delete costs.
   - Structural change: policy tuning and compaction.
   - Likelihood of beating `absl::flat_hash_map`: low

## Chosen Niche

We chose **large-value scratch / rebuild cache** because it is:

- a real large-value lookup family workload,
- structurally different from a generic map,
- and the strongest honest chance to beat all baselines.

## Implementation

We added:

- `perfmap::IndirectHashMap`
  - hot/cold split for large payload lookup
- `perfmap::ScratchHashMap`
  - O(1) clear for repeated rebuilds
- `perfmap::ScratchIndirectHashMap`
  - combines both ideas:
    - generation-stamped slots
    - compact key/index probe table
    - external payload storage reused across cycles

## Verified Win: Large-Value Scratch / Rebuild Workload

**Use case:** request-scoped parsed protobuf cache, batch-local JSON/object
metadata map, temporary feature-record cache rebuilt each batch.

**Pattern:** `clear -> insert large values -> lookup large values`, repeated
many times.

Why a custom map can win here:

- the table is repeatedly rebuilt from scratch;
- values are large enough that clearing/destroying them every cycle is costly;
- the custom map avoids table clearing with generations;
- the custom map avoids dragging large values through probe slots by storing
  only `key + payload_index` inline.

### Sample Run

```bash
./perfmap_bench \
  --benchmark_filter='(std_unordered_map|absl_flat_hash_map|absl_node_hash_map|perfmap_scratch_indirect_large_value)/large_value_scratch_cycle/16384$' \
  --benchmark_min_time=0.03s
```

### Throughput at 16,384 entries (`items_per_second`)

| Workload | std::unordered_map | absl::flat_hash_map | absl::node_hash_map | perfmap::ScratchIndirectHashMap |
|---|---:|---:|---:|---:|
| large_value_scratch_cycle | 8.03M/s | 4.58M/s | 7.50M/s | **58.40M/s** |

That is roughly:

- **7.27x faster than `std::unordered_map`**
- **12.75x faster than `absl::flat_hash_map`**
- **7.79x faster than `absl::node_hash_map`**

## Honest Mixed Result: Broad Large-Value Lookup

When we tried the broader long-lived large-value lookup problem, the more
general `IndirectHashMap` still lost to `absl::flat_hash_map`.

### Sample Run

```bash
./perfmap_bench \
  --benchmark_filter='(std_unordered_map|absl_flat_hash_map|absl_node_hash_map|perfmap_balanced|perfmap_indirect_large_value)/(large_value_find_hit|large_value_find_miss)/16384$' \
  --benchmark_min_time=0.03s
```

### Throughput at 16,384 entries (`items_per_second`)

| Workload | std | absl::flat | absl::node | perfmap::HashMap | perfmap::IndirectHashMap |
|---|---:|---:|---:|---:|---:|
| large_value_find_hit | 319.4M/s | **426.4M/s** | 260.2M/s | 141.3M/s | 173.8M/s |
| large_value_find_miss | 205.0M/s | **482.3M/s** | 405.6M/s | 65.2M/s | 85.1M/s |

Interpretation:

- `IndirectHashMap` is a real improvement over the old inline-value PerfMap.
- It still does not beat SwissTable on the broad lookup problem.
- That is why we narrowed to the more specific rebuild-heavy large-value niche.

## Concise Conclusion

The truthful conclusion is:

1. **Broad large-value lookup is not the best niche** for this repo if the bar
   is beating `absl::flat_hash_map`.
2. **Large-value scratch / rebuild caches are the best niche we found.**
3. On that niche, `perfmap::ScratchIndirectHashMap` beats:
   - `std::unordered_map`
   - `absl::node_hash_map`
   - `absl::flat_hash_map`

## Regenerating Results

```bash
python3 scripts/run_benchmarks.py --preset full --build-dir build --run-name local-full --enforce-regressions
```

For machine-readable output:

```bash
python3 scripts/run_benchmarks.py --preset smoke --build-dir build --run-name local-smoke --skip-compare
```

Each run directory now contains:

- raw benchmark JSON
- `summary.json`
- `events.jsonl`
- `report.md`
- `report.html`
- `dashboard/`

## Memory and Workload Context

The benchmark JSON now includes counters such as:

- `bytes_per_live_entry`
- `total_reserved_bytes`
- `hot_table_bytes`
- `payload_storage_bytes`

The repo also now includes deterministic trace-like workload coverage for:

- request-scoped dedup/enrichment
- graph traversal scratch state
- hotset phase shifts
- batch-local large-value metadata enrichment
