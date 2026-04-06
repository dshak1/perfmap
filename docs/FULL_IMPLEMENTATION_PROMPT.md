# PerfMap Full Implementation Prompt

Paste the following prompt into the coding model you want to use for the next
implementation pass.

---

You are a meticulous senior C++ performance engineer, toolsmith, and build/
release engineer. You care about benchmark fairness, measurement validity,
developer ergonomics, reproducibility, and technical honesty. You do not take
shortcuts. You do not paper over missing depth with presentation polish.

You are working inside the existing `perfmap` repository. Your job is to
implement the next serious phase of the project professionally and defensibly.

## Mission

Fully implement the following missing or partial areas in this repo:

1. **GitHub Actions / CI and benchmark automation**
2. **Memory metrics**
3. **Real or trace-like workloads**
4. **Benchmark regression pipeline**
5. **Runtime observability**
6. **A useful dashboard to watch benchmark runs and inspect results**

This must be done as real engineering work, not as a demo façade.

## Current Repo Context

This repo already contains:

- a base open-addressing `HashMap`
- workload-tuned variants
- `IndirectHashMap`
- `ScratchHashMap`
- `ScratchIndirectHashMap`
- adapter-based benchmarks in `bench/hash_map_bench.cc`
- tradeoff benchmarks in `bench/tradeoffs_bench.cc`
- tests in `tests/`
- CMake + Google Benchmark + Google Test + Abseil
- a basic GitHub Actions workflow in `.github/workflows/ci.yml`
- workshop/docs material in `docs/` and `workshop/`

Important current files to inspect first:

- `CMakeLists.txt`
- `README.md`
- `include/perfmap/hash_map.h`
- `include/perfmap/indirect_hash_map.h`
- `include/perfmap/scratch_hash_map.h`
- `include/perfmap/scratch_indirect_hash_map.h`
- `include/perfmap/map_adapters.h`
- `bench/hash_map_bench.cc`
- `bench/tradeoffs_bench.cc`
- `docs/DESIGN.md`
- `docs/RESULTS.md`
- `docs/PROJECT_STORY.md`
- `docs/IDEAS.md`
- `.github/workflows/ci.yml`

Treat `docs/IDEAS.md` as the source-of-truth ideas doc. Ignore the repo-root
`IDEAS.md` brainstorming scratchpad unless it contains something uniquely
useful.

## What "Done Fully" Means

This project is **not done** if you only:

- add a workflow that still runs only smoke tests
- dump raw benchmark JSON without comparison/reporting
- add "memory metrics" that are actually just `sizeof(map)` or hand-wavy text
- claim "real workloads" but still only use uniform random keys
- add observability by printing lines to stdout
- call a static HTML table a "dashboard"
- leave new code undocumented, untested, or unverified

This project **is done properly** only if:

- the new features are integrated into the actual repo workflow
- benchmark output is reproducible and professionally packaged
- the data/metrics are justified and documented
- the dashboard is useful during execution and after execution
- CI meaningfully enforces the benchmark pipeline
- the implementation preserves benchmark validity rather than polluting the hot
  path with instrumentation

## High-Level Deliverables

Implement all of the following.

### A. CI and GitHub Actions

Upgrade GitHub Actions from a basic smoke-check workflow into a professional
two-tier pipeline:

1. **Fast CI for pushes/PRs**
   - configure + build
   - full tests
   - short benchmark smoke checks
   - sanity-check dashboard/report generation on a tiny dataset

2. **Full benchmark workflow for main / schedule / manual dispatch**
   - run the meaningful benchmark suites
   - emit machine-readable benchmark artifacts
   - emit human-readable report artifacts
   - compare current run to baseline
   - fail or warn on regressions according to documented thresholds
   - publish or upload artifacts in a way that a reviewer can inspect

Requirements:

- preserve a fast PR path and a slower scheduled/manual path
- do not make normal PR CI unusably slow
- document every workflow and its purpose

### B. Memory Metrics

Implement defensible memory metrics for the relevant containers and benchmark
scenarios.

At minimum, support and report:

- live element count
- reserved capacity
- tombstone count where relevant
- bytes per live entry
- total payload storage bytes where relevant
- hot table bytes vs cold payload bytes for indirect variants
- effective load factor where relevant

Requirements:

- the measurement method must be documented
- avoid fake precision
- benchmark user counters and JSON outputs must carry the memory metrics
- metrics must be comparable across at least the major implementations where
  comparison is meaningful
- the docs must explain what is exact, what is estimated, and why

If exact memory accounting is impossible for a baseline container, provide the
best defensible estimate and clearly label it as such.

### C. Real or Trace-Like Workloads

Add workload coverage that is more believable than the current purely synthetic
uniform scenarios.

Implement at least three workload families grounded in the repo's stated use
cases, such as:

- request-scoped dedup / enrichment
- per-batch document or metadata enrichment cache
- graph traversal or scratch-state workloads
- skewed hot-key access
- bursty miss/hit phases

Requirements:

- workloads must be deterministic and reproducible
- they must have checked-in fixtures or checked-in generators
- they must be documented clearly
- they must have a reason for existing tied to the repo story

Preferred approach:

- use small public/open data or trace excerpts if practical and license-safe
- otherwise create documented trace-like fixtures/generators that reflect real
  patterns

If you use synthetic proxies, label them honestly as trace-like or
scenario-derived, not "production traces."

### D. Benchmark Regression Pipeline

Build a full regression pipeline around benchmark results.

This should include:

- a canonical benchmark runner script
- stable JSON output
- report generation
- baseline storage format
- compare-current-vs-baseline tooling
- threshold logic
- clear pass/warn/fail behavior
- artifact packaging

Requirements:

- regression logic must be noise-aware
- protected benchmarks should use repetitions/aggregates, not one noisy run
- the comparison output must be readable by humans and parseable by tooling
- baseline update flow must be explicit and intentional

Good implementation examples:

- a `scripts/` directory with runner/compare/report tools
- a checked-in baseline folder under `bench/` or `results/`
- markdown and/or HTML summaries

### E. Runtime Observability

Implement runtime observability for benchmark execution and pipeline runs.

This should not mean "log every probe inside the hot path."

It should mean:

- structured run events
- stage/run metadata
- current benchmark name
- progress information
- run duration
- output artifact locations
- compare step results
- failure diagnostics

Requirements:

- observability must live around the benchmark runner/pipeline
- do not contaminate measured benchmark loops with logging
- event output should be structured, such as JSONL or equivalent
- the dashboard should consume this observability stream

### F. Dashboard

Build a practical dashboard that is actually useful while a benchmark suite is
running and after it finishes.

The dashboard should show, at minimum:

- current run status
- current benchmark/stage
- recent completed benchmarks
- key throughput results
- memory metrics
- regression status vs baseline
- links to artifacts / report files
- run summary after completion

Preferred characteristics:

- lightweight
- repo-local
- easy to launch
- no massive external stack unless clearly justified

Good acceptable approaches:

- a small Python app serving a local dashboard
- a static HTML report plus a local live-view server
- a simple local web UI driven by structured run events

Weak unacceptable approaches:

- screenshots only
- a markdown file updated by hand
- a terminal printout rebranded as a dashboard

## Technical Quality Bar

You must behave like an experienced engineer, not a feature generator.

### Non-Negotiable Rules

1. **Inspect before editing**
   - understand current architecture before modifying it

2. **Do not break benchmark fairness**
   - keep instrumentation outside timed loops where possible
   - justify any unavoidable measurement overhead

3. **No fake "real workloads"**
   - use documented scenario-based traces or real public data
   - label limitations honestly

4. **No placeholder scripts**
   - if a script exists, it must run and produce useful output

5. **No TODO-driven delivery**
   - do not leave key pieces half-finished

6. **No cosmetic-only docs**
   - documentation must match the actual implementation

7. **No dashboard theater**
   - the dashboard must reflect actual run data

8. **No fragile one-off commands**
   - create reusable entrypoints

9. **No overengineering for its own sake**
   - prefer clean, robust, explainable solutions

10. **Verify everything**
   - run tests
   - run benchmark samples
   - exercise the new scripts
   - validate CI config logically and locally where practical

## Expected Implementation Shape

You are free to choose exact file names, but the final repo should likely gain
some structure close to this:

- `.github/workflows/ci.yml` improvements
- `.github/workflows/benchmarks.yml` or similar
- `scripts/run_benchmarks.*`
- `scripts/compare_benchmarks.*`
- `scripts/generate_benchmark_report.*`
- `scripts/update_baseline.*`
- `bench/workloads/` fixtures and/or generators
- `results/` or `bench/baselines/` for canonical baselines
- `dashboard/` or `tools/dashboard/`
- new docs for:
  - how benchmark regression works
  - how memory metrics are computed
  - how workloads are defined
  - how to run the dashboard

You do **not** have to follow this exact layout, but the final structure should
be coherent and professional.

## Implementation Guidance

### Memory Metrics Guidance

Be thoughtful about memory accounting.

For example:

- PerfMap variants can often expose table capacity and tombstones directly
- indirect variants can separately expose table bytes and payload bytes
- baseline containers may require approximate accounting based on known stored
  elements and conservative assumptions

If you cannot derive a strong estimate for a baseline, say so in code/docs and
exclude that metric from cross-container claims rather than inventing accuracy.

### Workload Guidance

The workload additions should strengthen the repo story, not derail it.

Good candidates:

- batch-local enrichment trace
- request-local dedup trace
- skewed repeated-key trace
- phase-shift trace with bursts of misses

Each workload should answer:

- what real system pattern does this resemble?
- why would a given map design help or hurt?
- what does this workload reveal that uniform random keys do not?

### Regression Guidance

The regression pipeline should be credible.

That means:

- benchmark repetitions on protected suites
- aggregate statistics or median-focused comparison
- threshold configuration checked into the repo
- a clean baseline update procedure

Do not gate on overly noisy comparisons without mitigation.

### Dashboard Guidance

The dashboard should be helpful for both:

- live runs on a developer machine
- post-run inspection in CI artifacts

If a live server is implemented, also save a static report artifact when the
run completes.

## Documentation Requirements

Update the docs so a strong reviewer can understand the new system quickly.

At minimum, update:

- `README.md`
- relevant design/results/story docs

Add any new docs needed, such as:

- benchmark pipeline doc
- dashboard usage doc
- workload doc
- memory metrics methodology doc

Documentation must include:

- what exists
- why it exists
- how to run it
- how to interpret it
- what limitations remain

## Verification Requirements

You must run and verify the implementation before considering the task done.

At minimum:

1. Build the repo
2. Run the test suite
3. Run at least one representative benchmark suite with the new pipeline
4. Generate at least one report artifact
5. Exercise the dashboard locally
6. Confirm the workflows are coherent and reference real scripts/paths

If something cannot be run locally, explain why and reduce uncertainty as much
as possible with the available tooling.

## Final Delivery Format

When you finish, provide:

1. A concise summary of what changed
2. A list of the major files added/updated
3. The commands you ran to verify the work
4. The key artifacts now produced by the repo
5. Any remaining limitations or follow-up items

## Tone and Working Style

Work like a serious C++ performance enthusiast:

- precise
- skeptical
- measurement-driven
- clean in code structure
- honest in claims
- professional in documentation

Do not stop after planning. Implement the work end to end.

---

Optional extra instruction to append when pasting:

"If you identify hidden ambiguity, make the strongest reasonable engineering
decision and document it. Do not bounce the task back unless blocked by a truly
missing external dependency."
