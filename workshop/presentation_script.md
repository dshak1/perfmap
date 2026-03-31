# PerfMap Workshop Script

## Slide 1

This workshop is centered on `perfmap`, a custom hash map project that is technical enough to stand out but still teachable in a room of students. The goal is not just to build something that compiles. The goal is to build something measurable.

## Slide 2

Frame the market honestly: simple apps are easier than ever to generate, so the differentiator is no longer just shipping a repo. It is whether you can explain trade-offs, back claims with benchmark data, and clearly show why your design choices mattered.

## Slide 3

Preview the structure so the room knows this is a real workshop, not random live coding. You build, validate, measure, extend, then transition into interview and panel discussion.

## Slide 4

Tell them exactly what they get: a better resume bullet, better technical vocabulary, and a project template they can grow into something personal.

## Slide 5

Define the project cleanly. PerfMap is an open-addressing hash map with linear probing, tombstones, rehashing, and a zero-allocation lookup path for benchmarks. The higher-level lesson is that memory layout can dominate algorithmic elegance.

## Slide 6

Use the diagram to explain locality versus pointer chasing. Make it clear that the story is not “we beat STL because STL is bad.” The story is “different data-structure layouts produce different performance profiles.”

## Slide 7

Walk through the benchmark wins and the one important loss. The miss-path regression is actually what makes the project stronger, because now students have a real trade-off to talk about instead of fake perfection.

## Slide 8

Tie the repo back to Google-style C++. Readability, consistency, explicit error handling, and tooling-friendly code matter more than flexing obscure language features.

## Slide 9

Run the repo. Show that the project has tests, benchmarks, and a clear workflow. Ask the room why `FindPtr()` exists and why tombstones matter before revealing the answers.

## Slide 10

Sell the AWS/DevOps path as “productionizing the benchmark story.” The key idea is reproducibility and regression detection, not just spinning up cloud infra for its own sake.

## Slide 11

Sell the AI/ML path as systems-first. Feature serving, retrieval, caching, and inference metadata all rely on fast lookups and stable latency. This makes the repo relevant to ML students without turning it into a shallow “AI wrapper.”

## Slide 12

Sell the advanced C++ path to the strongest builders in the room. Even one serious extension like Robin Hood hashing or SIMD probing can turn the project into a standout systems repo.

## Slide 13

If you run the post-workshop challenge, keep it grounded. The best project is not the one with the most features. It is the one with the cleanest hypothesis, strongest evidence, and most honest explanation.

## Slide 14

Use the guest questions to bridge from the project to careers. The deck should end on the idea that measurable engineering work is the real differentiator.
