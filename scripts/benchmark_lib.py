#!/usr/bin/env python3

from __future__ import annotations

import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


STANDARD_BENCHMARK_FIELDS = {
    "name",
    "family_index",
    "per_family_instance_index",
    "run_name",
    "run_type",
    "repetitions",
    "repetition_index",
    "threads",
    "iterations",
    "real_time",
    "cpu_time",
    "time_unit",
    "aggregate_name",
    "aggregate_unit",
}

AGGREGATE_SUFFIXES = ("_mean", "_median", "_stddev", "_cv")
ENVIRONMENT_COMPATIBILITY_KEYS = ("os", "arch", "compiler_id", "compiler_version_major")


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text())


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")


def load_manifest(path: Path) -> dict[str, Any]:
    manifest = read_json(path)
    manifest["manifest_path"] = str(path)
    return manifest


def select_display_entries(entries: list[dict[str, Any]]) -> list[dict[str, Any]]:
    medians = [entry for entry in entries if entry.get("aggregate_name") == "median"]
    if medians:
        return medians
    return [
        entry
        for entry in entries
        if entry.get("run_type") == "iteration"
        and entry.get("repetition_index", 0) == 0
    ]


def normalize_entry(entry: dict[str, Any], suite_name: str) -> dict[str, Any]:
    counters = {
        key: value
        for key, value in entry.items()
        if key not in STANDARD_BENCHMARK_FIELDS
    }
    benchmark_name = entry.get("run_name", entry["name"])
    return {
        "suite": suite_name,
        "name": benchmark_name,
        "display_name": entry["name"],
        "aggregate_name": entry.get("aggregate_name", "iteration"),
        "items_per_second": entry.get("items_per_second"),
        "cpu_time_ns": entry.get("cpu_time"),
        "real_time_ns": entry.get("real_time"),
        "time_unit": entry.get("time_unit", "ns"),
        "iterations": entry.get("iterations", 0),
        "counters": counters,
    }


def load_suite_rows(json_path: Path, suite_name: str) -> list[dict[str, Any]]:
    payload = read_json(json_path)
    entries = payload.get("benchmarks", [])
    return [normalize_entry(entry, suite_name) for entry in select_display_entries(entries)]


def load_run_rows(manifest: dict[str, Any], root: Path) -> dict[str, list[dict[str, Any]]]:
    rows_by_suite: dict[str, list[dict[str, Any]]] = {}
    for suite in manifest.get("suites", []):
        json_path = root / suite["artifacts"]["json"]
        rows_by_suite[suite["name"]] = load_suite_rows(json_path, suite["name"])
    return rows_by_suite


def load_thresholds(path: Path) -> dict[str, Any]:
    return read_json(path)


def environment_label(environment: dict[str, Any] | None) -> str:
    if not environment:
        return "unknown environment"
    os_name = environment.get("os", "unknown-os")
    arch = environment.get("arch", "unknown-arch")
    compiler = environment.get("compiler_id")
    compiler_version = environment.get("compiler_version_major") or environment.get(
        "compiler_version"
    )
    label = f"{os_name}/{arch}"
    if compiler:
        label += f" | {compiler}"
        if compiler_version:
            label += f" {compiler_version}"
    return label


def compare_environments(
    current_environment: dict[str, Any] | None,
    baseline_environment: dict[str, Any] | None,
) -> dict[str, Any]:
    if not current_environment:
        return {
            "compatible": False,
            "reason": "current run manifest is missing environment metadata",
            "current": current_environment or {},
            "baseline": baseline_environment or {},
            "mismatches": [],
        }
    if not baseline_environment:
        return {
            "compatible": False,
            "reason": "baseline manifest is missing environment metadata",
            "current": current_environment,
            "baseline": baseline_environment or {},
            "mismatches": [],
        }

    mismatches: list[dict[str, Any]] = []
    for key in ENVIRONMENT_COMPATIBILITY_KEYS:
        current_value = current_environment.get(key)
        baseline_value = baseline_environment.get(key)
        if current_value is None or baseline_value is None:
            continue
        if current_value != baseline_value:
            mismatches.append(
                {
                    "field": key,
                    "current": current_value,
                    "baseline": baseline_value,
                }
            )

    if mismatches:
        mismatch_summary = ", ".join(
            f"{entry['field']}: {entry['baseline']} -> {entry['current']}"
            for entry in mismatches
        )
        return {
            "compatible": False,
            "reason": f"incompatible runner/compiler baseline ({mismatch_summary})",
            "current": current_environment,
            "baseline": baseline_environment,
            "mismatches": mismatches,
        }

    return {
        "compatible": True,
        "reason": None,
        "current": current_environment,
        "baseline": baseline_environment,
        "mismatches": [],
    }


def thresholds_for_benchmark(name: str, thresholds: dict[str, Any]) -> dict[str, Any]:
    default = thresholds.get(
        "default", {"warn_pct": 5.0, "fail_pct": 12.0, "enforce": False}
    )
    for entry in thresholds.get("protected_benchmarks", []):
        if re.search(entry["pattern"], name):
            merged = dict(default)
            merged.update(entry)
            return merged
    return default


def relative_delta_pct(current: float | None, baseline: float | None) -> float | None:
    if current is None or baseline in (None, 0):
        return None
    return ((current - baseline) / baseline) * 100.0


def canonical_output_name(name: str) -> str:
    for suffix in AGGREGATE_SUFFIXES:
        if name.endswith(suffix):
            return name[: -len(suffix)]
    return name


def load_rows_from_manifest_path(manifest_path: Path) -> dict[str, list[dict[str, Any]]]:
    manifest = load_manifest(manifest_path)
    return load_run_rows(manifest, manifest_path.parent)


def compare_runs(
    current_manifest_path: Path,
    baseline_manifest_path: Path,
    thresholds_path: Path,
) -> dict[str, Any]:
    current_manifest = load_manifest(current_manifest_path)
    baseline_manifest = load_manifest(baseline_manifest_path)
    current_root = current_manifest_path.parent
    baseline_root = baseline_manifest_path.parent
    current_rows = load_run_rows(current_manifest, current_root)
    baseline_rows = load_run_rows(baseline_manifest, baseline_root)
    thresholds = load_thresholds(thresholds_path)
    compatibility = compare_environments(
        current_manifest.get("environment"),
        baseline_manifest.get("environment"),
    )

    comparisons: list[dict[str, Any]] = []
    checked = 0
    warns = 0
    fails = 0
    missing_baseline = 0
    report_only = 0

    if not compatibility["compatible"]:
        return {
            "generated_at": utc_now_iso(),
            "current_manifest": str(current_manifest_path),
            "baseline_manifest": str(baseline_manifest_path),
            "thresholds": str(thresholds_path),
            "environment_compatibility": compatibility,
            "summary": {
                "checked": 0,
                "warnings": 0,
                "failures": 0,
                "missing_baseline": 0,
                "report_only_regressions": 0,
                "baseline_compatible": False,
                "baseline_skip_reason": compatibility["reason"],
                "current_environment_label": environment_label(
                    compatibility["current"]
                ),
                "baseline_environment_label": environment_label(
                    compatibility["baseline"]
                ),
            },
            "comparisons": [],
            "top_regressions": [],
            "top_improvements": [],
        }

    for suite_name, rows in current_rows.items():
        baseline_index = {
            row["name"]: row for row in baseline_rows.get(suite_name, [])
        }
        for row in rows:
            current_throughput = row.get("items_per_second")
            baseline_row = baseline_index.get(row["name"])
            if baseline_row is None:
                missing_baseline += 1
                comparisons.append(
                    {
                        "suite": suite_name,
                        "name": row["name"],
                        "status": "missing_baseline",
                        "current_items_per_second": current_throughput,
                        "baseline_items_per_second": None,
                    }
                )
                continue

            checked += 1
            baseline_throughput = baseline_row.get("items_per_second")
            delta_pct = relative_delta_pct(current_throughput, baseline_throughput)
            memory_delta_pct = relative_delta_pct(
                row["counters"].get("bytes_per_live_entry"),
                baseline_row["counters"].get("bytes_per_live_entry"),
            )
            total_reserved_delta_pct = relative_delta_pct(
                row["counters"].get("total_reserved_bytes"),
                baseline_row["counters"].get("total_reserved_bytes"),
            )
            threshold = thresholds_for_benchmark(row["name"], thresholds)

            status = "pass"
            if delta_pct is not None and delta_pct < 0:
                regression_pct = abs(delta_pct)
                if threshold.get("enforce", False):
                    if regression_pct >= threshold["fail_pct"]:
                        status = "fail"
                        fails += 1
                    elif regression_pct >= threshold["warn_pct"]:
                        status = "warn"
                        warns += 1
                else:
                    status = "report" if regression_pct >= threshold["warn_pct"] else "pass"
                    if status == "report":
                        report_only += 1

            comparisons.append(
                {
                    "suite": suite_name,
                    "name": row["name"],
                    "status": status,
                    "thresholds": {
                        "warn_pct": threshold["warn_pct"],
                        "fail_pct": threshold["fail_pct"],
                    },
                    "current_items_per_second": current_throughput,
                    "baseline_items_per_second": baseline_throughput,
                    "items_per_second_delta_pct": delta_pct,
                    "current_bytes_per_live_entry": row["counters"].get(
                        "bytes_per_live_entry"
                    ),
                    "baseline_bytes_per_live_entry": baseline_row["counters"].get(
                        "bytes_per_live_entry"
                    ),
                    "bytes_per_live_entry_delta_pct": memory_delta_pct,
                    "current_total_reserved_bytes": row["counters"].get(
                        "total_reserved_bytes"
                    ),
                    "baseline_total_reserved_bytes": baseline_row["counters"].get(
                        "total_reserved_bytes"
                    ),
                    "total_reserved_bytes_delta_pct": total_reserved_delta_pct,
                }
            )

    regressions = sorted(
        [
            item
            for item in comparisons
            if item.get("items_per_second_delta_pct") is not None
            and item["items_per_second_delta_pct"] < 0
        ],
        key=lambda item: item["items_per_second_delta_pct"],
    )
    improvements = sorted(
        [
            item
            for item in comparisons
            if item.get("items_per_second_delta_pct") is not None
            and item["items_per_second_delta_pct"] > 0
        ],
        key=lambda item: item["items_per_second_delta_pct"],
        reverse=True,
    )

    return {
        "generated_at": utc_now_iso(),
        "current_manifest": str(current_manifest_path),
        "baseline_manifest": str(baseline_manifest_path),
        "thresholds": str(thresholds_path),
        "environment_compatibility": compatibility,
        "summary": {
            "checked": checked,
            "warnings": warns,
            "failures": fails,
            "missing_baseline": missing_baseline,
            "report_only_regressions": report_only,
            "baseline_compatible": True,
            "baseline_skip_reason": None,
            "current_environment_label": environment_label(
                current_manifest.get("environment")
            ),
            "baseline_environment_label": environment_label(
                baseline_manifest.get("environment")
            ),
        },
        "comparisons": comparisons,
        "top_regressions": regressions[:15],
        "top_improvements": improvements[:15],
    }


def suite_card(
    suite: dict[str, Any],
    rows: list[dict[str, Any]],
) -> dict[str, Any]:
    return {
        "name": suite["name"],
        "status": suite.get("status", "unknown"),
        "benchmark_count": len(rows),
        "filter": suite.get("filter"),
        "artifacts": suite.get("artifacts", {}),
    }


def build_report_data(
    manifest_path: Path,
    comparison_path: Path | None = None,
) -> dict[str, Any]:
    manifest = load_manifest(manifest_path)
    run_root = manifest_path.parent
    rows_by_suite = load_run_rows(manifest, run_root)
    rows = [row for suite_rows in rows_by_suite.values() for row in suite_rows]
    rows_sorted = sorted(
        rows,
        key=lambda row: row.get("items_per_second") or 0.0,
        reverse=True,
    )
    comparison = read_json(comparison_path) if comparison_path and comparison_path.exists() else None
    return {
        "generated_at": utc_now_iso(),
        "run_id": manifest.get("run_id"),
        "preset": manifest.get("preset"),
        "environment": manifest.get("environment"),
        "suite_cards": [
            suite_card(suite, rows_by_suite.get(suite["name"], []))
            for suite in manifest.get("suites", [])
        ],
        "rows": rows,
        "top_rows": rows_sorted[:30],
        "comparison_summary": comparison.get("summary") if comparison else None,
        "top_regressions": comparison.get("top_regressions", []) if comparison else [],
        "top_improvements": comparison.get("top_improvements", []) if comparison else [],
    }


def format_number(value: float | int | None, decimals: int = 2) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, int):
        return f"{value}"
    if math.isfinite(value) and abs(value) >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if math.isfinite(value) and abs(value) >= 1_000:
        return f"{value / 1_000:.2f}K"
    return f"{value:.{decimals}f}"


def render_markdown_report(report_data: dict[str, Any]) -> str:
    lines = [
        "# PerfMap Benchmark Report",
        "",
        f"- Run ID: `{report_data['run_id']}`",
        f"- Preset: `{report_data['preset']}`",
        f"- Generated: `{report_data['generated_at']}`",
        f"- Environment: `{environment_label(report_data.get('environment'))}`",
        "",
        "## Suite Summary",
        "",
        "| Suite | Status | Benchmarks |",
        "|---|---|---:|",
    ]
    for suite in report_data["suite_cards"]:
        lines.append(
            f"| {suite['name']} | {suite['status']} | {suite['benchmark_count']} |"
        )

    if report_data.get("comparison_summary"):
        summary = report_data["comparison_summary"]
        lines.extend(
            [
                "",
                "## Regression Summary",
                "",
                f"- Checked benchmarks: {summary['checked']}",
                f"- Warnings: {summary['warnings']}",
                f"- Failures: {summary['failures']}",
                f"- Missing baseline: {summary['missing_baseline']}",
                f"- Report-only regressions: {summary.get('report_only_regressions', 0)}",
            ]
        )
        if not summary.get("baseline_compatible", True):
            lines.extend(
                [
                    f"- Baseline compatibility: skipped (`{summary['baseline_skip_reason']}`)",
                    f"- Current environment: `{summary.get('current_environment_label', 'n/a')}`",
                    f"- Baseline environment: `{summary.get('baseline_environment_label', 'n/a')}`",
                ]
            )

    lines.extend(
        [
            "",
            "## Key Results",
            "",
            "| Benchmark | Throughput | Bytes / Live Entry | Total Reserved Bytes |",
            "|---|---:|---:|---:|",
        ]
    )
    for row in report_data["top_rows"][:20]:
        lines.append(
            "| {name} | {throughput} | {bytes_per_live} | {reserved} |".format(
                name=row["name"],
                throughput=format_number(row.get("items_per_second")),
                bytes_per_live=format_number(
                    row["counters"].get("bytes_per_live_entry")
                ),
                reserved=format_number(row["counters"].get("total_reserved_bytes")),
            )
        )
    return "\n".join(lines) + "\n"


def render_html_report(report_data: dict[str, Any]) -> str:
    suite_rows = "".join(
        f"<tr><td>{suite['name']}</td><td>{suite['status']}</td><td>{suite['benchmark_count']}</td></tr>"
        for suite in report_data["suite_cards"]
    )
    result_rows = "".join(
        "<tr>"
        f"<td>{row['name']}</td>"
        f"<td>{format_number(row.get('items_per_second'))}</td>"
        f"<td>{format_number(row['counters'].get('bytes_per_live_entry'))}</td>"
        f"<td>{format_number(row['counters'].get('total_reserved_bytes'))}</td>"
        "</tr>"
        for row in report_data["top_rows"][:30]
    )
    comparison = report_data.get("comparison_summary")
    comparison_html = ""
    if comparison:
        extra = ""
        if not comparison.get("baseline_compatible", True):
            extra = (
                f"<p>Baseline compatibility: skipped ({comparison['baseline_skip_reason']})<br>"
                f"Current environment: {comparison.get('current_environment_label', 'n/a')}<br>"
                f"Baseline environment: {comparison.get('baseline_environment_label', 'n/a')}</p>"
            )
        comparison_html = (
            "<section><h2>Regression Summary</h2>"
            f"<p>Checked: {comparison['checked']} | "
            f"Warnings: {comparison['warnings']} | "
            f"Failures: {comparison['failures']} | "
            f"Missing baseline: {comparison['missing_baseline']} | "
            f"Report-only regressions: {comparison.get('report_only_regressions', 0)}</p>"
            f"{extra}</section>"
        )

    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>PerfMap Benchmark Report</title>
  <style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, sans-serif; margin: 32px; color: #17202a; background: #f7f9fb; }}
    h1, h2 {{ margin-bottom: 0.35rem; }}
    table {{ border-collapse: collapse; width: 100%; margin: 16px 0 28px; background: white; }}
    th, td {{ border: 1px solid #d9e2ec; padding: 10px; text-align: left; }}
    th {{ background: #eef4f8; }}
    .meta {{ color: #52606d; }}
    .card {{ background: white; border: 1px solid #d9e2ec; border-radius: 12px; padding: 18px; margin-bottom: 18px; }}
  </style>
</head>
<body>
  <div class="card">
    <h1>PerfMap Benchmark Report</h1>
    <p class="meta">Run ID: {report_data['run_id']} | Preset: {report_data['preset']} | Generated: {report_data['generated_at']} | Environment: {environment_label(report_data.get('environment'))}</p>
  </div>
  <section>
    <h2>Suite Summary</h2>
    <table>
      <thead><tr><th>Suite</th><th>Status</th><th>Benchmarks</th></tr></thead>
      <tbody>{suite_rows}</tbody>
    </table>
  </section>
  {comparison_html}
  <section>
    <h2>Key Results</h2>
    <table>
      <thead><tr><th>Benchmark</th><th>Throughput</th><th>Bytes / Live Entry</th><th>Total Reserved Bytes</th></tr></thead>
      <tbody>{result_rows}</tbody>
    </table>
  </section>
</body>
</html>
"""
