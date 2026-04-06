#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from benchmark_lib import load_manifest, utc_now_iso, write_json


def main() -> int:
    parser = argparse.ArgumentParser(description="Promote a benchmark run directory into the canonical baseline store.")
    parser.add_argument("--run-dir", required=True)
    parser.add_argument("--baseline-dir", required=True)
    args = parser.parse_args()

    run_dir = Path(args.run_dir)
    baseline_dir = Path(args.baseline_dir)
    baseline_dir.mkdir(parents=True, exist_ok=True)

    manifest = load_manifest(run_dir / "manifest.json")
    for suite in manifest.get("suites", []):
      artifacts = suite.get("artifacts", {})
      json_artifact = artifacts.get("json")
      if not json_artifact:
          continue
      shutil.copy2(run_dir / json_artifact, baseline_dir / Path(json_artifact).name)

    baseline_manifest = {
        "run_id": manifest.get("run_id"),
        "source_run_dir": str(run_dir),
        "updated_at": utc_now_iso(),
        "preset": manifest.get("preset"),
        "suites": [],
    }
    for suite in manifest.get("suites", []):
        artifact_name = Path(suite["artifacts"]["json"]).name
        baseline_manifest["suites"].append(
            {
                "name": suite["name"],
                "artifacts": {"json": artifact_name},
                "filter": suite.get("filter"),
            }
        )

    write_json(baseline_dir / "manifest.json", baseline_manifest)
    print(f"Updated baseline at {baseline_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
