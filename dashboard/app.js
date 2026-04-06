const DATA_ROOT = "..";

async function fetchJson(path) {
  const response = await fetch(path, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Failed to fetch ${path}: ${response.status}`);
  }
  return await response.json();
}

async function fetchJsonl(path) {
  const response = await fetch(path, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Failed to fetch ${path}: ${response.status}`);
  }
  const text = await response.text();
  return text
    .trim()
    .split("\n")
    .filter(Boolean)
    .map((line) => JSON.parse(line));
}

function formatNumber(value) {
  if (value === null || value === undefined) return "n/a";
  if (Math.abs(value) >= 1_000_000) return `${(value / 1_000_000).toFixed(2)}M`;
  if (Math.abs(value) >= 1_000) return `${(value / 1_000).toFixed(2)}K`;
  return Number(value).toFixed(2);
}

function environmentLabel(environment) {
  return environment?.label || "unknown environment";
}

function renderArtifactLinks(artifacts) {
  const container = document.getElementById("artifact-links");
  container.innerHTML = "";
  Object.entries(artifacts || {}).forEach(([key, value]) => {
    if (!value) return;
    const link = document.createElement("a");
    link.href = `../${value}`;
    link.textContent = key.replaceAll("_", " ");
    container.appendChild(link);
  });
}

function suiteDetailText(suite, completed, total) {
  if (suite.current_benchmark) {
    return `Running ${suite.current_benchmark}`;
  }
  if (suite.status === "passed") {
    return `${completed}/${total} benchmarks complete`;
  }
  if (suite.status === "failed") {
    return `${completed}/${total} benchmarks completed before failure`;
  }
  if (suite.status === "running") {
    return `${completed}/${total} benchmarks complete`;
  }
  if (suite.status === "pending") {
    return `Queued with ${total} benchmarks`;
  }
  return suite.status || "unknown";
}

function renderSuites(suites) {
  const container = document.getElementById("suite-list");
  container.innerHTML = "";
  (suites || []).forEach((suite) => {
    const total = suite.benchmark_total || 0;
    const completed = suite.benchmark_completed || 0;
    const percent = total === 0 ? 0 : (completed / total) * 100;
    const detail = suiteDetailText(suite, completed, total);
    const filterMarkup = suite.filter
      ? `
        <details class="suite-filter">
          <summary>Benchmark filter</summary>
          <code>${suite.filter}</code>
        </details>
      `
      : "";
    const card = document.createElement("div");
    card.className = "suite-card";
    card.innerHTML = `
      <header>
        <div>
          <strong>${suite.name}</strong>
          <div class="muted">${suite.status}</div>
        </div>
        <div class="muted">${completed}/${total}</div>
      </header>
      <div class="progress-track"><div class="progress-bar" style="width: ${percent}%"></div></div>
      <p class="suite-detail">${detail}</p>
      ${filterMarkup}
    `;
    container.appendChild(card);
  });
}

function renderRecent(rows) {
  const tbody = document.getElementById("recent-table");
  tbody.innerHTML = "";
  (rows || []).slice().reverse().forEach((row) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${row.suite}</td>
      <td>${row.name}</td>
      <td>${formatNumber(row.items_per_second)}</td>
      <td>${formatNumber(row.bytes_per_live_entry)}</td>
      <td>${formatNumber(row.total_reserved_bytes)}</td>
    `;
    tbody.appendChild(tr);
  });
}

function renderComparison(comparison) {
  const summary = document.getElementById("comparison-summary");
  const tbody = document.getElementById("comparison-table");
  tbody.innerHTML = "";
  if (!comparison || !comparison.summary) {
    summary.textContent = "No comparison loaded yet.";
    return;
  }
  if (comparison.summary.baseline_compatible === false) {
    summary.textContent =
      `Comparison skipped: ${comparison.summary.baseline_skip_reason}. ` +
      `Current: ${comparison.summary.current_environment_label || "n/a"}. ` +
      `Baseline: ${comparison.summary.baseline_environment_label || "n/a"}.`;
    return;
  }
  summary.textContent = `Checked ${comparison.summary.checked}, warnings ${comparison.summary.warnings}, failures ${comparison.summary.failures}, missing baseline ${comparison.summary.missing_baseline}, report-only regressions ${comparison.summary.report_only_regressions || 0}.`;
  (comparison.top_regressions || []).forEach((row) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td class="status-${row.status}">${row.status}</td>
      <td>${row.name}</td>
      <td>${formatNumber(row.items_per_second_delta_pct)}</td>
      <td>${formatNumber(row.bytes_per_live_entry_delta_pct)}</td>
      <td>${formatNumber(row.total_reserved_bytes_delta_pct)}</td>
    `;
    tbody.appendChild(tr);
  });
}

function renderResults(reportData) {
  const tbody = document.getElementById("results-table");
  tbody.innerHTML = "";
  (reportData?.top_rows || []).forEach((row) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${row.suite}</td>
      <td>${row.name}</td>
      <td>${formatNumber(row.items_per_second)}</td>
      <td>${formatNumber(row.counters?.bytes_per_live_entry)}</td>
      <td>${formatNumber(row.counters?.total_reserved_bytes)}</td>
    `;
    tbody.appendChild(tr);
  });
}

function renderEvents(events) {
  const container = document.getElementById("event-log");
  container.innerHTML = "";
  events.slice(-20).reverse().forEach((event) => {
    const div = document.createElement("div");
    div.className = "event-item";
    div.innerHTML = `<strong>${event.event}</strong><br><span class="muted">${event.ts}</span><br>${event.suite || event.benchmark || ""}`;
    container.appendChild(div);
  });
}

async function refresh() {
  try {
    const [summary, reportData, comparison, events] = await Promise.all([
      fetchJson(`${DATA_ROOT}/summary.json`),
      fetchJson(`${DATA_ROOT}/report_data.json`).catch(() => ({ top_rows: [] })),
      fetchJson(`${DATA_ROOT}/comparison.json`).catch(() => null),
      fetchJsonl(`${DATA_ROOT}/events.jsonl`).catch(() => []),
    ]);

    document.getElementById("run-title").textContent = `Run ${summary.run_id}`;
    document.getElementById("run-meta").textContent =
      `${summary.generated_at || "n/a"} | ${environmentLabel(summary.environment)}`;
    document.getElementById("run-status").textContent = summary.status;
    document.getElementById("current-stage").textContent = summary.current_stage || "-";
    document.getElementById("current-suite").textContent = summary.current_suite || "-";
    document.getElementById("current-benchmark").textContent = summary.current_benchmark || "-";
    document.getElementById("finished-at").textContent = summary.finished_at || "running";
    document.getElementById("run-preset").textContent = summary.preset || "-";

    renderArtifactLinks(summary.artifacts);
    renderSuites(summary.suites);
    renderRecent(summary.recent_completed_benchmarks);
    renderComparison(comparison);
    renderResults(reportData);
    renderEvents(events);
  } catch (error) {
    document.getElementById("run-meta").textContent = error.message;
  }
}

refresh();
setInterval(refresh, 1500);
