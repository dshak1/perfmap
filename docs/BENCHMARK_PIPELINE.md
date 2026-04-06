# Benchmark Pipeline

PerfMap now has a real benchmark pipeline instead of ad hoc one-off commands.

## Presets

### Smoke

Purpose:

- fast CI for pushes and pull requests
- validates build, tests, fixture generation, report generation, and dashboard assets

Run:

```bash
python3 scripts/run_benchmarks.py --preset smoke --build-dir build --run-name local-smoke --skip-compare
```

### Full

Purpose:

- main-branch and scheduled benchmark runs
- repeated benchmarks with aggregate output
- baseline comparison
- regression enforcement on protected benchmarks

Run:

```bash
python3 scripts/run_benchmarks.py --preset full --build-dir build --run-name local-full --enforce-regressions
```

## Artifacts

Each run directory under `results/runs/<run-id>/` contains:

- `manifest.json`
- `summary.json`
- `events.jsonl`
- suite JSON outputs
- `comparison.json` when a baseline exists
- `report.md`
- `report.html`
- `report_data.json`
- `dashboard/`

`summary.json` and `events.jsonl` are the live observability contract. The
dashboard consumes them directly while the run is in progress and after it
finishes.

## Baseline Flow

The checked-in baseline lives in `bench/baselines/default/`.

To update it intentionally:

```bash
python3 scripts/update_baseline.py \
  --run-dir results/runs/local-full \
  --baseline-dir bench/baselines/default
```

That copies suite JSON outputs and writes a new baseline manifest. The
thresholds remain in `bench/baselines/default/thresholds.json`.

## Regression Logic

PerfMap compares every benchmark row, but only a documented protected subset is
CI-gating.

- protected benchmarks can warn or fail
- all other regressions are reported but do not fail CI

That design is intentional. Benchmark noise is real, so the pipeline only gates
on benchmarks stable enough to defend.
