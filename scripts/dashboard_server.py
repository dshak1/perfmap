#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def latest_run(root: Path) -> Path:
    candidates = sorted([path for path in root.iterdir() if path.is_dir()])
    if not candidates:
        raise SystemExit(f"No run directories found under {root}")
    return candidates[-1]


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve a PerfMap benchmark dashboard locally.")
    parser.add_argument("--run-dir", help="Run directory to serve")
    parser.add_argument("--runs-root", default="results/runs")
    parser.add_argument("--port", type=int, default=8123)
    args = parser.parse_args()

    run_dir = Path(args.run_dir) if args.run_dir else latest_run(Path(args.runs_root))
    os.chdir(run_dir)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), SimpleHTTPRequestHandler)
    print(f"Serving {run_dir} at http://127.0.0.1:{args.port}/dashboard/index.html")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
