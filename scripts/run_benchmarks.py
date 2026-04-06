#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from benchmark_lib import (
    build_report_data,
    compare_runs,
    environment_label,
    render_html_report,
    render_markdown_report,
    utc_now_iso,
    write_json,
)


REPO_ROOT = Path(__file__).resolve().parent.parent
DASHBOARD_ASSET_DIR = REPO_ROOT / "dashboard"
DEFAULT_THRESHOLDS = REPO_ROOT / "bench" / "baselines" / "default" / "thresholds.json"

BENCHMARK_ROW_RE = re.compile(
    r"^(?P<name>\S+)\s+(?P<real>\S+)\s+(?P<cpu>\S+)\s+(?P<iterations>\S+)(?P<counters>.*)$"
)
CMAKE_SET_RE = re.compile(r'^set\((?P<key>[A-Za-z0-9_]+)\s+"(?P<value>.*)"\)$')


@dataclass(frozen=True)
class SuiteConfig:
    name: str
    binary: str
    benchmark_filter: str | None
    min_time_seconds: float
    repetitions: int
    aggregates_only: bool


PRESETS: dict[str, list[SuiteConfig]] = {
    "smoke": [
        SuiteConfig(
            name="perfmap_bench",
            binary="perfmap_bench",
            benchmark_filter=r"^(std_unordered_map|perfmap_balanced|perfmap_scratch_indirect_large_value)/(insert_grow|find_hit|large_value_scratch_cycle)/1024$",
            min_time_seconds=0.001,
            repetitions=1,
            aggregates_only=False,
        ),
        SuiteConfig(
            name="perfmap_trace_bench",
            binary="perfmap_trace_bench",
            benchmark_filter=r"^(std_unordered_map|perfmap_scratch|perfmap_scratch_indirect_large_value)/trace_(request_dedup_enrichment|batch_metadata_enrichment)$",
            min_time_seconds=0.001,
            repetitions=1,
            aggregates_only=False,
        ),
        SuiteConfig(
            name="perfmap_tradeoffs",
            binary="perfmap_tradeoffs",
            benchmark_filter=r"^(BM_StdMap_MissHeavy|BM_PerfMap_MissHeavy)/1024$",
            min_time_seconds=0.001,
            repetitions=1,
            aggregates_only=False,
        ),
    ],
    "full": [
        SuiteConfig(
            name="perfmap_bench",
            binary="perfmap_bench",
            benchmark_filter=r"^(std_unordered_map|absl_flat_hash_map|absl_node_hash_map|perfmap_balanced|perfmap_read_heavy|perfmap_space_efficient|perfmap_indirect_large_value|perfmap_scratch|perfmap_scratch_indirect_large_value)/(insert_reserved|find_hit|find_miss|mixed|large_value_find_hit|large_value_find_miss|scratch_cycle|large_value_scratch_cycle)/(4096|16384|65536)$",
            min_time_seconds=0.01,
            repetitions=5,
            aggregates_only=True,
        ),
        SuiteConfig(
            name="perfmap_trace_bench",
            binary="perfmap_trace_bench",
            benchmark_filter=None,
            min_time_seconds=0.01,
            repetitions=5,
            aggregates_only=True,
        ),
        SuiteConfig(
            name="perfmap_tradeoffs",
            binary="perfmap_tradeoffs",
            benchmark_filter=r"^(BM_StdMap_MissHeavy|BM_PerfMap_MissHeavy|BM_StdMap_LargeValue_Find|BM_PerfMap_LargeValue_Find|BM_StdMap_EraseChurn|BM_PerfMap_EraseChurn)/(4096|16384)$",
            min_time_seconds=0.01,
            repetitions=5,
            aggregates_only=True,
        ),
    ],
}


def list_benchmarks(binary: Path, benchmark_filter: str | None) -> list[str]:
    cmd = [str(binary), "--benchmark_list_tests=true"]
    if benchmark_filter:
        cmd.append(f"--benchmark_filter={benchmark_filter}")
    result = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def parse_cmake_set_file(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.exists():
        return values
    for raw_line in path.read_text().splitlines():
        line = raw_line.strip()
        match = CMAKE_SET_RE.match(line)
        if match:
            values[match.group("key")] = match.group("value")
    return values


def compiler_metadata(build_dir: Path) -> dict[str, Any]:
    compiler_files = sorted(build_dir.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"))
    system_files = sorted(build_dir.glob("CMakeFiles/*/CMakeSystem.cmake"))
    compiler_values = parse_cmake_set_file(compiler_files[0]) if compiler_files else {}
    system_values = parse_cmake_set_file(system_files[0]) if system_files else {}
    compiler_version = compiler_values.get("CMAKE_CXX_COMPILER_VERSION")
    compiler_major = None
    if compiler_version:
        major_token = compiler_version.split(".", 1)[0]
        if major_token.isdigit():
            compiler_major = int(major_token)
    return {
        "os": system_values.get("CMAKE_SYSTEM_NAME") or platform.system(),
        "arch": system_values.get("CMAKE_SYSTEM_PROCESSOR") or platform.machine(),
        "compiler_id": compiler_values.get("CMAKE_CXX_COMPILER_ID"),
        "compiler_version": compiler_version,
        "compiler_version_major": compiler_major,
    }


def collect_environment(build_dir: Path) -> dict[str, Any]:
    environment = compiler_metadata(build_dir)
    environment["label"] = environment_label(environment)
    return environment


def parse_counter_blob(blob: str) -> dict[str, float]:
    counters: dict[str, float] = {}
    for token in blob.strip().split():
        if "=" not in token:
            continue
        key, raw_value = token.split("=", 1)
        raw_value = raw_value.rstrip(",")
        multiplier = 1.0
        if raw_value.endswith("M/s"):
            multiplier = 1_000_000.0
            raw_value = raw_value[:-3]
        elif raw_value.endswith("K/s"):
            multiplier = 1_000.0
            raw_value = raw_value[:-3]
        elif raw_value.endswith("/s"):
            raw_value = raw_value[:-2]
        try:
            counters[key] = float(raw_value) * multiplier
        except ValueError:
            continue
    return counters


def canonical_row_name(name: str) -> str:
    for suffix in ("_mean", "_median", "_stddev", "_cv"):
        if name.endswith(suffix):
            return name[: -len(suffix)]
    return name


def emit_event(events_path: Path, event: dict[str, Any]) -> None:
    with events_path.open("a") as handle:
        handle.write(json.dumps({"ts": utc_now_iso(), **event}) + "\n")


def copy_dashboard_assets(run_dir: Path) -> None:
    dashboard_dir = run_dir / "dashboard"
    dashboard_dir.mkdir(parents=True, exist_ok=True)
    for asset in ("index.html", "app.js", "styles.css"):
        shutil.copy2(DASHBOARD_ASSET_DIR / asset, dashboard_dir / asset)


def update_summary(path: Path, payload: dict[str, Any]) -> None:
    write_json(path, payload)


def run_suite(
    suite: SuiteConfig,
    build_dir: Path,
    run_dir: Path,
    events_path: Path,
    summary: dict[str, Any],
    suite_summary: dict[str, Any],
) -> None:
    binary = build_dir / suite.binary
    json_path = run_dir / f"{suite.name}.json"
    stdout_path = run_dir / f"{suite.name}.stdout.txt"
    stderr_path = run_dir / f"{suite.name}.stderr.txt"
    expected = list_benchmarks(binary, suite.benchmark_filter)
    unique_expected = list(expected)
    completed: list[dict[str, Any]] = []

    cmd = [
        str(binary),
        f"--benchmark_min_time={suite.min_time_seconds}s",
        f"--benchmark_out={json_path}",
        "--benchmark_out_format=json",
    ]
    if suite.benchmark_filter:
        cmd.append(f"--benchmark_filter={suite.benchmark_filter}")
    if suite.repetitions > 1:
        cmd.extend(
            [
                f"--benchmark_repetitions={suite.repetitions}",
                f"--benchmark_report_aggregates_only={'true' if suite.aggregates_only else 'false'}",
            ]
        )

    emit_event(events_path, {"event": "suite_started", "suite": suite.name, "command": cmd})
    suite_summary.update(
        {
            "status": "running",
            "benchmark_total": len(unique_expected),
            "benchmark_completed": 0,
            "current_benchmark": unique_expected[0] if unique_expected else None,
            "artifacts": {
                "json": json_path.name,
                "stdout": stdout_path.name,
                "stderr": stderr_path.name,
            },
        }
    )
    summary["current_stage"] = "benchmarking"
    summary["current_suite"] = suite.name
    summary["current_benchmark"] = suite_summary["current_benchmark"]
    update_summary(run_dir / "summary.json", summary)

    with stdout_path.open("w") as stdout_handle, stderr_path.open("w") as stderr_handle:
        process = subprocess.Popen(
            cmd,
            cwd=REPO_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        assert process.stdout is not None
        assert process.stderr is not None

        for line in process.stdout:
            stdout_handle.write(line)
            match = BENCHMARK_ROW_RE.match(line.strip())
            if not match:
                continue
            name = match.group("name")
            if name == "Benchmark":
                continue
            counters = parse_counter_blob(match.group("counters"))
            canonical_name = canonical_row_name(name)
            should_count = (
                suite.repetitions == 1
                or not suite.aggregates_only
                or name.endswith("_median")
            )
            if should_count:
                completed.append(
                    {
                        "suite": suite.name,
                        "name": canonical_name,
                        "display_name": name,
                        "items_per_second": counters.get("items_per_second"),
                        "bytes_per_live_entry": counters.get("bytes_per_live_entry"),
                        "total_reserved_bytes": counters.get("total_reserved_bytes"),
                    }
                )
                suite_summary["benchmark_completed"] = min(
                    len(completed), suite_summary["benchmark_total"]
                )
                if suite_summary["benchmark_completed"] < suite_summary["benchmark_total"]:
                    suite_summary["current_benchmark"] = unique_expected[
                        suite_summary["benchmark_completed"]
                    ]
                else:
                    suite_summary["current_benchmark"] = None
                summary["recent_completed_benchmarks"] = (
                    summary["recent_completed_benchmarks"] + [completed[-1]]
                )[-20:]
                summary["current_benchmark"] = suite_summary["current_benchmark"]
                update_summary(run_dir / "summary.json", summary)
                emit_event(
                    events_path,
                    {
                        "event": "benchmark_completed",
                        "suite": suite.name,
                        "benchmark": canonical_name,
                        "items_per_second": counters.get("items_per_second"),
                    },
                )

        for line in process.stderr:
            stderr_handle.write(line)

        returncode = process.wait()
    if returncode != 0:
        suite_summary["status"] = "failed"
        emit_event(
            events_path,
            {"event": "suite_failed", "suite": suite.name, "returncode": returncode},
        )
        raise subprocess.CalledProcessError(returncode, cmd)

    suite_summary["status"] = "passed"
    summary["current_benchmark"] = None
    update_summary(run_dir / "summary.json", summary)
    emit_event(events_path, {"event": "suite_finished", "suite": suite.name})


def run_pipeline(args: argparse.Namespace) -> int:
    run_id = args.run_name or datetime.now().strftime("%Y%m%d-%H%M%S")
    build_dir = (REPO_ROOT / args.build_dir).resolve()
    runs_root = (REPO_ROOT / args.output_root).resolve()
    run_dir = runs_root / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    copy_dashboard_assets(run_dir)
    events_path = run_dir / "events.jsonl"
    environment = collect_environment(build_dir)

    suites = PRESETS[args.preset]
    manifest = {
        "run_id": run_id,
        "preset": args.preset,
        "generated_at": utc_now_iso(),
        "build_dir": str(build_dir),
        "environment": environment,
        "suites": [],
    }
    summary = {
        "run_id": run_id,
        "preset": args.preset,
        "status": "running",
        "generated_at": utc_now_iso(),
        "current_stage": "initializing",
        "current_suite": None,
        "current_benchmark": None,
        "environment": environment,
        "recent_completed_benchmarks": [],
        "suites": [],
        "artifacts": {
            "manifest": "manifest.json",
            "events": "events.jsonl",
            "dashboard": "dashboard/index.html",
        },
    }

    for suite in suites:
        suite_summary = {
            "name": suite.name,
            "binary": suite.binary,
            "filter": suite.benchmark_filter,
            "repetitions": suite.repetitions,
            "aggregates_only": suite.aggregates_only,
            "status": "pending",
        }
        summary["suites"].append(suite_summary)
        manifest["suites"].append(suite_summary)
    update_summary(run_dir / "summary.json", summary)
    write_json(run_dir / "manifest.json", manifest)
    emit_event(
        events_path,
        {
            "event": "run_started",
            "run_id": run_id,
            "preset": args.preset,
            "environment": environment,
        },
    )

    exit_code = 0
    try:
        for suite_cfg, suite_summary in zip(suites, summary["suites"], strict=True):
            run_suite(suite_cfg, build_dir, run_dir, events_path, summary, suite_summary)
            write_json(run_dir / "manifest.json", manifest)
        summary["current_stage"] = "comparing"
        summary["current_suite"] = None
        update_summary(run_dir / "summary.json", summary)
        write_json(run_dir / "manifest.json", manifest)
        comparison_path = None
        if not args.skip_compare and Path(args.baseline_dir, "manifest.json").exists():
            emit_event(events_path, {"event": "compare_started"})
            comparison = compare_runs(
                run_dir / "manifest.json",
                Path(args.baseline_dir) / "manifest.json",
                Path(args.thresholds),
            )
            comparison_path = run_dir / "comparison.json"
            write_json(comparison_path, comparison)
            summary["comparison_summary"] = comparison["summary"]
            emit_event(events_path, {"event": "compare_finished", "summary": comparison["summary"]})
            if args.enforce_regressions and comparison["summary"]["failures"]:
                exit_code = 1
        else:
            emit_event(events_path, {"event": "compare_skipped"})

        summary["current_stage"] = "reporting"
        report_data = build_report_data(run_dir / "manifest.json", comparison_path)
        write_json(run_dir / "report_data.json", report_data)
        (run_dir / "report.md").write_text(render_markdown_report(report_data))
        (run_dir / "report.html").write_text(render_html_report(report_data))
        summary["artifacts"].update(
            {
                "summary": "summary.json",
                "comparison": "comparison.json" if comparison_path else None,
                "report_markdown": "report.md",
                "report_html": "report.html",
                "report_data": "report_data.json",
            }
        )
        emit_event(events_path, {"event": "report_generated"})
        summary["status"] = "passed" if exit_code == 0 else "failed"
    except subprocess.CalledProcessError as exc:
        summary["status"] = "failed"
        summary["failure"] = {"command": exc.cmd, "returncode": exc.returncode}
        emit_event(events_path, {"event": "run_failed", "returncode": exc.returncode})
        exit_code = exc.returncode or 1
    finally:
        summary["current_stage"] = "complete"
        summary["current_suite"] = None
        summary["current_benchmark"] = None
        summary["finished_at"] = utc_now_iso()
        write_json(run_dir / "manifest.json", manifest)
        update_summary(run_dir / "summary.json", summary)
        emit_event(events_path, {"event": "run_finished", "status": summary["status"]})

    print(f"Run directory: {run_dir}")
    print(f"Dashboard: {run_dir / 'dashboard' / 'index.html'}")
    print(f"Report: {run_dir / 'report.html'}")
    return exit_code


def main() -> int:
    parser = argparse.ArgumentParser(description="Run PerfMap benchmark suites with structured observability, comparison, and reports.")
    parser.add_argument("--preset", choices=sorted(PRESETS), default="smoke")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--output-root", default="results/runs")
    parser.add_argument("--run-name", default=None)
    parser.add_argument("--baseline-dir", default=str(REPO_ROOT / "bench" / "baselines" / "default"))
    parser.add_argument("--thresholds", default=str(DEFAULT_THRESHOLDS))
    parser.add_argument("--skip-compare", action="store_true")
    parser.add_argument("--enforce-regressions", action="store_true")
    args = parser.parse_args()
    return run_pipeline(args)


if __name__ == "__main__":
    raise SystemExit(main())
