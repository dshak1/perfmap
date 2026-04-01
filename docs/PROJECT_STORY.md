# PerfMap Project Story

## How I Came Across The Idea

PerfMap came from a very specific pattern I kept seeing when I looked at
C++ systems and performance-oriented software roles:

- memory layout mattered
- latency and throughput mattered
- benchmark rigor mattered
- "I built something" was not enough by itself

I wanted a project that matched that bar. A hash map was a good target
because it is familiar enough to matter, but deep enough to force real
systems reasoning about cache locality, probe behavior, deletion
semantics, API design, and measurement.

The second part of the idea came from reading more about cache-aware
hashing and workload-sensitive systems design. The interesting question
stopped being:

> "Can I build a hash map?"

and became:

> "Can I build one that wins on a believable workload, explain why it
> wins, and also show where it loses?"

That is what turned the repo from a generic data-structure exercise into
a performance engineering lab.

## Problems I Was Trying To Fix

The repo is really trying to answer three problems:

1. **Student projects often stop at implementation and never get to
   measurement.**
   PerfMap fixes that by making tests and benchmarks part of the core
   repo, not an afterthought.

2. **A lot of "fast" claims are too generic to defend.**
   PerfMap narrows the story to specific workload shapes instead of
   pretending one container beats every baseline everywhere.

3. **Most hash map discussions stay too abstract.**
   PerfMap makes the important trade-offs visible:
   - contiguous storage vs pointer chasing
   - tombstones vs broken probe chains
   - readability vs hot-path APIs
   - generality vs specialization

## What A Workload Means Here

A workload is not just "we used a hash map."

It is the actual usage pattern over time:

- how often the map gets built
- whether lookups are mostly hits or misses
- whether entries churn constantly
- whether values are tiny or large
- whether the map is long-lived or temporary
- whether `clear` happens rarely or constantly

That matters because the same data structure can look excellent on one
workload and mediocre on another.

## Three Workshop-Friendly Workloads

These are the three easiest real-world stories to explain in a workshop.

### 1. Request-scoped dedup / enrichment

**Technical shape**

- one request arrives
- many repeated IDs or keys appear inside it
- you build a temporary lookup structure for just that request
- you deduplicate repeated work
- then the request finishes and the map is discarded

**Why it is useful**

This is a natural scratch/rebuild workload. The map does not need to
live forever. It needs to be cheap to rebuild and cheap to query while
the request is active.

**Simple way to say it**

"Inside one request, I do not want to compute or fetch the same thing
over and over again."

### 2. Per-batch document or metadata enrichment cache

**Technical shape**

- a batch of records arrives
- records point to large metadata blobs or parsed objects
- you build a temporary cache for that batch
- you query it heavily during processing
- then you clear it and rebuild for the next batch

**Why it matters**

This is the strongest niche in the current repo because it combines the
two biggest structural wins:

- O(1) clear for repeated rebuild cycles
- indirect storage so large payloads do not bloat the probe path

**Simple way to say it**

"I process one batch, build a temporary cache for that batch, use it
hard, then throw it away and do the next batch."

### 3. Graph traversal visited map

**Technical shape**

- each BFS or DFS query needs temporary visited state
- nodes are marked seen during one traversal
- the traversal ends
- visited state gets reset for the next query

**Why it is useful**

This is another clean scratch/rebuild example. It is intuitive, easy to
visualize, and good for explaining why reset cost matters.

**Simple way to say it**

"Every graph query needs a fresh temporary map, so reset cost becomes
part of the performance story."

## Why The Repo Narrowed To The Batch-Cache Niche

The repo started broad, but the most defensible benchmark story turned
out to be the large-value scratch/rebuild family. That is why the
strongest specialization today is `ScratchIndirectHashMap`.

It is the best combination of:

- believable use case
- measurable structural advantage
- honest comparison against strong baselines

That is also why the repo keeps its negative results. Broad large-value
lookup still loses to `absl::flat_hash_map`, and that makes the project
stronger because the specialization now looks intentional instead of
convenient.

## Extension Plan

The main follow-up ideas are:

- GitHub Actions CI with build, test, and benchmark smoke checks
- benchmark regression tracking over time
- memory and probe-length instrumentation
- more trace-like workloads such as Zipfian or bursty patterns
- advanced probing redesigns like Robin Hood hashing or metadata bytes
- cloud or service wrappers only if they support the measurement story

The rule is simple: extensions should make the repo more measurable,
more credible, or more useful. They should not dilute the core technical
story.
