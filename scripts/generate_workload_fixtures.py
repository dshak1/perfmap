#!/usr/bin/env python3

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
WORKLOAD_DIR = REPO_ROOT / "bench" / "workloads"


@dataclass(frozen=True)
class Event:
    op: str
    key: int
    value_seed: int
    expected_hit: int


def write_fixture(
    name: str,
    description: str,
    story: str,
    value_type: str,
    peak_live_entries: int,
    events: list[Event],
) -> None:
    path = WORKLOAD_DIR / f"{name}.csv"
    with path.open("w", newline="") as handle:
        handle.write(f"# name: {name}\n")
        handle.write(f"# description: {description}\n")
        handle.write(f"# story: {story}\n")
        handle.write(f"# value_type: {value_type}\n")
        handle.write(f"# peak_live_entries: {peak_live_entries}\n")
        writer = csv.writer(handle)
        writer.writerow(["op", "key", "value_seed", "expected_hit"])
        for event in events:
          writer.writerow([event.op, event.key, event.value_seed, event.expected_hit])


def request_dedup_enrichment() -> tuple[int, list[Event]]:
    events: list[Event] = []
    peak = 0
    request_count = 28
    request_size = 40
    shared_hot_keys = [1000 + i for i in range(10)]

    for request_id in range(request_count):
        events.append(Event("clear", 0, 0, 0))
        seen: set[int] = set()
        request_keys: list[int] = []
        for i in range(request_size):
            if i % 5 == 0:
                key = shared_hot_keys[(request_id + i) % len(shared_hot_keys)]
            else:
                key = request_id * 500 + i
            request_keys.append(key)

        request_keys.extend(shared_hot_keys[:6])
        request_keys.extend(shared_hot_keys[:6])

        for index, key in enumerate(request_keys):
            expected_hit = 1 if key in seen else 0
            events.append(Event("find", key, 0, expected_hit))
            if key not in seen:
                seen.add(key)
                events.append(Event("insert", key, request_id * 1000 + index, 0))
        peak = max(peak, len(seen))
    return peak, events


def graph_traversal_scratch() -> tuple[int, list[Event]]:
    events: list[Event] = []
    peak = 0
    traversal_count = 18
    frontier_width = 22

    for traversal in range(traversal_count):
        events.append(Event("clear", 0, 0, 0))
        seen: set[int] = set()
        frontier = [traversal * 4096 + i for i in range(frontier_width)]
        for depth in range(4):
            next_frontier: list[int] = []
            for node in frontier:
                for branch in range(3):
                    neighbor = node + depth * 97 + branch * 17
                    expected_hit = 1 if neighbor in seen else 0
                    events.append(Event("find", neighbor, 0, expected_hit))
                    if neighbor not in seen:
                        seen.add(neighbor)
                        events.append(
                            Event("insert", neighbor, traversal * 10_000 + neighbor, 0)
                        )
                        next_frontier.append(neighbor)
                hot_back_edge = traversal * 4096 + (node % frontier_width)
                events.append(
                    Event("find", hot_back_edge, 0, 1 if hot_back_edge in seen else 0)
                )
            frontier = next_frontier[:frontier_width]
        peak = max(peak, len(seen))
    return peak, events


def hotset_phase_shift() -> tuple[int, list[Event]]:
    events: list[Event] = []
    initial_keys = [200_000 + i for i in range(512)]
    hot_keys = initial_keys[:32]
    warm_keys = initial_keys[32:160]
    cold_keys = initial_keys[160:]

    for i, key in enumerate(initial_keys):
        events.append(Event("insert", key, 50_000 + i, 0))

    for round_id in range(14):
        for i in range(220):
            key = hot_keys[(round_id * 7 + i) % len(hot_keys)]
            events.append(Event("find", key, 0, 1))
        for i in range(70):
            key = warm_keys[(round_id * 13 + i) % len(warm_keys)]
            events.append(Event("find", key, 0, 1))
        for i in range(36):
            key = 900_000 + round_id * 100 + i
            events.append(Event("find", key, 0, 0))
        if round_id % 3 == 0:
            for i in range(18):
                key = cold_keys[(round_id * 11 + i) % len(cold_keys)]
                events.append(Event("find", key, 0, 1))
    return len(initial_keys), events


def batch_metadata_enrichment() -> tuple[int, list[Event]]:
    events: list[Event] = []
    peak = 0
    batch_count = 14
    batch_size = 96

    for batch_id in range(batch_count):
        events.append(Event("clear", 0, 0, 0))
        inserted: list[int] = []
        for row in range(batch_size):
            key = batch_id * 10_000 + row
            inserted.append(key)
            events.append(Event("insert", key, batch_id * 1_000_000 + row, 0))

        hot_keys = inserted[:16]
        for round_id in range(10):
            for i in range(48):
                key = hot_keys[(round_id * 5 + i) % len(hot_keys)]
                events.append(Event("find", key, 0, 1))
            for i in range(24):
                key = inserted[(round_id * 17 + i) % len(inserted)]
                events.append(Event("find", key, 0, 1))
            for i in range(12):
                key = batch_id * 10_000 + batch_size + round_id * 16 + i
                events.append(Event("find", key, 0, 0))
        peak = max(peak, len(inserted))
    return peak, events


def main() -> None:
    WORKLOAD_DIR.mkdir(parents=True, exist_ok=True)

    peak, events = request_dedup_enrichment()
    write_fixture(
        "request_dedup_enrichment",
        "Request-scoped dedup and enrichment trace with repeated IDs inside each request.",
        "Temporary per-request cache used to avoid repeated enrichment or parsing work.",
        "int",
        peak,
        events,
    )

    peak, events = graph_traversal_scratch()
    write_fixture(
        "graph_traversal_scratch",
        "Traversal-local visited-state trace with repeated neighbor checks and fresh traversal resets.",
        "Visited-map scratch state for BFS/DFS-style traversals where reset cost matters.",
        "int",
        peak,
        events,
    )

    peak, events = hotset_phase_shift()
    write_fixture(
        "hotset_phase_shift",
        "Long-lived cache trace with hot-key skew, warm lookups, and bursty miss phases.",
        "Hot-key lookup cache with phase shifts between skewed hits and miss bursts.",
        "int",
        peak,
        events,
    )

    peak, events = batch_metadata_enrichment()
    write_fixture(
        "batch_metadata_enrichment",
        "Batch-local metadata cache rebuilt per batch, then queried heavily with a hot subset.",
        "Per-batch enrichment cache for large parsed objects or metadata blobs.",
        "large_value",
        peak,
        events,
    )


if __name__ == "__main__":
    main()
