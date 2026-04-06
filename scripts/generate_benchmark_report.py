#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from benchmark_lib import (
    build_report_data,
    render_html_report,
    render_markdown_report,
    write_json,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate markdown and HTML reports for a benchmark run.")
    parser.add_argument("--run-dir", required=True, help="Run directory containing manifest.json")
    parser.add_argument("--output-markdown", required=True)
    parser.add_argument("--output-html", required=True)
    parser.add_argument("--output-data", required=True)
    parser.add_argument("--comparison", help="Optional comparison.json path", default=None)
    args = parser.parse_args()

    run_dir = Path(args.run_dir)
    comparison_path = Path(args.comparison) if args.comparison else None
    report_data = build_report_data(run_dir / "manifest.json", comparison_path)
    write_json(Path(args.output_data), report_data)
    Path(args.output_markdown).write_text(render_markdown_report(report_data))
    Path(args.output_html).write_text(render_html_report(report_data))
    print(f"wrote {args.output_markdown}, {args.output_html}, and {args.output_data}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
