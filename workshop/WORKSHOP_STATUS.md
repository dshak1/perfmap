# PerfMap Workshop Status

Snapshot taken on **March 31, 2026**.

## 1. Workshop-Critical Checklist

| Item | Status | Evidence | Next Action |
|---|---|---|---|
| Repo builds cleanly | Implemented | `cmake --build build -j8` succeeded | None |
| Tests pass | Implemented | `ctest --test-dir build --output-on-failure` passed `96/96` tests | None |
| Benchmark story is still honest | Implemented | Spot checks confirm the same win/loss pattern in `docs/RESULTS.md` | Keep using the scratch-large-value niche and the broad-lookup loss |
| A strong real-world scenario exists for the winning workload | Partial | `docs/RESULTS.md` names request/batch cache patterns | Tighten the talk track around a "per-batch document enrichment cache" |
| Slides contain bullet points only | Partial before deck rewrite | Existing deck had bullets, but not enough audience interaction | Updated in `workshop/perfmap_workshop_deck.md` |
| Speaker notes exist | Partial before deck rewrite | `workshop/presentation_script.md` existed, but was too thin for a two-hour room | Expanded script and embedded notes in the deck |
| Audience-engagement questions are built into the deck | Missing before deck rewrite | Old deck only had guest questions near the end | Added early and mid-talk participation prompts |
| Honest win + honest loss are both explained | Implemented | `docs/RESULTS.md`, `bench/tradeoffs_bench.cc`, `CONTEXT.md` | Keep this central in the talk |
| Design doc is presentation-ready | Missing before this pass | `docs/DESIGN.md` still had TODOs | Replaced with a complete design writeup |
| Attendee starter repo/folder exists | Missing before this pass | No starter skeleton existed | Added `starter/perfmap-10d` |

## 2. Status Matrix for `docs/IDEAS.md`

### Highest-leverage near-term extensions

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| Auto-generate benchmark reports | Partial | `workshop/generate_assets.py`, benchmark JSON output support in `docs/RESULTS.md` | Assets exist, but there is no script that runs benchmarks and regenerates markdown tables directly from JSON |
| Memory metrics | Missing | `CONTEXT.md` explicitly calls this out as a gap | No bytes-per-entry or reserved-vs-live reporting yet |
| Probe-length instrumentation | Missing | No probe histogram or probe counters in code | Good next serious engineering upgrade |

### Serious systems upgrades

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| Robin Hood hashing | Missing | Mentioned in docs only | Good future advanced branch, not current workshop scope |
| SwissTable-style metadata bytes | Missing | Mentioned in docs only | Important extension, not implemented |
| SIMD / NEON probe filtering | Missing | Mentioned in docs only | No SIMD-specific code paths exist |
| Better storage semantics / move-only support | Partial | Current maps work for ordinary copyable values | Object-lifetime and generic-container rigor are still below production container level |

### Cloud / DevOps extensions

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| Benchmark regression pipeline | Partial | `.github/workflows/ci.yml` now builds, tests, and runs benchmark smoke checks | Historical result storage, dashboards, and scheduled regression tracking are still missing |
| Multi-machine benchmarking | Missing | No cross-arch or cloud-run automation | Current results are local-only |
| PerfMap-as-a-Service | Missing | No service wrapper exists | Do not claim this is done |

### Open-source and user path

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| Real workshop/demo usage | Implemented | `workshop/` deck, script, assets | This is the strongest current "user" story |
| Contributor guide | Missing | No `CONTRIBUTING.md` | Easy open-source polish item |
| Issue labels / good first issues | Missing | Repo metadata not present locally | GitHub-side task |
| Extension roadmap | Partial | `docs/IDEAS.md` already acts as a roadmap | Could later be condensed into a contributor-facing roadmap |
| Honest comparisons | Implemented | `bench/tradeoffs_bench.cc`, `docs/RESULTS.md` | One of the repo's strengths |

### App / wrapper ideas

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| VS Code extension | Missing | No extension files | Mention only as a future track |
| Desktop/Web visualizer | Missing | No UI app exists | Also a future track |
| App Store angle | Missing | No app wrapper exists | Not worth discussing as current scope |

### Real workloads and measurement

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| Multiple synthetic workload families | Implemented | `bench/hash_map_bench.cc` | Grow, reserved, hit, miss, erase, mixed, churn, large-value, scratch-cycle |
| Realistic narrative for workloads | Partial | `docs/RESULTS.md` gives believable examples | Talk track needed stronger packaging, now added in deck/script |
| Trace-like workloads (Zipf, bursty, heavy skew) | Missing | No trace-driven benchmarks yet | Good next benchmark track |
| Hardware-aware metrics | Missing | No cache-counter or latency-percentile pipeline | Only throughput is currently measured |
| Memory/performance tradeoff tables | Missing | No automatic memory report | Current story is speed-first |

### Feedback loops and observability

| Idea | Status | Evidence | Notes |
|---|---|---|---|
| Builder feedback loop | Partial | Benchmarks, docs, results | Lacks commit-to-commit automated comparisons |
| User/learner feedback loop | Missing | No survey or structured feedback artifact | Could be added after the workshop |
| Runtime feedback loop | Missing | No deployed service, tracing, or dashboards | Not part of the current repo |

## 3. What Is Fully Implemented Today

- Core open-addressing map with tombstones, rehashing, and a zero-allocation
  fast path
- Workload-tuned balanced, read-heavy, churn-heavy, and space-efficient
  variants
- Large-value `IndirectHashMap`
- Scratch and scratch-indirect specializations for rebuild-heavy workloads
- Shared adapter layer across STL, Abseil, and PerfMap variants
- Contract tests plus PerfMap-specific invariant tests
- General benchmark suite plus explicit tradeoff benchmarks
- GitHub Actions CI for build, tests, and benchmark smoke coverage
- Workshop deck, notes, generated assets, and a starter folder for attendees

## 4. What Is Still Missing

These are the main things you should **not** imply are already built:

- benchmark regression CI
- historical benchmark result storage and dashboards
- memory accounting
- probe-length instrumentation
- cross-machine benchmark automation
- VS Code or GUI visualizer
- service wrapper / cloud deployment
- advanced probing redesigns like Robin Hood, metadata bytes, or SIMD probing

## 5. Immediate To-Do Order Before Going Live

1. Build the updated deck and make sure the exported `.pptx` opens cleanly.
2. Upload the updated deck plus keep `presentation_script.md` open as your
   backup speaker notes.
3. If you publish the starter repo separately, point attendees at
   `starter/perfmap-10d` as the seed layout.
4. In the talk, keep repeating the same three anchors:
   - real workload
   - measurable evidence
   - honest trade-offs
