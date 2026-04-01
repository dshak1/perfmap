# PerfMap Workshop Script

## Slide 1

Set the tone immediately. This session is about building a small systems
project that becomes impressive because it is measurable and explainable,
not because it has a huge feature list.

## Slide 2

Start engaging the room right away. Ask who has written C++, who knows
what a hash map is under the hood, and what makes a project feel strong
versus tutorial-tier. Use the answers to seed the rest of the talk.

## Slide 3

Define the five ideas you want people to remember: real problem,
iteration, measurable evidence, intentional AI use, and enough depth to
defend the hard parts in an interview.

## Slide 4

Show the opposite. A project feels weak when it has familiar surface
area but no constraints, no numbers, no real trade-off, and no sign that
the builder actually understood the design.

## Slide 5

Give students practical places to find better ideas: classes,
professors, research, clubs, hackathons, startups, and repeated pain
points they actually encounter. The theme is constraints. Constraints
produce better projects.

## Slide 6

Explain why PerfMap is a good workshop repo. It is narrow enough to
teach in one room, but deep enough to talk about memory layout,
benchmarking, API design, and trade-offs.

## Slide 7

Define the project cleanly: open addressing, linear probing, tombstones,
rehashing, power-of-two capacity, and a `FindPtr()` fast path. Keep the
message simple: the real lesson is memory behavior.

## Slide 8

Use the diagram to contrast contiguous storage with pointer chasing.
Pause and ask why a flat layout might win even when the asymptotic
complexity looks similar on paper.

## Slide 9

Show that the repo is more than one container. Different policies and
specializations exist because different workloads want different
trade-offs. This is where the project starts looking like a systems lab
instead of a toy implementation.

## Slide 10

Explain benchmark fairness in plain English. Same keys, same setup
rules, same adapter contract, and explicit "where we lose" cases kept in
the repo. This is the difference between a benchmark story and benchmark
theater.

## Slide 11

Use the current numbers. The strongest win is the large-value
scratch/rebuild niche, where `ScratchIndirectHashMap` is about `7.3x`
faster than `std::unordered_map` in the March 31, 2026 spot check. Then
say the broad large-value lookup case still loses to `absl::flat_hash_map`.

## Slide 12

Tell the real-world story clearly: per-batch document enrichment cache.
Each batch carries lots of large metadata blobs, you build a temporary
lookup structure, hammer it for that batch, then clear and rebuild.
That makes the winning specialization feel believable.

## Slide 13

Use Google-style C++ as a credibility layer, not a gimmick. The point is
explicit error handling, readability, consistent structure, and
correctness-first engineering.

## Slide 14

Walk the room through the live demo sequence. Build, test, benchmark,
then inspect the code. Ask why tombstones exist and why `FindPtr()`
exists before showing the answers.

## Slide 15

Sell the three extension paths depending on audience interest:
cloud/DevOps, low-level ML systems, and advanced performance C++. This
helps students see how one repo can branch into different career stories.

## Slide 16

Tell them there is a starter repo layout in `starter/perfmap-10d`. The
point is not to hand them the finished answer. The point is to let them
build the core invariants themselves and compare against the completed
repo later.

## Slide 17

Close with the mini challenge and guest questions. Reinforce the meta
lesson: the best student projects are the ones that are measurable,
defensible, and honest about trade-offs.
