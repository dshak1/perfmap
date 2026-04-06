#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from benchmark_lib import compare_runs, write_json


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare benchmark run to a stored baseline.")
    parser.add_argument("--run-dir", required=True, help="Run directory containing manifest.json")
    parser.add_argument(
        "--baseline-dir",
        required=True,
        help="Baseline directory containing manifest.json and suite JSON outputs",
    )
    parser.add_argument("--thresholds", required=True, help="Threshold config JSON")
    parser.add_argument("--output", required=True, help="Comparison output JSON path")
    args = parser.parse_args()

    run_dir = Path(args.run_dir)
    baseline_dir = Path(args.baseline_dir)
    payload = compare_runs(
        run_dir / "manifest.json",
        baseline_dir / "manifest.json",
        Path(args.thresholds),
    )
    write_json(Path(args.output), payload)
    summary = payload["summary"]
    print(
        "checked={checked} warnings={warnings} failures={failures} missing_baseline={missing} baseline_compatible={compatible}".format(
            checked=summary["checked"],
            warnings=summary["warnings"],
            failures=summary["failures"],
            missing=summary["missing_baseline"],
            compatible=summary.get("baseline_compatible", True),
        )
    )
    if not summary.get("baseline_compatible", True):
        print(f"baseline_skip_reason={summary.get('baseline_skip_reason')}")
    return 1 if summary["failures"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
