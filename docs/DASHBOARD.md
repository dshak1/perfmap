# Dashboard

PerfMap ships a lightweight local dashboard for live and post-run inspection.

## Start the Server

```bash
python3 scripts/dashboard_server.py --run-dir results/runs/local-smoke --port 8123
```

Open:

- `http://127.0.0.1:8123/dashboard/index.html`

## What the Dashboard Shows

- run status
- current stage
- current suite and next benchmark
- suite progress
- recent completed benchmarks from the structured event stream
- key results from `report_data.json`
- regression summary from `comparison.json`
- links to JSON, Markdown, HTML, and dashboard artifacts

## Live vs Post-Run

The dashboard reads:

- `summary.json`
- `events.jsonl`
- `report_data.json`
- `comparison.json`

Because those files live inside the run directory and are updated as the run
progresses, the same dashboard works during execution and after completion.
