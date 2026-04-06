# Memory Metrics Methodology

PerfMap reports memory metrics as benchmark user counters and in benchmark JSON
output.

## Reported Metrics

- `live_entries`
- `reserved_capacity`
- `tombstones`
- `hot_table_bytes`
- `payload_storage_bytes`
- `total_reserved_bytes`
- `live_payload_bytes`
- `bytes_per_live_entry`
- `effective_load_factor`
- `memory_bytes_exact`
- `memory_capacity_exact`

## Exact vs Estimated

### Exact for PerfMap Variants

PerfMap containers expose exact internal structural metrics:

- probe-table capacity
- tombstone count where applicable
- hot-table storage bytes
- cold payload storage bytes for indirect variants

For direct open-addressing maps:

- `hot_table_bytes = vector.capacity() * sizeof(slot)`

For indirect variants:

- `hot_table_bytes = table.capacity() * sizeof(index or scratch slot)`
- `payload_storage_bytes = values.capacity() * sizeof(payload storage element)`

### Estimated for Baselines

The STL and Abseil baselines do not expose exact allocator-level memory usage,
so the repo reports documented estimates.

`std::unordered_map` and `absl::node_hash_map`:

- bucket array bytes estimated as `bucket_count * sizeof(void*)`
- live node bytes estimated as
  `live_entries * (sizeof(pair<const K, V>) + 2 pointers)`

`absl::flat_hash_map`:

- slot bytes estimated as `bucket_count * sizeof(pair<const K, V>)`
- control-byte overhead estimated as roughly `bucket_count + 16`

Those estimates are intentionally labeled as estimates. The pipeline does not
pretend they are exact allocator measurements.

## Interpretation

`bytes_per_live_entry` is useful for checking whether a throughput win comes
with obviously heavier memory use.

`effective_load_factor` matters more than raw load for the tombstone-based
open-addressing maps because `(live + tombstones) / capacity` determines probe
growth on the miss path.
