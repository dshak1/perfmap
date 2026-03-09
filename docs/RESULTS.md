# PerfMap Benchmark Results

**Machine:** _e.g. MacBook Pro M2, 16GB RAM_
**Compiler:** _e.g. Apple Clang 15.0, -O2 -march=native_
**Date:** _March 2026_

---

## Insert Performance

| Size (elements) | std::unordered_map (ns/op) | PerfMap (ns/op) | Speedup |
|-----------------|---------------------------|-----------------|---------|
| 1,024           |                           |                 |         |
| 4,096           |                           |                 |         |
| 16,384          |                           |                 |         |
| 65,536          |                           |                 |         |
| 262,144         |                           |                 |         |

## Lookup Performance (Hit)

| Size (elements) | std::unordered_map (ns/op) | PerfMap (ns/op) | Speedup |
|-----------------|---------------------------|-----------------|---------|
| 1,024           |                           |                 |         |
| 4,096           |                           |                 |         |
| 16,384          |                           |                 |         |
| 65,536          |                           |                 |         |
| 262,144         |                           |                 |         |

## Lookup Performance (Miss)

| Size (elements) | std::unordered_map (ns/op) | PerfMap (ns/op) | Speedup |
|-----------------|---------------------------|-----------------|---------|
| 1,024           |                           |                 |         |
| 4,096           |                           |                 |         |
| 16,384          |                           |                 |         |
| 65,536          |                           |                 |         |
| 262,144         |                           |                 |         |

## Mixed Workload (50% insert, 40% lookup, 10% erase)

| Size (elements) | std::unordered_map (ns/op) | PerfMap (ns/op) | Speedup |
|-----------------|---------------------------|-----------------|---------|
| 1,024           |                           |                 |         |
| 4,096           |                           |                 |         |
| 16,384          |                           |                 |         |
| 65,536          |                           |                 |         |
| 131,072         |                           |                 |         |

## Analysis

_TODO: Fill in after running benchmarks. Key questions to answer:_

1. **Where does PerfMap win?** At what sizes and for which operations?
2. **Where does it lose?** And why? (hint: think about rehash cost)
3. **Cache effect:** Do you see a cliff at any size? (L1 → L2 → L3 transitions)
4. **What would you optimize next?** (Robin Hood hashing? SIMD probing? Better hash?)

## Raw Output

_Paste the raw console output from `./perfmap_bench` below:_

```
(paste here)
```
