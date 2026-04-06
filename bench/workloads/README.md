# Trace-Like Workload Fixtures

These fixtures are deterministic scenario-derived traces, not production
captures. They exist to stress believable usage patterns that the uniform
synthetic microbenchmarks do not reveal.

Current fixtures:

- `request_dedup_enrichment.csv`
  - request-local repeated ID lookups with inserts on first sight
- `graph_traversal_scratch.csv`
  - traversal-local visited-state checks with repeated resets
- `hotset_phase_shift.csv`
  - long-lived cache with a hot set plus bursty miss phases
- `batch_metadata_enrichment.csv`
  - batch-local large-value enrichment cache with rebuild-heavy access

Every file is generated deterministically by:

```bash
python3 scripts/generate_workload_fixtures.py
```

The benchmark binary replays the checked-in CSVs directly. The generator is
kept in the repo so the fixtures remain explainable, reproducible, and easy to
extend without pretending they came from a production system.
