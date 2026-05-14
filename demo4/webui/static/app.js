const PARAM_FIELDS = [
  "lam",
  "Pe",
  "Pe2",
  "eps",
  "Da",
  "K0",
  "ny",
  "xpo_l",
  "xpo_r",
  "endT",
  "total_count",
  "coeff_dt",
  "x_ini_posi",
  "alpha",
  "Sc",
];

const GENERATOR_DEFAULTS = {
  aMin: 0.001,
  aMax: 10,
  aCount: 41,
  pe1Min: 1000,
  pe1Max: 1000,
  pe1Count: 1,
  pe2Min: 0.1,
  pe2Max: 1e8,
  pe2Count: 91,
  lam: 0.033333,
  eps: 0.1,
  Da: 100,
  K0: 1,
  ny: 50,
  xpo_l: 0.333333,
  xpo_r: 0.666667,
  total_count: 10,
  x_ini_posi: 5,
  coeffMax: 0.1,
  Sc: 16667,
};

const SKIP_WARMUP_STORAGE_KEY = "demo4.skipWarmupDefault";

const state = {
  env: null,
  cases: [],
  currentCaseId: null,
  currentCase: null,
  pollHandle: null,
  caseQuery: "",
  caseOffset: 0,
  caseLimit: 50,
  caseTotal: 0,
  caseHasMore: false,
  caseMaxId: 0,
  caseFormBaseline: "",
  caseFormDirty: false,
  resultMeta: null,
  etaPoints: [],
  currentSnapshotCount: null,
  // Run overview / log sub-tab state
  lastBuildLog: [],
  lastRunLog: [],
  runProgressHistory: [], // err samples for sparkline
  runSubtab: "overview",
  logOnlyErrors: { build: false, run: false },
  logFollow: { build: true, run: true },
  warmupPollHandle: null,
  warmupStatus: null,
  selectedWarmupConcurrency: null,
  warmupCandidatesTouched: false,
  demo4Status: null,
  demo4Results: null,
  demo4PollHandle: null,
};

const caseListEl = document.getElementById("case-list");
const caseSearchInputEl = document.getElementById("case-search-input");
const clearCaseSearchBtnEl = document.getElementById("clear-case-search-btn");
const caseListMetaEl = document.getElementById("case-list-meta");
const loadMoreCasesBtnEl = document.getElementById("load-more-cases-btn");
const envGridEl = document.getElementById("env-grid");
const envBannerEl = document.getElementById("env-banner");
const currentCaseLabelEl = document.getElementById("current-case-label");
const taskStateBadgeEl = document.getElementById("task-state-badge");
const caseIdInputEl = document.getElementById("field-case-id");
const saveHintEl = document.getElementById("save-hint");
const buildRunBtnEl = document.getElementById("build-run-btn");
const batchRunBtnEl = document.getElementById("batch-run-btn");
const batchRunScopeEl = document.getElementById("batch-run-scope");
const stopBtnEl = document.getElementById("stop-btn");
const forceRestartEl = document.getElementById("force-restart-checkbox");
const buildLogEl = document.getElementById("build-log");
const runLogEl = document.getElementById("run-log");
const runStageEl = document.getElementById("run-stage");
const batchProgressEl = document.getElementById("batch-progress");
const runStartedEl = document.getElementById("run-started");
const runFinishedEl = document.getElementById("run-finished");
const buildExitEl = document.getElementById("build-exit");
const runExitEl = document.getElementById("run-exit");
const runErrorEl = document.getElementById("run-error");
const snapshotSelectEl = document.getElementById("snapshot-select");
const resultsEmptyEl = document.getElementById("results-empty");
const resultsBodyEl = document.getElementById("results-body");
const summaryGridEl = document.getElementById("summary-grid");
const workflowTabEls = Array.from(document.querySelectorAll("[data-tab-target]"));
const workflowPanelEls = Array.from(document.querySelectorAll(".tab-panel"));

// Run sub-tab + overview elements
const runSubtabEls = Array.from(document.querySelectorAll("[data-run-subtab]"));
const runStatusBannerEl = document.getElementById("run-status-banner");
const stageChipsEl = document.getElementById("stage-chips");
const overallProgressFillEl = document.getElementById("overall-progress-fill");
const overallProgressLabelEl = document.getElementById("overall-progress-label");
const overallProgressPercentEl = document.getElementById("overall-progress-percent");
const runKpiEl = document.getElementById("run-kpi");
const kpiCaseEl = document.getElementById("kpi-case");
const kpiBackendEl = document.getElementById("kpi-backend");
const kpiEtaEl = document.getElementById("kpi-eta");
const kpiErrEl = document.getElementById("kpi-err");
const kpiUsedEl = document.getElementById("kpi-used");
const kpiErrSparkEl = document.getElementById("kpi-err-spark");
const generateFormEl = document.getElementById("generate-form");
const previewGenerateBtnEl = document.getElementById("preview-generate-btn");
const generateCasesBtnEl = document.getElementById("generate-cases-btn");
const generateMessageEl = document.getElementById("generate-message");
const generatePreviewEl = document.getElementById("generate-preview");
const generatePlannedEl = document.getElementById("generate-planned");
const generateIdRangeEl = document.getElementById("generate-id-range");
const generateOutputEl = document.getElementById("generate-output");
const warmupLogicalEl = document.getElementById("warmup-logical");
const warmupNumThreadsEl = document.getElementById("warmup-num-threads");
const warmupCaseCountEl = document.getElementById("warmup-case-count");
const warmupLeaderEl = document.getElementById("warmup-leader");
const warmupCandidatesInputEl = document.getElementById("warmup-candidates-input");
const warmupSecondsInputEl = document.getElementById("warmup-seconds-input");
const warmupWarmupSecondsInputEl = document.getElementById("warmup-warmup-seconds-input");
const warmupResultsBodyEl = document.getElementById("warmup-results-body");
const warmupLogEl = document.getElementById("warmup-log");
const warmupErrorEl = document.getElementById("warmup-error");
const startWarmupBtnEl = document.getElementById("start-warmup-btn");
const stopWarmupBtnEl = document.getElementById("stop-warmup-btn");
const applyWarmupBtnEl = document.getElementById("apply-warmup-btn");
const warmupManualInputEl = document.getElementById("warmup-manual-concurrency-input");
const warmupSkipInputEl = document.getElementById("warmup-skip-checkbox");
const demo4MessageEl = document.getElementById("demo4-message");
const demo4StatusGridEl = document.getElementById("demo4-status-grid");
const demo4ProgressPanelEl = document.getElementById("demo4-progress-panel");
const demo4ProgressLabelEl = document.getElementById("demo4-progress-label");
const demo4ProgressPercentEl = document.getElementById("demo4-progress-percent");
const demo4ProgressFillEl = document.getElementById("demo4-progress-fill");
const demo4ProgressBodyEl = document.getElementById("demo4-progress-body");
const demo4LogEl = document.getElementById("demo4-log");
const demo4PrecheckBtnEl = document.getElementById("demo4-precheck-btn");
const demo4ValidationDtBtnEl = document.getElementById("demo4-validation-dt-btn");
const demo4ValidationNyBtnEl = document.getElementById("demo4-validation-ny-btn");
const demo4BuildBtnEl = document.getElementById("demo4-build-btn");
const demo4ReportBtnEl = document.getElementById("demo4-report-btn");
const demo4StopBtnEl = document.getElementById("demo4-stop-btn");
const demo4RefreshBtnEl = document.getElementById("demo4-refresh-btn");
const demo4SchemeBodyEl = document.getElementById("demo4-scheme-body");
const demo4PrecheckBodyEl = document.getElementById("demo4-precheck-body");
const demo4ValidationBodyEl = document.getElementById("demo4-validation-body");
const demo4CurveMetricsBodyEl = document.getElementById("demo4-curve-metrics-body");
const demo4EtaCurveChartEl = document.getElementById("demo4-eta-curve-chart");

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: {
      "Content-Type": "application/json",
    },
    ...options,
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    const error = new Error(payload.error || `Request failed: ${response.status}`);
    error.code = payload.code || "REQUEST_FAILED";
    error.details = payload.details || {};
    error.status = response.status;
    throw error;
  }
  return payload;
}

function showMessage(message) {
  envBannerEl.textContent = message;
}

function showGenerateMessage(message) {
  if (generateMessageEl) {
    generateMessageEl.textContent = message;
  }
}

function generatorInput(name) {
  return generateFormEl ? generateFormEl.querySelector(`[data-gen-field="${name}"]`) : null;
}

function setGeneratorField(name, value) {
  const input = generatorInput(name);
  if (!input) return;
  if (input.type === "checkbox") {
    input.checked = Boolean(value);
  } else {
    input.value = String(value);
  }
}

function fillGeneratorDefaults() {
  Object.entries(GENERATOR_DEFAULTS).forEach(([name, value]) => setGeneratorField(name, value));
  setGeneratorField("startId", Math.max(1, (state.caseMaxId || 0) + 1));
  setGeneratorField("overwrite", false);
}

function readGeneratorNumber(name) {
  const input = generatorInput(name);
  const value = Number(input ? input.value : "");
  if (!Number.isFinite(value)) {
    throw new Error(`${name} must be numeric.`);
  }
  return value;
}

function collectGeneratorPayload({ dryRun = false } = {}) {
  return {
    aRange: [readGeneratorNumber("aMin"), readGeneratorNumber("aMax"), readGeneratorNumber("aCount")],
    Pe1Range: [readGeneratorNumber("pe1Min"), readGeneratorNumber("pe1Max"), readGeneratorNumber("pe1Count")],
    Pe2Range: [readGeneratorNumber("pe2Min"), readGeneratorNumber("pe2Max"), readGeneratorNumber("pe2Count")],
    fixed: {
      lam: readGeneratorNumber("lam"),
      eps: readGeneratorNumber("eps"),
      Da: readGeneratorNumber("Da"),
      K0: readGeneratorNumber("K0"),
      ny: readGeneratorNumber("ny"),
      xpo_l: readGeneratorNumber("xpo_l"),
      xpo_r: readGeneratorNumber("xpo_r"),
      total_count: readGeneratorNumber("total_count"),
      x_ini_posi: readGeneratorNumber("x_ini_posi"),
    },
    coeffMax: readGeneratorNumber("coeffMax"),
    Sc: readGeneratorNumber("Sc"),
    startId: readGeneratorNumber("startId"),
    overwrite: Boolean(generatorInput("overwrite")?.checked),
    dryRun,
  };
}

function renderGeneratorPreview(payload) {
  if (!generatePreviewEl) return;
  generatePlannedEl.textContent = String(payload.planned ?? "-");
  generateIdRangeEl.textContent = payload.planned ? `${payload.startId} - ${payload.endId}` : "-";
  generateOutputEl.textContent = payload.inputDir || "-";
  generatePreviewEl.replaceChildren();

  const previewRows = [...(payload.preview?.first || []), ...(payload.preview?.last || [])];
  const rows = Array.from(new Map(previewRows.map((row) => [row.id, row])).values());
  const scatterPoints = payload.scatter?.points || [];
  if (!rows.length && !scatterPoints.length) {
    generatePreviewEl.classList.add("hidden");
    return;
  }

  generatePreviewEl.classList.remove("hidden");
  const chartWrap = document.createElement("section");
  chartWrap.className = "generator-chart";
  const chartHeader = document.createElement("div");
  chartHeader.className = "generator-chart-header";
  const chartTitle = document.createElement("h3");
  chartTitle.textContent = "alpha / Pe2 coverage";
  const chartMeta = document.createElement("span");
  const uniqueCount = payload.scatter?.total ?? scatterPoints.length;
  chartMeta.textContent = `${uniqueCount} unique point${uniqueCount === 1 ? "" : "s"}${payload.scatter?.sampled ? " sampled" : ""}`;
  chartHeader.append(chartTitle, chartMeta);
  const canvas = document.createElement("canvas");
  canvas.width = 980;
  canvas.height = 420;
  chartWrap.append(chartHeader, canvas);
  generatePreviewEl.appendChild(chartWrap);
  drawAlphaPe2Scatter(canvas, scatterPoints);

  const table = document.createElement("table");
  table.className = "preview-table";
  const header = document.createElement("tr");
  ["id", "Pe", "Pe2", "alpha", "endT", "coeff_dt"].forEach((name) => {
    const th = document.createElement("th");
    th.textContent = name;
    header.appendChild(th);
  });
  table.appendChild(header);
  rows.forEach((row) => {
    const tr = document.createElement("tr");
    ["id", "Pe", "Pe2", "alpha", "endT", "coeff_dt"].forEach((name) => {
      const td = document.createElement("td");
      td.textContent = formatValue(row[name]);
      tr.appendChild(td);
    });
    table.appendChild(tr);
  });
  const tableWrap = document.createElement("div");
  tableWrap.className = "preview-table-wrap";
  tableWrap.appendChild(table);
  generatePreviewEl.appendChild(tableWrap);
}

function drawAlphaPe2Scatter(canvas, points) {
  const { ctx, width, height } = setupHiDPI(canvas);
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, width, height);

  const valid = points
    .map((point) => ({ alpha: Number(point.alpha), pe2: Number(point.Pe2), count: Number(point.count || 1) }))
    .filter((point) => point.alpha > 0 && point.pe2 > 0 && Number.isFinite(point.alpha) && Number.isFinite(point.pe2));

  if (!valid.length) {
    ctx.fillStyle = "#6b7280";
    ctx.font = "14px sans-serif";
    ctx.fillText("No alpha / Pe2 points to preview", 24, height / 2);
    return;
  }

  const margin = { left: 72, right: 26, top: 20, bottom: 58 };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;
  const logAlpha = valid.map((point) => Math.log10(point.alpha));
  const logPe2 = valid.map((point) => Math.log10(point.pe2));
  const xDomain = paddedDomain(logAlpha);
  const yDomain = paddedDomain(logPe2);
  const xSpan = xDomain[1] - xDomain[0] || 1;
  const ySpan = yDomain[1] - yDomain[0] || 1;
  const xFor = (value) => margin.left + ((Math.log10(value) - xDomain[0]) / xSpan) * plotW;
  const yFor = (value) => margin.top + plotH - ((Math.log10(value) - yDomain[0]) / ySpan) * plotH;

  ctx.strokeStyle = "#e5e7eb";
  ctx.lineWidth = 1;
  ctx.font = "11px sans-serif";
  ctx.fillStyle = "#6b7280";
  for (let i = 0; i <= 5; i += 1) {
    const x = margin.left + (plotW * i) / 5;
    const y = margin.top + (plotH * i) / 5;
    ctx.beginPath();
    ctx.moveTo(x, margin.top);
    ctx.lineTo(x, margin.top + plotH);
    ctx.moveTo(margin.left, y);
    ctx.lineTo(margin.left + plotW, y);
    ctx.stroke();
    const xValue = 10 ** (xDomain[0] + (xSpan * i) / 5);
    const yValue = 10 ** (yDomain[1] - (ySpan * i) / 5);
    ctx.fillText(formatTick(xValue), x - 16, height - 28);
    ctx.fillText(formatTick(yValue), 12, y + 4);
  }

  ctx.strokeStyle = "#111827";
  ctx.lineWidth = 1.2;
  ctx.strokeRect(margin.left, margin.top, plotW, plotH);

  const maxCount = Math.max(...valid.map((point) => point.count), 1);
  const densityRadius = Math.sqrt((plotW * plotH) / (Math.PI * Math.max(valid.length, 1))) * 0.12;
  const baseRadius = Math.max(1.8, Math.min(4.2, densityRadius));
  const maxRadius = Math.max(baseRadius + 1.2, Math.min(7.2, baseRadius * 1.9));
  valid.forEach((point) => {
    const ratio = Math.sqrt(point.count / maxCount);
    const radius = baseRadius + ratio * (maxRadius - baseRadius);
    const x = xFor(point.alpha);
    const y = yFor(point.pe2);
    const glow = ctx.createRadialGradient(x, y, 0, x, y, radius * 2.5);
    glow.addColorStop(0, "rgba(15, 118, 110, 0.24)");
    glow.addColorStop(1, "rgba(15, 118, 110, 0)");
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(x, y, radius * 2.1, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = "rgba(15, 118, 110, 0.82)";
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
  });

  ctx.fillStyle = "#111827";
  ctx.font = "12px sans-serif";
  ctx.fillText("alpha", margin.left + plotW - 38, height - 10);
  ctx.save();
  ctx.translate(18, margin.top + 28);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText("Pe2", 0, 0);
  ctx.restore();
}

async function previewGeneratedCases() {
  const payload = collectGeneratorPayload({ dryRun: true });
  const result = await api("/api/generate-cases", {
    method: "POST",
    body: JSON.stringify(payload),
  });
  renderGeneratorPreview(result);
  const collisionText = result.collisions?.length ? ` Collisions: ${result.collisions.join(", ")}.` : "";
  showGenerateMessage(`Preview ready: ${result.planned} case(s).${collisionText}`);
}

async function generateCasesFromForm() {
  const payload = collectGeneratorPayload({ dryRun: false });
  const result = await api("/api/generate-cases", {
    method: "POST",
    body: JSON.stringify(payload),
  });
  renderGeneratorPreview(result);
  showGenerateMessage(`Generated ${result.generated} case(s): ${result.startId} - ${result.endId}.`);
  await refreshCases();
  if (result.startId) {
    await loadCase(result.startId);
  }
}

function setRunError(message) {
  if (!message) {
    runErrorEl.classList.add("hidden");
    runErrorEl.textContent = "";
    return;
  }
  runErrorEl.classList.remove("hidden");
  runErrorEl.textContent = message;
}

function selectWorkflowTab(target) {
  workflowTabEls.forEach((tab) => {
    const isActive = tab.dataset.tabTarget === target;
    tab.classList.toggle("active", isActive);
    tab.setAttribute("aria-selected", String(isActive));
  });

  workflowPanelEls.forEach((panel) => {
    panel.classList.toggle("hidden", panel.id !== `${target}-tab-panel`);
  });

  if (target === "results" && state.currentCaseId) {
    refreshResults().catch((error) => setRunError(error.message));
  }
  if (target === "warmup") {
    refreshWarmupStatus().catch((error) => setWarmupError(error.message));
  }
}

function setDemo4Message(message) {
  if (!demo4MessageEl || !message) return;
  demo4MessageEl.textContent = message;
}

function demo4SelectedInput(kind) {
  const select = document.getElementById(`demo4-${kind}-input-dir`);
  const custom = document.getElementById(`demo4-${kind}-custom-input`);
  const selected = select?.value || "demo4/input";
  if (selected === "custom") {
    return custom?.value?.trim() || "demo4/input";
  }
  return selected;
}

function demo4Cell(text, tag = "td") {
  const cell = document.createElement(tag);
  cell.textContent = text ?? "-";
  return cell;
}

function demo4RenderTable(tbody, columns, rows, emptyText) {
  if (!tbody) return;
  tbody.replaceChildren();
  if (!rows.length) {
    const row = document.createElement("tr");
    const cell = demo4Cell(emptyText || "No data.");
    cell.colSpan = columns.length;
    row.appendChild(cell);
    tbody.appendChild(row);
    return;
  }
  const header = document.createElement("tr");
  columns.forEach((column) => header.appendChild(demo4Cell(column.label, "th")));
  tbody.appendChild(header);
  rows.forEach((item) => {
    const row = document.createElement("tr");
    columns.forEach((column) => {
      const raw = typeof column.value === "function" ? column.value(item) : item[column.value];
      row.appendChild(demo4Cell(raw));
    });
    tbody.appendChild(row);
  });
}

function renderDemo4Status(status) {
  if (!demo4StatusGridEl) return;
  state.demo4Status = status;
  const task = status.task || {};
  const files = status.files || {};
  demo4StatusGridEl.replaceChildren();
  const cards = [
    ["Task status", task.status || "idle"],
    ["Task kind", task.kind || "-"],
    ["Precheck rows", files.precheckSummary?.rowCount ?? 0],
    ["Validation rows", files.validationSummary?.rowCount ?? 0],
    ["Summary JSON", files.combinedSummary?.exists ? "generated" : "missing"],
    ["Last finished", task.finishedAt || "-"],
  ];
  cards.forEach(([label, value]) => {
    const item = document.createElement("div");
    item.className = "demo4-status-card";
    const span = document.createElement("span");
    span.textContent = label;
    const strong = document.createElement("strong");
    strong.textContent = String(value);
    item.append(span, strong);
    demo4StatusGridEl.appendChild(item);
  });
  if (demo4LogEl) {
    demo4LogEl.textContent = (task.log || []).join("\n") || "Waiting for a demo4 task.";
    demo4LogEl.scrollTop = demo4LogEl.scrollHeight;
  }
  const isRunning = task.status === "running";
  [demo4PrecheckBtnEl, demo4ValidationDtBtnEl, demo4ValidationNyBtnEl, demo4BuildBtnEl, demo4ReportBtnEl].forEach((button) => {
    if (button) button.disabled = isRunning;
  });
  if (demo4StopBtnEl) demo4StopBtnEl.disabled = !isRunning;
  renderDemo4ValidationProgress(status.validationProgress);
}

function renderDemo4ValidationProgress(progress) {
  if (!demo4ProgressPanelEl) return;
  const rows = progress?.rows || [];
  const overall = Number(progress?.overallPercent || 0);
  const percentText = `${Math.max(0, Math.min(100, overall)).toFixed(1)}%`;
  demo4ProgressLabelEl.textContent = progress
    ? `endT ${formatValue(progress.endT)}`
    : "No validation task";
  demo4ProgressPercentEl.textContent = percentText;
  demo4ProgressFillEl.style.width = percentText;
  demo4RenderTable(
    demo4ProgressBodyEl,
    [
      { label: "case", value: "case" },
      { label: "scheme", value: (row) => demo4DisplayScheme(row) },
      { label: "variant", value: (row) => demo4DisplayVariant(row) },
      { label: "ny", value: (row) => demo4DisplayNy(row) },
      { label: "t", value: (row) => formatValue(row.time) },
      { label: "time (s)", value: (row) => formatValue(row.elapsed_seconds) },
      { label: "endT", value: (row) => formatValue(row.endT) },
      { label: "progress", value: (row) => `${Number(row.percent || 0).toFixed(1)}%` },
      { label: "status", value: "status" },
    ],
    rows,
    "No validation progress."
  );
}

function demo4DisplayVariant(row) {
  return String(row?.display_variant || row?.displayVariant || row?.variant || "-");
}

function demo4DisplayScheme(row) {
  return String(row?.scheme_display || row?.schemeDisplay || row?.advection_scheme || row?.advectionScheme || "-");
}

function demo4DisplayReferenceVariant(row) {
  return String(row?.reference_display_variant || row?.referenceDisplayVariant || row?.reference_variant || "-");
}

function demo4DisplayNy(row) {
  const ny = Number(row?.ny);
  return Number.isFinite(ny) ? `ny_${ny}` : "-";
}

function demo4MetricRows(payload, validationRows) {
  const validationMap = new Map(
    (validationRows || []).map((row) => [`${row.case_id}::${row.variant}`, row])
  );
  return (payload.curveMetricRows || []).map((row) => {
    const validationRow = validationMap.get(`${row.case_id}::${row.variant}`) || null;
    return {
      ...row,
      advection_scheme: row.advection_scheme || validationRow?.advection_scheme || "upwind",
      scheme_display: row.scheme_display || validationRow?.scheme_display || "Baseline Upwind",
      display_variant: row.display_variant || validationRow?.display_variant || row.variant,
      reference_display_variant:
        row.reference_display_variant ||
        validationRow?.reference_display_variant ||
        row.reference_variant,
      elapsed_seconds: validationRow?.elapsed_seconds ?? row.elapsed_seconds,
      iterations_per_second:
        validationRow?.iterations_per_second ?? row.iterations_per_second,
      final_eta_diff_abs:
        validationRow?.final_eta_diff_abs ?? row.final_eta_diff_abs,
    };
  });
}

function demo4CurveLegendLabel(item) {
  const scheme = demo4DisplayScheme(item);
  const label = demo4DisplayVariant(item);
  const nyLabel = demo4DisplayNy(item);
  if (label.startsWith("ny_")) {
    return `${scheme} / case ${item.caseId} / ${label}`;
  }
  if (nyLabel !== "-") {
    return `${scheme} / case ${item.caseId} / ${nyLabel} / ${label}`;
  }
  return `${scheme} / case ${item.caseId} / ${label}`;
}

function renderDemo4Results(payload) {
  state.demo4Results = payload;
  const candidateRows = payload.candidateSchemeRows || payload.summary?.candidate_scheme_rows || [];
  demo4RenderTable(
    demo4SchemeBodyEl,
    [
      { label: "scheme", value: "name" },
      { label: "benefit", value: "expected_benefit" },
      { label: "cost", value: "expected_cost" },
      { label: "risk", value: "implementation_risk" },
      { label: "status", value: "status" },
      { label: "evidence", value: "evidence" },
    ],
    candidateRows,
    "No candidate scheme rows."
  );

  const precheckRows = payload.precheckRows || [];
  demo4RenderTable(
    demo4PrecheckBodyEl,
    [
      { label: "case", value: "case_id" },
      { label: "dt_current", value: (row) => formatValue(row.dt_current) },
      { label: "dt_diffusion_limit", value: (row) => formatValue(row.dt_diffusion_limit) },
      { label: "dt_advection_limit", value: (row) => formatValue(row.dt_advection_limit) },
      { label: "dt_eta_estimate", value: (row) => formatValue(row.dt_eta_estimate) },
      { label: "dt_oscillation_T_over_20", value: (row) => formatValue(row.dt_oscillation_T_over_20) },
      { label: "risk_items", value: "risk_items" },
    ],
    precheckRows,
    "No precheck results."
  );

  const validationRows = payload.validationRows || [];
  const curveMetricRows = demo4MetricRows(payload, validationRows);
  renderDemo4EtaCurveChart(payload.etaSeries || []);
  demo4RenderTable(
    demo4CurveMetricsBodyEl,
    [
      { label: "case", value: "case_id" },
      { label: "scheme", value: (row) => demo4DisplayScheme(row) },
      { label: "variant", value: (row) => demo4DisplayVariant(row) },
      { label: "reference", value: (row) => demo4DisplayReferenceVariant(row) },
      { label: "status", value: "comparison_status" },
      { label: "ny", value: (row) => demo4DisplayNy(row) },
      { label: "max abs", value: (row) => formatScientific(row.linf_abs) },
      { label: "max rel", value: (row) => formatScientific(row.linf_rel) },
      { label: "final eta diff", value: (row) => formatScientific(row.final_eta_diff_abs) },
      { label: "runtime (s)", value: (row) => formatValue(row.elapsed_seconds) },
      { label: "iter/s", value: (row) => formatValue(row.iterations_per_second) },
      { label: "common t", value: (row) => formatValue(row.common_time_end) },
      { label: "coeff_dt", value: (row) => formatValue(row.coeff_dt) },
      { label: "RMS abs", value: (row) => formatScientific(row.rmse_abs) },
      { label: "RMS rel", value: (row) => formatScientific(row.rmse_rel) },
      { label: "AUC abs", value: (row) => formatScientific(row.auc_abs_diff) },
      { label: "AUC rel", value: (row) => formatScientific(row.auc_rel_diff) },
    ],
    curveMetricRows,
    "No curve metrics."
  );
  demo4RenderTable(
    demo4ValidationBodyEl,
    [
      { label: "case", value: "case_id" },
      { label: "scheme", value: (row) => demo4DisplayScheme(row) },
      { label: "variant", value: (row) => demo4DisplayVariant(row) },
      { label: "exit", value: "exit_code" },
      { label: "seconds", value: (row) => formatValue(row.elapsed_seconds) },
      { label: "ny", value: (row) => demo4DisplayNy(row) },
      { label: "final time", value: (row) => formatValue(row.final_time) },
      { label: "eta points", value: "eta_point_count" },
      { label: "iter/s", value: (row) => formatValue(row.iterations_per_second) },
      { label: "dt adjust", value: "dt_adjustment_count" },
    ],
    validationRows,
    "No validation results."
  );
}

function renderDemo4EtaCurveChart(seriesRows) {
  if (!demo4EtaCurveChartEl) return;
  const series = (seriesRows || [])
    .map((row) => ({
      caseId: String(row.case_id ?? ""),
      advectionScheme: String(row.advection_scheme || "upwind"),
      schemeDisplay: String(row.scheme_display || row.advection_scheme || ""),
      variant: String(row.variant || ""),
      displayVariant: String(row.display_variant || row.variant || ""),
      exitCode: Number(row.exit_code),
      ny: Number(row.ny),
      points: (row.points || [])
        .map((point) => ({
          time: Number(point.time),
          eta: Number(point.eta),
        }))
        .filter((point) => Number.isFinite(point.time) && Number.isFinite(point.eta)),
    }))
    .filter((row) => (
      row.caseId &&
      row.points.length >= 2
    ));

  demo4EtaCurveChartEl.replaceChildren();
  if (!series.length) {
    demo4EtaCurveChartEl.className = "demo4-chart-empty";
    demo4EtaCurveChartEl.textContent = "No eta curve validation results.";
    return;
  }

  demo4EtaCurveChartEl.className = "demo4-chart";
  const width = 720;
  const height = 280;
  const margin = { top: 18, right: 26, bottom: 46, left: 72 };
  const plotWidth = width - margin.left - margin.right;
  const plotHeight = height - margin.top - margin.bottom;
  const allPoints = series.flatMap((item) => item.points);
  const xMin = Math.min(...allPoints.map((point) => point.time));
  const xMax = Math.max(...allPoints.map((point) => point.time));
  let yMin = Math.min(...allPoints.map((point) => point.eta));
  let yMax = Math.max(...allPoints.map((point) => point.eta));
  if (yMin === yMax) {
    const pad = Math.max(Math.abs(yMax) * 0.1, yMax === 0 ? 1 : 1e-12);
    yMin -= pad;
    yMax += pad;
  }
  const xRange = xMax - xMin || 1;
  const yRange = yMax - yMin || 1;
  const xScale = (value) => margin.left + ((value - xMin) / xRange) * plotWidth;
  const yScale = (value) => margin.top + plotHeight - ((value - yMin) / yRange) * plotHeight;
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", "eta curve comparison chart");
  svg.classList.add("demo4-chart-svg");

  const make = (tag, attrs = {}) => {
    const node = document.createElementNS("http://www.w3.org/2000/svg", tag);
    Object.entries(attrs).forEach(([key, value]) => node.setAttribute(key, String(value)));
    return node;
  };
  const text = (value, x, y, attrs = {}) => {
    const node = make("text", { x, y, ...attrs });
    node.textContent = value;
    return node;
  };

  svg.appendChild(make("line", {
    x1: margin.left,
    y1: margin.top + plotHeight,
    x2: margin.left + plotWidth,
    y2: margin.top + plotHeight,
    class: "demo4-chart-axis",
  }));
  svg.appendChild(make("line", {
    x1: margin.left,
    y1: margin.top,
    x2: margin.left,
    y2: margin.top + plotHeight,
    class: "demo4-chart-axis",
  }));

  const xTicks = [xMin, (xMin + xMax) / 2, xMax];
  xTicks.forEach((tick) => {
    const x = xScale(tick);
    svg.appendChild(make("line", {
      x1: x,
      y1: margin.top + plotHeight,
      x2: x,
      y2: margin.top + plotHeight + 5,
      class: "demo4-chart-axis",
    }));
    svg.appendChild(text(formatValue(tick), x, height - 20, {
      class: "demo4-chart-label",
      "text-anchor": "middle",
    }));
  });
  const yTicks = [yMin, (yMin + yMax) / 2, yMax];
  yTicks.forEach((tick) => {
    const y = yScale(tick);
    svg.appendChild(make("line", {
      x1: margin.left - 5,
      y1: y,
      x2: margin.left,
      y2: y,
      class: "demo4-chart-axis",
    }));
    svg.appendChild(make("line", {
      x1: margin.left,
      y1: y,
      x2: margin.left + plotWidth,
      y2: y,
      class: "demo4-chart-grid",
    }));
    svg.appendChild(text(formatScientific(tick, 2), margin.left - 10, y + 4, {
      class: "demo4-chart-label",
      "text-anchor": "end",
    }));
  });
  svg.appendChild(text("time", margin.left + plotWidth / 2, height - 4, {
    class: "demo4-chart-label",
    "text-anchor": "middle",
  }));
  svg.appendChild(text("eta_ave", 14, margin.top + plotHeight / 2, {
    class: "demo4-chart-label",
    transform: `rotate(-90 14 ${margin.top + plotHeight / 2})`,
    "text-anchor": "middle",
  }));

  const colors = ["#0f766e", "#2563eb", "#b45309", "#7c3aed", "#be123c"];
  const legendColumns = 2;
  const legendColumnWidth = 220;
  const legendRowHeight = 16;
  series.forEach((item, index) => {
    const color = colors[index % colors.length];
    const path = item.points
      .map((point, pointIndex) => `${pointIndex === 0 ? "M" : "L"} ${xScale(point.time).toFixed(2)} ${yScale(point.eta).toFixed(2)}`)
      .join(" ");
    svg.appendChild(make("path", {
      d: path,
      fill: "none",
      stroke: color,
      "stroke-width": 1.35,
    }));
    if (item.points.length <= 18) {
      item.points.forEach((point) => {
        const circle = make("circle", {
          cx: xScale(point.time),
          cy: yScale(point.eta),
          r: 1.75,
          fill: color,
        });
        const title = make("title");
        title.textContent = `${demo4CurveLegendLabel(item)}, t=${formatValue(point.time)}, eta=${formatScientific(point.eta)}`;
        circle.appendChild(title);
        svg.appendChild(circle);
      });
    }
    const legendX = margin.left + 12 + (index % legendColumns) * legendColumnWidth;
    const legendY = margin.top + 12 + Math.floor(index / legendColumns) * legendRowHeight;
    svg.appendChild(text(demo4CurveLegendLabel(item), legendX, legendY, {
      class: "demo4-chart-label",
      fill: color,
    }));
  });

  demo4EtaCurveChartEl.appendChild(svg);
}

async function refreshDemo4Status() {
  const status = await api("/api/demo4/status");
  renderDemo4Status(status);
  if (status.task?.status === "running") {
    if (!state.demo4PollHandle) {
      state.demo4PollHandle = window.setInterval(() => {
        refreshDemo4().catch((error) => setDemo4Message(error.message));
      }, 1500);
    }
  } else if (state.demo4PollHandle) {
    window.clearInterval(state.demo4PollHandle);
    state.demo4PollHandle = null;
  }
  return status;
}

async function refreshDemo4() {
  const [status, results] = await Promise.all([
    api("/api/demo4/status"),
    api("/api/demo4/results"),
  ]);
  renderDemo4Status(status);
  renderDemo4Results(results);
  if (status.task?.status === "running") {
    if (!state.demo4PollHandle) {
      state.demo4PollHandle = window.setInterval(() => {
        refreshDemo4().catch((error) => setDemo4Message(error.message));
      }, 1500);
    }
  } else if (state.demo4PollHandle) {
    window.clearInterval(state.demo4PollHandle);
    state.demo4PollHandle = null;
  }
  return { status, results };
}

async function handleDemo4Precheck() {
  const cases = document.getElementById("demo4-precheck-cases")?.value || "";
  await api("/api/demo4/precheck", {
    method: "POST",
    body: JSON.stringify({ inputDir: demo4SelectedInput("precheck"), cases }),
  });
  setDemo4Message("demo4 precheck started.");
  await refreshDemo4Status();
}

async function handleDemo4Validation(kind) {
  const cases = document.getElementById("demo4-validation-cases")?.value || "";
  const isDt = kind === "dt";
  const advectionScheme = document.getElementById("demo4-validation-advection-scheme")?.value || "upwind";
  await api("/api/demo4/validate", {
    method: "POST",
    body: JSON.stringify({
      inputDir: demo4SelectedInput("validation"),
      cases,
      kind,
      advectionScheme,
      maxEndT: Number(document.getElementById("demo4-validation-max-endt")?.value || 0.00008),
      variants: isDt
        ? ["baseline", "dt_refined"]
        : ["baseline", "ny_refined_1", "ny_refined_2", "ny_refined_3", "ny_refined"],
      dtRefine: isDt ? Number(document.getElementById("demo4-validation-dt-refine")?.value || 4) : 1,
      nyRefine: Number(document.getElementById("demo4-validation-ny-refine")?.value || 2),
      timeoutSeconds: Number(document.getElementById("demo4-validation-timeout")?.value || 300),
    }),
  });
  const schemeLabel = advectionScheme === "tvd-mc" ? "High-Resolution (TVD-MC)" : "Baseline Upwind";
  setDemo4Message(`demo4 ${isDt ? "dt-refine" : "ny-refine"} validation started with ${schemeLabel}.`);
  await refreshDemo4Status();
}

async function handleDemo4Report() {
  await api("/api/demo4/report", { method: "POST", body: JSON.stringify({}) });
  setDemo4Message("demo4 summary task started.");
  await refreshDemo4Status();
}

async function handleDemo4Build() {
  await api("/api/demo4/build", { method: "POST", body: JSON.stringify({}) });
  setDemo4Message("demo4 solver build started.");
  await refreshDemo4Status();
}

async function handleDemo4Stop() {
  await api("/api/demo4/stop", { method: "POST", body: JSON.stringify({}) });
  setDemo4Message("Requested demo4 task stop.");
  await refreshDemo4Status();
}

function renderEnv(env) {
  state.env = env;
  envGridEl.replaceChildren();
  showMessage(
    env.canRun
      ? "Build environment is ready. Save a case, then build and run."
      : "Build environment is not fully ready. You can still edit cases and inspect existing results."
  );

  env.checks.forEach((item) => {
    const article = document.createElement("article");
    article.className = `status-card ${item.ok ? "ok" : "bad"}`;

    const label = document.createElement("span");
    label.className = "stat-label";
    label.textContent = item.label;

    const badge = document.createElement("strong");
    badge.className = `badge status-badge ${item.ok ? "ok" : "bad"}`;
    badge.textContent = item.ok ? "Ready" : "Missing";

    const details = document.createElement("p");
    details.textContent = item.details;

    const body = document.createElement("div");
    body.append(label, details);

    article.append(badge, body);
    envGridEl.appendChild(article);
  });

  buildRunBtnEl.disabled = !env.canRun || state.currentCaseId === null;
  batchRunBtnEl.disabled = !env.canRun;
  ensureWarmupCandidates();
  renderWarmup(state.warmupStatus || {});
}

function renderCases() {
  caseListEl.replaceChildren();
  renderCaseListMeta();
  renderBatchRunScope();
  if (!state.cases.length) {
    const empty = document.createElement("div");
    empty.className = "empty-state";
    empty.textContent = state.caseQuery
      ? "No matching cases. Clear the search or enter an existing case id."
      : "No case files found. Create a new case first.";
    caseListEl.appendChild(empty);
  } else {
    state.cases.forEach((item) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = `case-item ${item.id === state.currentCaseId ? "active" : ""}`;

      const body = document.createElement("span");
      body.className = "case-item-body";

      const title = document.createElement("strong");
      title.className = "case-item-title";
      title.textContent = `Case ${item.id}`;

      const path = document.createElement("small");
      path.className = "case-item-path";
      path.textContent = item.path;

      body.append(title, path);

      const modifiedAt = document.createElement("span");
      modifiedAt.className = "case-item-date";
      modifiedAt.textContent = item.modifiedAt || "";

      button.append(body, modifiedAt);
      button.addEventListener("click", () => loadCase(item.id));
      caseListEl.appendChild(button);
    });
  }

  loadMoreCasesBtnEl.classList.toggle("hidden", !state.caseHasMore);
  loadMoreCasesBtnEl.disabled = !state.caseHasMore;
  ensureWarmupCandidates();
  renderWarmup(state.warmupStatus || {});
}

function renderCaseListMeta() {
  const shown = state.cases.length;
  if (state.caseTotal === 0) {
    caseListMetaEl.textContent = state.caseQuery ? "0 matching results" : "0 cases";
    return;
  }
  const querySuffix = state.caseQuery ? ` · search: ${state.caseQuery}` : "";
  caseListMetaEl.textContent = `Showing ${shown} / ${state.caseTotal}${querySuffix}`;
}

function renderBatchRunScope() {
  if (!state.caseQuery) {
    batchRunScopeEl.textContent = `Batch scope: all cases (${state.caseTotal})`;
    return;
  }
  batchRunScopeEl.textContent = `Batch scope: search "${state.caseQuery}" (${state.caseTotal})`;
}

function warmupCaseCount() {
  return state.caseTotal || state.cases.length || 0;
}

function autoWarmupCandidates() {
  const logical = Math.max(1, Number(state.env?.logicalProcessors || navigator.hardwareConcurrency || 1));
  const candidates = new Set([1]);
  for (let value = 2; value <= logical; value *= 2) {
    candidates.add(value);
  }
  [Math.round(logical * 0.75), logical - 2, logical - 1, logical].forEach((value) => {
    if (value >= 1) candidates.add(value);
  });
  return Array.from(candidates)
    .filter((value) => value >= 1 && value <= logical)
    .sort((a, b) => a - b);
}

function ensureWarmupCandidates() {
  if (!warmupCandidatesInputEl || state.warmupCandidatesTouched) {
    return;
  }
  warmupCandidatesInputEl.value = autoWarmupCandidates().join(",");
}

function parseWarmupCandidates() {
  const source = warmupCandidatesInputEl?.value || "";
  const logical = Math.max(1, Number(state.env?.logicalProcessors || navigator.hardwareConcurrency || 1));
  const values = source
    .split(/[,\s]+/)
    .map((token) => Number(token.trim()))
    .filter((value) => Number.isInteger(value) && value >= 1 && value <= logical);
  return Array.from(new Set(values)).sort((a, b) => a - b);
}

function setWarmupError(message) {
  if (!warmupErrorEl) {
    return;
  }
  if (!message) {
    warmupErrorEl.classList.add("hidden");
    warmupErrorEl.textContent = "";
    return;
  }
  warmupErrorEl.classList.remove("hidden");
  warmupErrorEl.textContent = message;
}

function isSkipWarmupEnabled() {
  return Boolean(warmupSkipInputEl && warmupSkipInputEl.checked);
}

function persistSkipWarmup() {
  if (!warmupSkipInputEl) return;
  try {
    localStorage.setItem(SKIP_WARMUP_STORAGE_KEY, isSkipWarmupEnabled() ? "1" : "0");
  } catch (_) {
    // Local storage can be unavailable in private contexts.
  }
}

function restoreSkipWarmup() {
  if (!warmupSkipInputEl) return;
  try {
    warmupSkipInputEl.checked = localStorage.getItem(SKIP_WARMUP_STORAGE_KEY) === "1";
  } catch (_) {
    warmupSkipInputEl.checked = false;
  }
}

function manualWarmupConcurrency() {
  if (!warmupManualInputEl) return null;
  const raw = warmupManualInputEl.value.trim();
  if (raw === "") return null;
  const value = Number(raw);
  if (!Number.isInteger(value) || value < 0) {
    throw new Error("Manual concurrency must be an integer >= 0.");
  }
  return value;
}

async function applyDefaultRunSettings() {
  await api("/api/warmup/apply", {
    method: "POST",
    body: JSON.stringify({ concurrency: 0 }),
  });
  await refreshEnv();
}

async function ensureDefaultRunSettings() {
  if (!isSkipWarmupEnabled()) return;
  if (!state.env || Number(state.env.numThreads || 0) !== 0) {
    await applyDefaultRunSettings();
  }
}

async function handleSkipWarmupChange() {
  persistSkipWarmup();
  if (!isSkipWarmupEnabled()) {
    renderWarmup(state.warmupStatus || {});
    return;
  }
  state.selectedWarmupConcurrency = null;
  if (warmupManualInputEl) warmupManualInputEl.value = "";
  await applyDefaultRunSettings();
  await refreshWarmupStatus();
  setWarmupError("");
}

function formatThroughput(value) {
  const number = Number(value || 0);
  if (!Number.isFinite(number)) return "-";
  if (number >= 100) return `${number.toFixed(1)} iter/s`;
  return `${number.toFixed(2)} iter/s`;
}

function formatEstimatedMakespan(value) {
  const number = Number(value);
  if (!Number.isFinite(number) || number <= 0) return "-";
  if (number < 1) return `${(number * 1000).toFixed(0)} ms`;
  if (number < 60) return `${number.toFixed(2)} s`;
  const minutes = Math.floor(number / 60);
  const seconds = number - minutes * 60;
  return `${minutes}m ${seconds.toFixed(0)}s`;
}

function renderWarmup(status = {}) {
  state.warmupStatus = status;
  const skipWarmup = isSkipWarmupEnabled();
  let manualValue = null;
  try {
    manualValue = manualWarmupConcurrency();
  } catch (_) {
    manualValue = null;
  }
  const logical = status.logicalProcessors ?? state.env?.logicalProcessors ?? navigator.hardwareConcurrency ?? "-";
  const numThreads = status.numThreads ?? state.env?.numThreads ?? 0;
  const count = (status.caseIds && status.caseIds.length) || warmupCaseCount();
  const leader = status.leader || null;
  const results = status.results || [];

  if (warmupLogicalEl) warmupLogicalEl.textContent = String(logical || "-");
  if (warmupNumThreadsEl) warmupNumThreadsEl.textContent = numThreads === 0 ? "0 auto" : String(numThreads);
  if (warmupCaseCountEl) warmupCaseCountEl.textContent = String(count || 0);
  if (warmupLeaderEl) {
    warmupLeaderEl.textContent = skipWarmup
      ? "skipped, default auto"
      : leader
        ? `concurrency ${leader.concurrency}, estimated p90 tail ${formatEstimatedMakespan(leader.estimatedMakespan)}`
        : "Not run yet";
  }

  if (warmupResultsBodyEl) {
    warmupResultsBodyEl.replaceChildren();
    if (!results.length) {
      const row = document.createElement("tr");
      const cell = document.createElement("td");
      cell.colSpan = 9;
      cell.textContent = "No warmup results yet.";
      row.appendChild(cell);
      warmupResultsBodyEl.appendChild(row);
    } else {
      results.forEach((item) => {
        const row = document.createElement("tr");
        row.className = !skipWarmup && item.concurrency === state.selectedWarmupConcurrency ? "warmup-row-selected" : "";
        row.addEventListener("click", () => {
          if (isSkipWarmupEnabled()) return;
          state.selectedWarmupConcurrency = item.concurrency;
          if (warmupManualInputEl) warmupManualInputEl.value = "";
          renderWarmup(state.warmupStatus || {});
        });

        [
          item.concurrency,
          formatThroughput(item.iterationsPerSecond),
          formatThroughput(item.workerThroughputMin),
          formatThroughput(item.workerThroughputMedian),
          formatThroughput(item.workerThroughputP90),
          formatThroughput(item.workerThroughputSlowest),
          formatEstimatedMakespan(item.estimatedMakespan),
          String(item.sampleCaseCount || "-"),
          item.status || "-",
        ].forEach((value) => {
          const cell = document.createElement("td");
          cell.textContent = String(value);
          row.appendChild(cell);
        });
        warmupResultsBodyEl.appendChild(row);
      });
    }
  }

  if (warmupLogEl) {
    const log = status.log || [];
    warmupLogEl.textContent = skipWarmup
      ? "Warmup is skipped. Later runs will use NUM_THREADS=0 auto."
      : (log.length ? log.join("\n") : "Waiting to start warmup.");
    warmupLogEl.scrollTop = warmupLogEl.scrollHeight;
  }

  const running = status.status === "running";
  if (startWarmupBtnEl) startWarmupBtnEl.disabled = skipWarmup || running || !(state.env && state.env.canRun);
  if (stopWarmupBtnEl) stopWarmupBtnEl.disabled = !running;
  if (applyWarmupBtnEl) applyWarmupBtnEl.disabled = skipWarmup || running || (manualValue === null && !state.selectedWarmupConcurrency);
  if (warmupSkipInputEl) warmupSkipInputEl.disabled = running;
  if (warmupManualInputEl) warmupManualInputEl.disabled = skipWarmup || running;
  [warmupCandidatesInputEl, warmupSecondsInputEl, warmupWarmupSecondsInputEl].forEach((input) => {
    if (input) input.disabled = skipWarmup || running;
  });

  if (running) {
    if (!state.warmupPollHandle) {
      state.warmupPollHandle = window.setInterval(refreshWarmupStatus, 2000);
    }
  } else if (state.warmupPollHandle) {
    window.clearInterval(state.warmupPollHandle);
    state.warmupPollHandle = null;
  }
}

async function refreshWarmupStatus() {
  const status = await api("/api/warmup/status");
  renderWarmup(status);
}

async function handleStartWarmup() {
  if (isSkipWarmupEnabled()) {
    setWarmupError("Warmup is skipped. Uncheck the default-run option to benchmark candidates.");
    return;
  }
  setWarmupError("");
  const candidates = parseWarmupCandidates();
  if (!candidates.length) {
    throw new Error("Enter at least one valid concurrency candidate.");
  }
  state.selectedWarmupConcurrency = null;
  await api("/api/warmup/start", {
    method: "POST",
    body: JSON.stringify({
      caseQuery: state.caseQuery,
      candidates,
      seconds: Number(warmupSecondsInputEl?.value || 30),
      warmupSeconds: Number(warmupWarmupSecondsInputEl?.value || 3),
    }),
  });
  selectWorkflowTab("warmup");
  await refreshWarmupStatus();
}

async function handleStopWarmup() {
  await api("/api/warmup/stop", {
    method: "POST",
    body: JSON.stringify({}),
  });
  await refreshWarmupStatus();
}

async function handleApplyWarmup() {
  let manual = null;
  try {
    manual = manualWarmupConcurrency();
  } catch (error) {
    setWarmupError(error.message);
    return;
  }
  const concurrency = manual !== null ? manual : state.selectedWarmupConcurrency ?? state.warmupStatus?.leader?.concurrency;
  if (concurrency === null || concurrency === undefined) {
    throw new Error("Select a benchmark row or enter a manual concurrency value.");
  }
  const result = await api("/api/warmup/apply", {
    method: "POST",
    body: JSON.stringify({ concurrency }),
  });
  const applied = Number(result.numThreads ?? concurrency ?? 0);
  state.selectedWarmupConcurrency = applied;
  setWarmupError("");
  if (state.env) state.env.numThreads = applied;
  if (state.warmupStatus) state.warmupStatus.numThreads = applied;
  renderWarmup(state.warmupStatus || {});
  if (warmupLogEl) {
    warmupLogEl.textContent = `Applied NUM_THREADS=${applied}${applied === 0 ? " auto" : ""}. Rebuild before the next run to make it effective.`;
  }
  Promise.all([refreshEnv(), refreshWarmupStatus()])
    .then(() => {
      if (warmupLogEl) {
        warmupLogEl.textContent = `Applied NUM_THREADS=${applied}${applied === 0 ? " auto" : ""}. Rebuild before the next run to make it effective.`;
      }
    })
    .catch((error) => setWarmupError(error.message));
}

function ensureCaseInLoadedList(data) {
  if (!data.path || !data.modifiedAt) {
    return;
  }
  const summary = {
    id: data.id,
    path: data.path,
    canonicalPath: data.canonicalPath,
    modifiedAt: data.modifiedAt,
  };
  const existingIndex = state.cases.findIndex((item) => item.id === data.id);
  if (existingIndex >= 0) {
    state.cases[existingIndex] = summary;
    return;
  }
  state.cases.unshift(summary);
}

function fillCaseForm(data) {
  state.currentCase = data;
  state.currentCaseId = data.id;
  caseIdInputEl.value = String(data.id);
  PARAM_FIELDS.forEach((field) => {
    const input = document.querySelector(`[data-field="${field}"]`);
    if (input) {
      input.value = data[field] ?? "";
    }
  });
  currentCaseLabelEl.textContent = `Case ${data.id}`;
  saveHintEl.textContent = `Save path: ${data.canonicalPath}`;
  buildRunBtnEl.disabled = !(state.env && state.env.canRun);
  ensureCaseInLoadedList(data);
  markCaseFormClean();
  renderCases();
}

function nextCaseId() {
  const highest = state.cases.reduce(
    (max, item) => Math.max(max, item.id),
    Math.max(state.caseMaxId, state.currentCaseId || 0)
  );
  return highest + 1;
}

function collectCasePayload() {
  const payload = {};
  if (state.currentCase?.runtime) {
    payload.runtime = state.currentCase.runtime;
  }
  PARAM_FIELDS.forEach((field) => {
    const input = document.querySelector(`[data-field="${field}"]`);
    payload[field] = input.value;
  });
  return payload;
}

function caseFormSnapshot() {
  return JSON.stringify({
    id: Number(caseIdInputEl.value),
    payload: collectCasePayload(),
  });
}

function markCaseFormClean() {
  state.caseFormBaseline = caseFormSnapshot();
  state.caseFormDirty = false;
}

function updateCaseFormDirty() {
  if (!state.caseFormBaseline) {
    state.caseFormDirty = true;
    return;
  }
  state.caseFormDirty = caseFormSnapshot() !== state.caseFormBaseline;
}

async function refreshEnv() {
  const env = await api("/api/env");
  renderEnv(env);
}

function buildCasesUrl(offset) {
  const params = new URLSearchParams({
    offset: String(offset),
    limit: String(state.caseLimit),
  });
  if (state.caseQuery) {
    params.set("q", state.caseQuery);
  }
  return `/api/cases?${params.toString()}`;
}

function mergeCaseItems(items) {
  items.forEach((item) => {
    const existingIndex = state.cases.findIndex((caseItem) => caseItem.id === item.id);
    if (existingIndex >= 0) {
      state.cases[existingIndex] = item;
    } else {
      state.cases.push(item);
    }
  });
}

async function loadCasePage({ reset = false } = {}) {
  const offset = reset ? 0 : state.caseOffset;
  const payload = await api(buildCasesUrl(offset));
  state.caseTotal = payload.total || 0;
  state.caseHasMore = Boolean(payload.hasMore);
  state.caseMaxId = Math.max(state.caseMaxId, payload.maxId || 0);
  state.caseOffset = offset + (payload.items || []).length;
  if (reset) {
    state.cases = payload.items || [];
  } else {
    mergeCaseItems(payload.items || []);
  }
  renderCases();
}

async function refreshCases() {
  state.caseQuery = caseSearchInputEl.value.trim();
  state.caseOffset = 0;
  await loadCasePage({ reset: true });
}

async function loadMoreCases() {
  if (!state.caseHasMore) {
    return;
  }
  await loadCasePage();
}

async function loadCase(caseId) {
  const data = await api(`/api/cases/${caseId}`);
  fillCaseForm(data);
  await refreshResults();
}

async function saveCurrentCase() {
  const caseId = Number(caseIdInputEl.value);
  if (!caseId || caseId < 1) {
    throw new Error("Case ID must be an integer greater than 0.");
  }
  const payload = collectCasePayload();
  await api(`/api/cases/${caseId}`, {
    method: "PUT",
    body: JSON.stringify(payload),
  });
  await refreshCases();
  await loadCase(caseId);
}

const KNOWN_STATES = ["idle", "building", "running", "finished", "failed", "stopped"];

function applyStateClass(el, value) {
  el.classList.add("badge", "state-badge");
  KNOWN_STATES.forEach((s) => el.classList.remove(`state-${s}`));
  if (value && KNOWN_STATES.includes(value)) {
    el.classList.add(`state-${value}`);
  }
}

function paintExit(el, code) {
  if (el.parentElement) {
    el.parentElement.classList.toggle("hidden", code === null || code === undefined || code === 0);
  }
  el.textContent = code ?? "-";
  el.classList.add("badge", "exit-badge");
  el.classList.remove("exit-ok", "exit-bad");
  if (code === 0) {
    el.classList.add("exit-ok");
  } else if (typeof code === "number" && code !== 0) {
    el.classList.add("exit-bad");
  }
}

// ===========================================================================
// Stage parsing + overview rendering
// ===========================================================================

const RUN_PROGRESS_RE = /\[Case\s+(\d+)\]\[([^\]]+)\]\s+(\d+(?:\.\d+)?)%\s*\|\s*eta=([\d.eE+-]+)\s*\|\s*err=([\d.eE+-]+)\s*\|\s*used\s+([\d.]+)s/;
const CASE_COMPLETION_RE = /\[Case\s+(\d+)\]\[[^\]]+\]\s+(?:Converged!|Finished\b)/i;
const COMPILE_PERCENT_RE = /^\[\s*(\d+)%\]/;
const THREADS_RE = /OpenMP\s+Threads\s*:\s*(\d+)/i;
const ANSI_ESCAPE_RE = /\x1b\[[0-?]*[ -/]*[@-~]/g;

const STAGE_DEFS = [
  { key: "prepare",   label: "Prepare",   weight: 0.05 },
  { key: "configure", label: "Configure", weight: 0.15 },
  { key: "compile",   label: "Compile",   weight: 0.40 },
  { key: "run",       label: "Run",       weight: 0.40 },
];

// eta_eq = K0 / (K0 + 1). Batch runs use the currently loaded case as an approximation.
function getCurrentEtaEq() {
  const k0Input = document.querySelector('[data-field="K0"]');
  if (!k0Input) return null;
  const k0 = parseFloat(k0Input.value);
  if (!isFinite(k0) || k0 <= 0) return null;
  return k0 / (k0 + 1);
}

function parseStages(buildLog, runLog, status) {
  const stages = {};
  STAGE_DEFS.forEach((def) => {
    stages[def.key] = { ...def, state: "pending", percent: 0 };
  });

  let prepareSeen = false;
  let configureStart = -1, configureEnd = -1;
  let compileStart = -1, compileEnd = -1;
  let latestCompilePercent = 0;

  for (let i = 0; i < buildLog.length; i++) {
    const line = buildLog[i];
    if (/^\[info\]\s+Preparing\b/i.test(line)) prepareSeen = true;  // matches both "Preparing build for case X" and "Preparing batch build for N cases"
    if (/^\[info\]\s+Running:.*cmake\s+-S/.test(line) && configureStart < 0) configureStart = i;
    if (/^--\s+Build files have been written to/.test(line)) configureEnd = i;
    if (/^\[info\]\s+Running:.*cmake\s+--build/.test(line) && compileStart < 0) compileStart = i;
    if (/Built\s+target/i.test(line)) compileEnd = i;
    const m = line.match(COMPILE_PERCENT_RE);
    if (m) {
      const p = parseInt(m[1], 10);
      if (p > latestCompilePercent) latestCompilePercent = p;
    }
  }

  const buildFailed = typeof status.buildExitCode === "number" && status.buildExitCode !== 0;
  const runFailed = typeof status.runExitCode === "number" && status.runExitCode !== 0;
  const overall = status.status || "idle";

  // Prepare
  if (prepareSeen) stages.prepare.state = "done";
  else if (overall !== "idle") stages.prepare.state = "active";

  // Configure
  if (configureStart >= 0) {
    if (configureEnd >= 0) {
      stages.configure.state = "done";
      stages.configure.percent = 100;
    } else if (buildFailed) {
      stages.configure.state = "failed";
    } else {
      stages.configure.state = "active";
    }
  }

  // Compile
  if (compileStart >= 0) {
    if (compileEnd >= 0) {
      stages.compile.state = "done";
      stages.compile.percent = 100;
    } else if (buildFailed) {
      stages.compile.state = "failed";
      stages.compile.percent = latestCompilePercent;
    } else {
      stages.compile.state = "active";
      stages.compile.percent = latestCompilePercent;
    }
  } else if (buildFailed && configureEnd >= 0) {
    // Build failed before compile started; odd but possible.
    stages.compile.state = "failed";
  }

  // Run: prefer eta-based percent (eta / eta_eq) over step %.
  const etaEq = getCurrentEtaEq();
  let latestRunStepPct = 0;
  let latestRunEta = null;
  let runSawProgress = false;
  for (let i = runLog.length - 1; i >= 0; i--) {
    const m = runLog[i].match(RUN_PROGRESS_RE);
    if (m) {
      latestRunStepPct = parseFloat(m[3]);
      latestRunEta = parseFloat(m[4]);
      runSawProgress = true;
      break;
    }
  }
  let runPctEffective = latestRunStepPct;
  let runPctIsEta = false;
  if (etaEq !== null && etaEq > 0 && latestRunEta !== null && isFinite(latestRunEta)) {
    runPctEffective = Math.max(0, Math.min(100, (latestRunEta / etaEq) * 100));
    runPctIsEta = true;
  }
  stages.run.isEta = runPctIsEta;

  const runStarted = compileEnd >= 0 || runLog.length > 0 || overall === "running";
  if (runStarted && stages.compile.state === "done") {
    if (overall === "finished" || (typeof status.runExitCode === "number" && status.runExitCode === 0)) {
      stages.run.state = "done";
      stages.run.percent = 100;
    } else if (runFailed || (overall === "failed" && stages.compile.state === "done")) {
      stages.run.state = "failed";
      stages.run.percent = runPctEffective;
    } else if (overall === "running" || runSawProgress) {
      stages.run.state = "active";
      stages.run.percent = runPctEffective;
    }
  }

  // Stopped: anything still active becomes cancelled
  if (overall === "stopped") {
    Object.keys(stages).forEach((k) => {
      if (stages[k].state === "active") stages[k].state = "cancelled";
    });
  }

  return stages;
}

function parseRunProgress(runLog) {
  const history = [];
  let latest = null;
  for (const line of runLog) {
    const m = line.match(RUN_PROGRESS_RE);
    if (m) {
      const sample = {
        case: parseInt(m[1], 10),
        backend: m[2],
        percent: parseFloat(m[3]),
        eta: parseFloat(m[4]),
        err: parseFloat(m[5]),
        used: parseFloat(m[6]),
      };
      latest = sample;
      history.push({ percent: sample.percent, err: sample.err, used: sample.used });
    }
  }
  if (history.length > 240) history.splice(0, history.length - 240);

  let threads = null;
  for (const line of runLog) {
    const m = line.match(THREADS_RE);
    if (m) { threads = parseInt(m[1], 10); break; }
  }
  return { latest, history, threads };
}

function cleanLogLine(line) {
  return String(line || "").replace(ANSI_ESCAPE_RE, "").trimEnd();
}

function deriveBatchStatus(status, runLog) {
  if (status.mode !== "batch" || !(status.totalCases > 1)) {
    return status;
  }
  const completedCaseIds = new Set();
  runLog.forEach((line) => {
    const m = line.match(CASE_COMPLETION_RE);
    if (m) {
      completedCaseIds.add(Number(m[1]));
    }
  });
  const completedCases = Math.max(status.completedCases || 0, completedCaseIds.size);
  if (completedCases === (status.completedCases || 0)) {
    return status;
  }
  return { ...status, completedCases };
}

function computeOverallProgress(stages) {
  let total = 0;
  Object.values(stages).forEach((s) => {
    if (s.state === "done") total += s.weight * 100;
    else if (s.state === "active") total += s.weight * (s.percent || 0);
  });
  return Math.max(0, Math.min(100, Math.round(total)));
}

function stageIcon(stateName) {
  switch (stateName) {
    case "done": return "OK";
    case "failed": return "!";
    case "cancelled": return "-";
    case "active": return "...";
    default: return ".";
  }
}

function formatStagePercent(p) {
  if (!isFinite(p) || p <= 0) return "";
  return p < 10 ? p.toFixed(1) : Math.round(p).toString();
}

function stageStatusText(s) {
  switch (s.state) {
    case "pending": return "pending";
    case "active":
      if (s.key === "compile") {
        const v = formatStagePercent(s.percent);
        return v ? `${v}%` : "running";
      }
      if (s.key === "run") {
        const v = formatStagePercent(s.percent);
        if (!v) return "running";
        return s.isEta ? `eta ${v}%` : `step ${v}%`;
      }
      return "running";
    case "done": return "done";
    case "failed": {
      if (s.key === "run" && s.percent > 0) {
        const v = formatStagePercent(s.percent);
        return s.isEta ? `failed · eta ${v}%` : `failed · step ${v}%`;
      }
      return "failed";
    }
    case "cancelled": {
      if (s.key === "run" && s.percent > 0) {
        const v = formatStagePercent(s.percent);
        return s.isEta ? `cancelled · eta ${v}%` : `cancelled · step ${v}%`;
      }
      return "cancelled";
    }
    default: return "";
  }
}

function renderStageChips(stages) {
  stageChipsEl.replaceChildren();
  STAGE_DEFS.forEach((def) => {
    const s = stages[def.key];
    const chip = document.createElement("div");
    chip.className = `stage-chip stage-chip--${s.state}`;

    const icon = document.createElement("span");
    icon.className = "stage-chip-icon";
    icon.textContent = stageIcon(s.state);

    const body = document.createElement("div");
    body.className = "stage-chip-body";

    const name = document.createElement("span");
    name.className = "stage-chip-name";
    name.textContent = s.label;

    const status = document.createElement("span");
    status.className = "stage-chip-status";
    status.textContent = stageStatusText(s);

    body.append(name, status);
    chip.append(icon, body);

    if (s.state === "active" && (s.key === "compile" || s.key === "run") && s.percent > 0) {
      const bar = document.createElement("div");
      bar.className = "stage-chip-bar";
      const fill = document.createElement("div");
      fill.className = "stage-chip-bar-fill";
      fill.style.width = `${Math.min(100, s.percent)}%`;
      bar.appendChild(fill);
      chip.appendChild(bar);
    }
    stageChipsEl.appendChild(chip);
  });
}

// Find the stage where a failed/stopped task got stuck.
function findStuckStage(stages) {
  for (const def of STAGE_DEFS) {
    const s = stages[def.key];
    if (s.state === "failed" || s.state === "cancelled") return s;
  }
  // Fallback: last non-pending stage
  let last = null;
  for (const def of STAGE_DEFS) {
    const s = stages[def.key];
    if (s.state !== "pending") last = s;
  }
  return last;
}

function renderOverallProgress(stages, status) {
  const rawPct = computeOverallProgress(stages);
  const overallStatus = status.status || "idle";
  const showPercent = overallStatus === "running" || overallStatus === "building" || overallStatus === "finished";

  // Bar width still reflects achieved progress (informative even when failed)
  const barWidth = overallStatus === "idle" ? 0 : (overallStatus === "finished" ? 100 : rawPct);
  overallProgressFillEl.style.width = `${barWidth}%`;

  // Color
  overallProgressFillEl.classList.remove(
    "overall-progress-fill--ok",
    "overall-progress-fill--bad",
    "overall-progress-fill--running",
    "overall-progress-fill--stopped"
  );
  if (overallStatus === "finished") overallProgressFillEl.classList.add("overall-progress-fill--ok");
  else if (overallStatus === "failed") overallProgressFillEl.classList.add("overall-progress-fill--bad");
  else if (overallStatus === "stopped") overallProgressFillEl.classList.add("overall-progress-fill--stopped");
  else if (overallStatus === "running" || overallStatus === "building") {
    overallProgressFillEl.classList.add("overall-progress-fill--running");
  }

  // Label + percent text
  let label;
  switch (overallStatus) {
    case "idle":     label = "Not started"; break;
    case "building": label = "Building"; break;
    case "running": {
      const active = STAGE_DEFS.map((d) => d.key).find((k) => stages[k].state === "active");
      if (active === "run" && stages.run.isEta) label = "Running · eta convergence";
      else if (active) label = `Running · ${stages[active].label}`;
      else label = "Running";
      break;
    }
    case "finished": label = "Finished"; break;
    case "failed": {
      const stuck = findStuckStage(stages);
      label = stuck ? `Failed · ${stuck.label}` : "Failed";
      break;
    }
    case "stopped": {
      const stuck = findStuckStage(stages);
      label = stuck ? `Stopped · ${stuck.label}` : "Stopped";
      break;
    }
    default: label = "Not started";
  }
  overallProgressLabelEl.textContent = label;

  // Overall percent is intentionally minimal:
  // - running/building: blank (the bar shows visual progress; per-stage chips have meaningful %)
  // - finished: 100%
  // - idle/failed/stopped: blank (label already conveys the state)
  // The Run stage chip's "eta X%" is the single source of truth for run-phase progress.
  if (overallStatus === "finished") {
    overallProgressPercentEl.textContent = "100%";
  } else {
    overallProgressPercentEl.textContent = "";
  }
}

function renderRunStatusBanner(stages, status) {
  let cls = "run-status-banner--idle";
  let text = "No runs started yet.";
  switch (status.status) {
    case "running":
    case "building": {
      cls = "run-status-banner--running";
      const active = STAGE_DEFS.map((d) => d.key).find((k) => stages[k].state === "active");
      const name = active ? stages[active].label : (status.stage || "running");
      text = `Running · ${name}`;
      if (active === "run" && stages.run.percent > 0) {
        const v = stages.run.percent;
        const num = v < 10 ? v.toFixed(1) : Math.round(v).toString();
        text += stages.run.isEta ? ` · eta convergence ${num}%` : ` · ${num}%`;
      } else if (active === "compile" && stages.compile.percent > 0) {
        text += ` · ${stages.compile.percent}%`;
      }
      // Batch tag: "(batch k/N)" so user sees their position in the queue
      if (status.mode === "batch" && (status.totalCases || 0) > 1) {
        const pos = (status.completedCases || 0) + 1;
        text += ` · batch ${pos}/${status.totalCases}`;
      }
      break;
    }
    case "finished":
      cls = "run-status-banner--ok";
      text = "Build and run finished.";
      break;
    case "failed": {
      cls = "run-status-banner--bad";
      const stuck = findStuckStage(stages);
      const where = stuck ? ` · stuck at ${stuck.label}` : "";
      text = `Task failed${where}` + (status.error ? ` · ${status.error}` : "");
      break;
    }
    case "stopped": {
      cls = "run-status-banner--stopped";
      const stuck = findStuckStage(stages);
      const where = stuck ? ` · stopped at ${stuck.label}` : "";
      text = `Task stopped${where}`;
      break;
    }
  }
  runStatusBannerEl.className = `run-status-banner ${cls}`;
  runStatusBannerEl.textContent = text;
}

function formatNumeric(n) {
  if (n === null || n === undefined || !isFinite(n)) return "-";
  if (n === 0) return "0";
  const abs = Math.abs(n);
  if (abs >= 1) return n.toFixed(4);
  if (abs >= 0.001) return n.toFixed(5);
  return n.toExponential(2);
}

function formatErr(n) {
  if (n === null || n === undefined || !isFinite(n)) return "-";
  if (n === 0) return "0";
  const abs = Math.abs(n);
  if (abs >= 1) return n.toFixed(3);
  if (abs >= 0.0001) return n.toFixed(5);
  return n.toExponential(2);
}

function formatDuration(seconds) {
  if (seconds === null || seconds === undefined || !isFinite(seconds)) return "-";
  if (seconds < 60) return `${seconds.toFixed(1)}s`;
  const totalMin = Math.floor(seconds / 60);
  const remSec = seconds - totalMin * 60;
  if (totalMin < 60) return `${totalMin}m ${remSec.toFixed(0)}s`;
  const h = Math.floor(totalMin / 60);
  const m = totalMin - h * 60;
  return `${h}h ${m}m`;
}

function renderRunKpi(progress, status) {
  if (!progress.latest) {
    runKpiEl.classList.add("hidden");
    return;
  }
  runKpiEl.classList.remove("hidden");

  // Batch context: show "Case N · k/total" so large batches still read clearly.
  const isBatch = status && status.mode === "batch" && (status.totalCases || 0) > 1;
  let caseText = `Case ${progress.latest.case}`;
  if (isBatch) {
    const pos = (status.completedCases || 0) + 1;
    caseText += ` · ${pos}/${status.totalCases}`;
  }
  kpiCaseEl.textContent = caseText;

  const backendText = progress.threads
    ? `${progress.latest.backend} · ${progress.threads}T`
    : progress.latest.backend;
  kpiBackendEl.textContent = backendText;
  kpiEtaEl.textContent = formatNumeric(progress.latest.eta);
  kpiErrEl.textContent = formatErr(progress.latest.err);
  kpiUsedEl.textContent = formatDuration(progress.latest.used);
  drawErrSparkline(progress.history.map((s) => s.err));
}

function drawErrSparkline(errs) {
  if (!kpiErrSparkEl) return;
  const dpr = window.devicePixelRatio || 1;
  const cssW = kpiErrSparkEl.clientWidth || 320;
  const cssH = kpiErrSparkEl.clientHeight || 40;
  if (kpiErrSparkEl.width !== Math.round(cssW * dpr)) {
    kpiErrSparkEl.width = Math.round(cssW * dpr);
    kpiErrSparkEl.height = Math.round(cssH * dpr);
  }
  const ctx = kpiErrSparkEl.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, cssW, cssH);

  const valid = errs.filter((e) => isFinite(e) && e > 0);
  if (valid.length < 2) {
    // Single-point fallback or empty
    ctx.fillStyle = "rgba(15, 118, 110, 0.5)";
    ctx.font = "11px sans-serif";
    ctx.fillText(valid.length === 1 ? "Need more samples" : "No samples yet", 6, 14);
    return;
  }

  const logs = valid.map((e) => Math.log10(e));
  const minL = Math.min(...logs);
  const maxL = Math.max(...logs);
  const range = (maxL - minL) || 1;

  const padX = 4;
  const padY = 4;
  const innerW = cssW - padX * 2;
  const innerH = cssH - padY * 2;

  // Fill area under curve
  ctx.beginPath();
  for (let i = 0; i < logs.length; i++) {
    const x = padX + (i / (logs.length - 1)) * innerW;
    const y = padY + innerH - ((logs[i] - minL) / range) * innerH;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.lineTo(padX + innerW, padY + innerH);
  ctx.lineTo(padX, padY + innerH);
  ctx.closePath();
  ctx.fillStyle = "rgba(15, 118, 110, 0.12)";
  ctx.fill();

  // Stroke line
  ctx.beginPath();
  for (let i = 0; i < logs.length; i++) {
    const x = padX + (i / (logs.length - 1)) * innerW;
    const y = padY + innerH - ((logs[i] - minL) / range) * innerH;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.strokeStyle = "#0f766e";
  ctx.lineWidth = 1.5;
  ctx.stroke();

  // Endpoint dot
  const lastX = padX + innerW;
  const lastY = padY + innerH - ((logs[logs.length - 1] - minL) / range) * innerH;
  ctx.fillStyle = "#0f766e";
  ctx.beginPath();
  ctx.arc(lastX, lastY, 2.5, 0, Math.PI * 2);
  ctx.fill();
}

// ===========================================================================
// Log line classification + rendering
// ===========================================================================

const LOG_PATTERNS = [
  { kind: "error",   re: /\b(error|fatal|FAILED|undefined reference to|cannot find|No such file)\b/i },
  { kind: "warning", re: /\b(warning)\s*:/i },
  { kind: "progress", re: /^\s*\[Case\s+\d+\]\[/ },
  { kind: "compile-progress", re: /^\[\s*\d+%\]/ },
  { kind: "cmake",   re: /^--\s+/ },
  { kind: "info",    re: /^\[info\]/ },
  { kind: "success", re: /\b(Built target|Build succeeded|success|complete)\b/i },
];

function classifyLogLine(line) {
  for (const { kind, re } of LOG_PATTERNS) {
    if (re.test(line)) return kind;
  }
  return "";
}

function renderLog(role, lines) {
  const el = role === "build" ? buildLogEl : runLogEl;
  const summaryEl = document.getElementById(`${role}-log-summary`);
  const onlyErrors = state.logOnlyErrors[role];
  const follow = state.logFollow[role];

  let errCount = 0;
  let warnCount = 0;
  const classified = lines.map((line) => {
    const kind = classifyLogLine(line);
    if (kind === "error") errCount++;
    else if (kind === "warning") warnCount++;
    return { line, kind };
  });

  // Summary
  summaryEl.replaceChildren();
  const totalSpan = document.createElement("span");
  totalSpan.textContent = `${classified.length} lines`;
  summaryEl.append(totalSpan);
  if (errCount > 0) {
    const sep = document.createElement("span");
    sep.textContent = " · ";
    const errSpan = document.createElement("span");
    errSpan.className = "log-summary-err";
    errSpan.textContent = `${errCount} errors`;
    summaryEl.append(sep, errSpan);
  }
  if (warnCount > 0) {
    const sep = document.createElement("span");
    sep.textContent = " · ";
    const warnSpan = document.createElement("span");
    warnSpan.className = "log-summary-warn";
    warnSpan.textContent = `${warnCount} warnings`;
    summaryEl.append(sep, warnSpan);
  }

  const visible = onlyErrors
    ? classified.filter((c) => c.kind === "error" || c.kind === "warning")
    : classified;

  const wasAtBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 24;

  el.replaceChildren();
  visible.forEach((c, idx) => {
    const row = document.createElement("div");
    row.className = `log-line${c.kind ? ` log-line--${c.kind}` : ""}`;
    const num = document.createElement("span");
    num.className = "log-gutter";
    num.textContent = String(idx + 1);
    const txt = document.createElement("span");
    txt.className = "log-text";
    txt.textContent = c.line;
    row.append(num, txt);
    el.appendChild(row);
  });

  if (follow && (wasAtBottom || lines.length > 0)) {
    el.scrollTop = el.scrollHeight;
  }
}

function jumpToFirstError(role) {
  const el = role === "build" ? buildLogEl : runLogEl;
  // Switch to that subtab first
  selectRunSubtab(role);
  const firstErr = el.querySelector(".log-line--error, .log-line--warning");
  if (firstErr) {
    firstErr.scrollIntoView({ behavior: "smooth", block: "center" });
    firstErr.classList.remove("flash");
    void firstErr.offsetWidth; // force reflow to restart animation
    firstErr.classList.add("flash");
  }
}

function copyLogToClipboard(role) {
  const lines = role === "build" ? state.lastBuildLog : state.lastRunLog;
  const text = lines.join("\n");
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).catch(() => fallbackCopy(text));
  } else {
    fallbackCopy(text);
  }
}

function fallbackCopy(text) {
  const ta = document.createElement("textarea");
  ta.value = text;
  ta.style.position = "fixed";
  ta.style.opacity = "0";
  document.body.appendChild(ta);
  ta.select();
  try { document.execCommand("copy"); } catch (_) { /* noop */ }
  document.body.removeChild(ta);
}

function selectRunSubtab(target) {
  state.runSubtab = target;
  runSubtabEls.forEach((tab) => {
    const isActive = tab.dataset.runSubtab === target;
    tab.classList.toggle("active", isActive);
    tab.setAttribute("aria-selected", String(isActive));
  });
  ["overview", "build", "run"].forEach((t) => {
    const panel = document.getElementById(`run-subpanel-${t}`);
    if (panel) panel.classList.toggle("hidden", t !== target);
  });
}

function renderRunOverview(status) {
  const stages = parseStages(state.lastBuildLog, state.lastRunLog, status);
  const progress = parseRunProgress(state.lastRunLog);
  renderStageChips(stages);
  renderOverallProgress(stages, status);
  renderRunStatusBanner(stages, status);
  renderRunKpi(progress, status);
}

function formatStatus(status) {
  const s = status || "idle";
  taskStateBadgeEl.textContent = s;
  applyStateClass(taskStateBadgeEl, s);
}

function renderTaskStatus(status) {
  state.lastBuildLog = (status.buildLog || []).map(cleanLogLine);
  state.lastRunLog = (status.runLog || []).map(cleanLogLine);
  const effectiveStatus = deriveBatchStatus(status, state.lastRunLog);

  formatStatus(status.status);
  runStageEl.textContent = effectiveStatus.stage || "-";
  applyStateClass(runStageEl, effectiveStatus.stage || "idle");
  if (effectiveStatus.mode === "batch") {
    batchProgressEl.textContent = `${effectiveStatus.completedCases || 0} / ${effectiveStatus.totalCases || 0}`;
  } else {
    batchProgressEl.textContent = "-";
  }
  runStartedEl.textContent = effectiveStatus.startedAt || "-";
  runFinishedEl.textContent = effectiveStatus.finishedAt || "-";
  paintExit(buildExitEl, effectiveStatus.buildExitCode);
  paintExit(runExitEl, effectiveStatus.runExitCode);
  renderLog("build", state.lastBuildLog);
  renderLog("run", state.lastRunLog);
  renderRunOverview(effectiveStatus);
  setRunError(effectiveStatus.error || "");
  stopBtnEl.disabled = effectiveStatus.status !== "running";
  batchRunBtnEl.disabled = effectiveStatus.status === "running" || !(state.env && state.env.canRun);
  buildRunBtnEl.disabled = effectiveStatus.status === "running" || !(state.env && state.env.canRun && state.currentCaseId !== null);

  if (effectiveStatus.status === "running") {
    if (!state.pollHandle) {
      // First time we see this run: switch to the run tab once, then start polling.
      // Subsequent polls won't yank the user back if they navigate away.
      selectWorkflowTab("run");
      state.pollHandle = window.setInterval(refreshTaskStatus, 2000);
    }
  } else if (state.pollHandle) {
    window.clearInterval(state.pollHandle);
    state.pollHandle = null;
    if (state.currentCaseId) {
      if (effectiveStatus.status === "finished") {
        selectWorkflowTab("results");
      } else {
        refreshResults().catch((error) => setRunError(error.message));
      }
    }
  }
}

async function refreshTaskStatus() {
  const status = await api("/api/run/status");
  renderTaskStatus(status);
}

function demo4SameCase(row, caseId) {
  return Number(row?.case_id) === Number(caseId);
}

function demo4RowsForCurrentCase(rows, caseId) {
  return (rows || []).filter((row) => demo4SameCase(row, caseId));
}

function demo4RiskText(row) {
  const risk = String(row?.risk_items || "").trim();
  return risk || "none";
}

function demo4ValidationText(rows) {
  if (!rows.length) return "";
  const okCount = rows.filter((row) => Number(row.exit_code) === 0).length;
  const failedCount = rows.length - okCount;
  const reference = rows.find((row) => row.relative_final_eta_vs_dt_ny_refined !== undefined && row.relative_final_eta_vs_dt_ny_refined !== "");
  const relText = reference ? `, rel=${formatValue(reference.relative_final_eta_vs_dt_ny_refined)}` : "";
  return `${okCount}/${rows.length} ok${failedCount ? `, ${failedCount} failed` : ""}${relText}`;
}

function addDemo4SummaryCards(demo4Results, caseId) {
  if (!demo4Results || !caseId) return;

  const precheck = demo4RowsForCurrentCase(demo4Results.precheckRows, caseId)[0];
  if (precheck) {
    addSummaryCard("demo4 risk", demo4RiskText(precheck));
    addSummaryCard(
      "demo4 dt ratios",
      `diff=${formatValue(precheck.ratio_dt_to_diffusion)}, adv=${formatValue(precheck.ratio_dt_to_advection)}, osc=${formatValue(precheck.ratio_dt_to_oscillation)}`
    );
  }

  const validationRows = demo4RowsForCurrentCase(demo4Results.validationRows, caseId);
  const validationText = demo4ValidationText(validationRows);
  if (validationText) {
    addSummaryCard("demo4 validation", validationText);
  }
}

function renderSummary(summary, meta, demo4Results = null) {
  summaryGridEl.replaceChildren();
  const plot = meta.plot || {};
  const rows = [
    ["Case", meta.caseId],
    ["Advection", formatTriple(plot.Pe, plot.Pe2, plot.lam)],
    ["Grid", formatMesh(plot)],
    ["Adsorption zone", formatRange(plot.xpo_l, plot.xpo_r)],
    ["Output", `endT=${formatValue(plot.endT)}, N=${formatValue(plot.total_count)}`],
    ["eta_eq", formatValue(plot.eta_eq)],
    ["Snapshots", String(meta.snapshotCount || 0)],
    ["Files", `${meta.etaAvailable ? "eta OK" : "eta missing"} / ${meta.remarksAvailable ? "remarks OK" : "remarks missing"}`],
  ];

  const compactSummary = summary || {};
  [
    ["Converged", "converged"],
    ["final eta", "final_eta_ave"],
    ["rel error", "final_rel_error"],
    ["Iterations", "actual_iterations"],
    ["Elapsed", "time_total"],
  ].forEach(([label, key]) => {
    if (compactSummary[key] !== undefined) {
      rows.push([label, compactSummary[key]]);
    }
  });

  rows.forEach(([label, value]) => addSummaryCard(label, value));
  addDemo4SummaryCards(demo4Results, meta.caseId);
}

function addSummaryCard(label, value) {
  const row = document.createElement("div");
  row.className = "details-row";

  const labelEl = document.createElement("dt");
  labelEl.textContent = label;

  const valueEl = document.createElement("dd");
  valueEl.textContent = value;

  row.append(labelEl, valueEl);
  summaryGridEl.appendChild(row);
}

async function refreshResults() {
  if (!state.currentCaseId) {
    resultsBodyEl.classList.add("hidden");
    resultsEmptyEl.classList.remove("hidden");
    return;
  }

  const [meta, etaData, snapshotData, demo4Results] = await Promise.all([
    api(`/api/results/${state.currentCaseId}`),
    api(`/api/results/${state.currentCaseId}/eta`),
    api(`/api/results/${state.currentCaseId}/snapshots`),
    api("/api/demo4/results").catch(() => null),
  ]);

  state.resultMeta = meta.plot || {};
  state.etaPoints = etaData.points || [];
  if (demo4Results) {
    renderDemo4Results(demo4Results);
  }
  renderSummary(meta.summary, meta, demo4Results || state.demo4Results);
  populateSnapshots(snapshotData.snapshots || []);

  if (!meta.etaAvailable && !meta.remarksAvailable && !(snapshotData.snapshots || []).length) {
    resultsBodyEl.classList.add("hidden");
    resultsEmptyEl.classList.remove("hidden");
    return;
  }

  resultsEmptyEl.classList.add("hidden");
  resultsBodyEl.classList.remove("hidden");

  const latestCount = meta.latestSnapshot;
  if (latestCount !== null && latestCount !== undefined) {
    snapshotSelectEl.value = String(latestCount);
    await loadSnapshot(latestCount);
  } else {
    state.currentSnapshotCount = null;
    drawEvolutionCharts({ matrix: [], profile: [] });
  }
}

function populateSnapshots(snapshots) {
  snapshotSelectEl.innerHTML = "";
  if (!snapshots.length) {
    const option = document.createElement("option");
    option.textContent = "No snapshots";
    option.value = "";
    snapshotSelectEl.appendChild(option);
    snapshotSelectEl.disabled = true;
    return;
  }

  snapshotSelectEl.disabled = false;
  snapshots.forEach((snapshot) => {
    const option = document.createElement("option");
    option.value = String(snapshot.count);
    option.textContent = `Snapshot ${snapshot.count}`;
    snapshotSelectEl.appendChild(option);
  });
}

async function loadSnapshot(count) {
  if (count === "" || count === null || count === undefined) {
    return;
  }
  const snapshot = await api(`/api/results/${state.currentCaseId}/snapshot/${count}`);
  state.currentSnapshotCount = Number(count);
  drawEvolutionCharts(snapshot);
}

function drawEvolutionCharts(snapshot) {
  const meta = state.resultMeta || {};
  const tNow = snapshotTime(state.currentSnapshotCount, meta);
  const etaSlice = sliceEtaSeries(state.etaPoints, tNow, meta);
  drawHeatmap(document.getElementById("heatmap-canvas"), snapshot.matrix || [], meta, tNow);
  drawLineChart(document.getElementById("profile-chart"), snapshot.profile || [], {
    xKey: "x",
    yKey: "eta",
    color: "#b91c1c",
    xLabel: "x",
    yLabel: "eta",
    xDomain: finiteDomain(meta.xleft, meta.xright),
    yDomain: etaDomain(meta, snapshot.profile || []),
  });
  drawLineChart(document.getElementById("eta-chart"), etaSlice, {
    xKey: "time",
    yKey: "eta",
    color: "#b91c1c",
    xLabel: "t*",
    yLabel: "eta_bar",
    xDomain: finiteDomain(0, meta.endT),
    yDomain: etaDomain(meta, state.etaPoints),
    marker: lastPoint(etaSlice, "time", "eta"),
    referenceY: finiteNumber(meta.eta_eq),
  });
  drawLineChart(document.getElementById("deta-chart"), etaSlice, {
    xKey: "time",
    yKey: "dEtaDt",
    color: "#0f766e",
    xLabel: "t*",
    yLabel: "d eta_bar / dt*",
    xDomain: finiteDomain(0, meta.endT),
    yDomain: paddedDomain(state.etaPoints.map((point) => point.dEtaDt)),
    marker: lastPoint(etaSlice, "time", "dEtaDt"),
  });
}

function setupHiDPI(canvas) {
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const cssW = Math.max(1, Math.round(rect.width));
  const cssH = Math.max(1, Math.round(rect.height));
  canvas.width = cssW * dpr;
  canvas.height = cssH * dpr;
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { ctx, width: cssW, height: cssH };
}

function drawLineChart(canvas, points, options) {
  const {
    xKey,
    yKey,
    color,
    xLabel,
    yLabel,
    xDomain,
    yDomain,
    marker,
    referenceY,
  } = options;
  const { ctx, width, height } = setupHiDPI(canvas);
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, width, height);

  if (!points.length) {
    ctx.fillStyle = "#7a6f60";
    ctx.font = "16px sans-serif";
    ctx.fillText("No data", width / 2 - 30, height / 2);
    return;
  }

  const compact = height < 180;
  const margin = compact
    ? { left: 48, right: 14, top: 12, bottom: 30 }
    : { left: 58, right: 18, top: 16, bottom: 36 };
  const xs = points.map((point) => point[xKey]).filter(isFiniteNumber);
  const ys = points.map((point) => point[yKey]).filter(isFiniteNumber);
  const [minX, maxX] = xDomain || paddedDomain(xs);
  const [minY, maxY] = yDomain || paddedDomain(ys);
  const xSpan = maxX - minX || 1;
  const ySpan = maxY - minY || 1;
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;

  const xFor = (value) => margin.left + ((value - minX) / xSpan) * plotW;
  const yFor = (value) => height - margin.bottom - ((value - minY) / ySpan) * plotH;

  ctx.strokeStyle = "#e2d8c8";
  ctx.lineWidth = 1;
  ctx.font = "11px sans-serif";
  ctx.fillStyle = "#8b7d6a";
  for (let i = 0; i <= 4; i += 1) {
    const x = margin.left + (plotW * i) / 4;
    const y = margin.top + (plotH * i) / 4;
    ctx.beginPath();
    ctx.moveTo(margin.left, y);
    ctx.lineTo(width - margin.right, y);
    ctx.stroke();
    ctx.fillText(formatTick(maxY - (ySpan * i) / 4), 6, y + 4);
    ctx.fillText(formatTick(minX + (xSpan * i) / 4), x - 12, height - 10);
  }

  ctx.beginPath();
  ctx.moveTo(margin.left, margin.top);
  ctx.lineTo(margin.left, height - margin.bottom);
  ctx.lineTo(width - margin.right, height - margin.bottom);
  ctx.stroke();

  if (isFiniteNumber(referenceY) && referenceY >= minY && referenceY <= maxY) {
    const y = yFor(referenceY);
    ctx.strokeStyle = "#2563eb";
    ctx.setLineDash([5, 5]);
    ctx.beginPath();
    ctx.moveTo(margin.left, y);
    ctx.lineTo(width - margin.right, y);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  let hasStarted = false;
  points.forEach((point, index) => {
    if (!isFiniteNumber(point[xKey]) || !isFiniteNumber(point[yKey])) {
      return;
    }
    const x = xFor(point[xKey]);
    const y = yFor(point[yKey]);
    if (!hasStarted || index === 0) {
      ctx.moveTo(x, y);
      hasStarted = true;
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();

  if (marker && isFiniteNumber(marker.x) && isFiniteNumber(marker.y)) {
    ctx.fillStyle = "#111827";
    ctx.beginPath();
    ctx.arc(xFor(marker.x), yFor(marker.y), 4, 0, Math.PI * 2);
    ctx.fill();
  }

  ctx.fillStyle = "#3f352b";
  ctx.font = "12px sans-serif";
  ctx.fillText(xLabel || xKey, width - margin.right - 22, height - 20);
  ctx.save();
  ctx.translate(18, margin.top + 28);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText(yLabel || yKey, 0, 0);
  ctx.restore();
}

function drawHeatmap(canvas, matrix, meta = {}, tNow = null) {
  const { ctx, width, height } = setupHiDPI(canvas);
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, width, height);

  if (!matrix.length || !matrix[0].length) {
    ctx.fillStyle = "#7a6f60";
    ctx.font = "16px sans-serif";
    ctx.fillText("No snapshot matrix", width / 2 - 44, height / 2);
    return;
  }

  const rows = matrix.length;
  const cols = matrix[0].length;
  const values = matrix.flat();
  const min = Math.min(...values);
  const max = Math.max(...values);
  const domain = resultDomain(meta, rows, cols);
  const compact = height < 360;
  const margin = compact
    ? { left: 46, right: 18, top: 42, bottom: 48 }
    : { left: 56, right: 24, top: 52, bottom: 58 };
  const bandH = compact ? 10 : 13;
  const plotX = margin.left;
  const plotY = margin.top + bandH + 6;
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom - bandH * 2 - 12;
  const cellW = plotW / rows;
  const cellH = plotH / cols;

  matrix.forEach((row, rowIndex) => {
    row.forEach((value, colIndex) => {
      const ratio = value;
      ctx.fillStyle = heatColor(ratio);
      const x = plotX + rowIndex * cellW;
      const y = plotY + plotH - (colIndex + 1) * cellH;
      ctx.fillRect(x, y, Math.ceil(cellW), Math.ceil(cellH));
    });
  });

  drawAdsorptionBands(ctx, plotX, plotY, plotW, plotH, bandH, domain, meta);

  ctx.strokeStyle = "#1f2937";
  ctx.lineWidth = 1;
  ctx.strokeRect(plotX, plotY, plotW, plotH);

  ctx.fillStyle = "rgba(28, 26, 24, 0.86)";
  ctx.font = compact ? "11px sans-serif" : "12px sans-serif";
  ctx.fillText(formatTitle(meta), margin.left, 18);
  ctx.fillText(`t* = ${isFiniteNumber(tNow) ? formatValue(tNow) : "-"}`, margin.left, 34);
  ctx.fillText(`c*: min ${formatValue(min)}  max ${formatValue(max)}`, Math.max(margin.left + 150, width - margin.right - 190), 34);

  drawAxisTicks(ctx, plotX, plotY, plotW, plotH, domain);
  drawColorbar(ctx, plotX, height - margin.bottom + 34, plotW);
}

function heatColor(value) {
  const v = Math.max(0, Math.min(1, value));
  const stops = [
    [0.0, [49, 54, 149]],
    [0.25, [69, 117, 180]],
    [0.5, [116, 173, 209]],
    [0.65, [254, 224, 144]],
    [0.82, [244, 109, 67]],
    [1.0, [165, 0, 38]],
  ];
  for (let i = 1; i < stops.length; i += 1) {
    if (v <= stops[i][0]) {
      const [loPos, loColor] = stops[i - 1];
      const [hiPos, hiColor] = stops[i];
      const t = (v - loPos) / (hiPos - loPos || 1);
      const [r, g, b] = loColor.map((channel, index) =>
        Math.round(channel + (hiColor[index] - channel) * t)
      );
      return `rgb(${r}, ${g}, ${b})`;
    }
  }
  return "rgb(165, 0, 38)";
}

function isFiniteNumber(value) {
  return typeof value === "number" && Number.isFinite(value);
}

function finiteNumber(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

function finiteDomain(min, max) {
  const lo = finiteNumber(min);
  const hi = finiteNumber(max);
  if (lo === null || hi === null || lo === hi) {
    return null;
  }
  return [lo, hi];
}

function paddedDomain(values) {
  const finiteValues = values.map(Number).filter(Number.isFinite);
  if (!finiteValues.length) {
    return [0, 1];
  }
  const min = Math.min(...finiteValues);
  const max = Math.max(...finiteValues);
  const span = max - min;
  const pad = span ? span * 0.12 : Math.max(Math.abs(max) * 0.12, 0.01);
  return [min - pad, max + pad];
}

function etaDomain(meta, points) {
  const etaEq = finiteNumber(meta.eta_eq);
  if (etaEq !== null && etaEq > 0) {
    return [0, etaEq * 1.2];
  }
  return paddedDomain(points.map((point) => point.eta));
}

function snapshotTime(count, meta) {
  if (!Number.isFinite(count)) {
    return null;
  }
  const dtOutput = finiteNumber(meta.dt_output);
  if (dtOutput !== null) {
    return count * dtOutput;
  }
  const endT = finiteNumber(meta.endT);
  const totalCount = finiteNumber(meta.total_count);
  if (endT !== null && totalCount) {
    return count * (endT / totalCount);
  }
  return null;
}

function sliceEtaSeries(points, tNow, meta) {
  if (!Array.isArray(points) || !points.length) {
    return [];
  }
  if (!isFiniteNumber(tNow)) {
    return points;
  }
  const tolerance = (finiteNumber(meta.dt_output) || 0) * 0.5;
  return points.filter((point) => point.time <= tNow + tolerance);
}

function lastPoint(points, xKey, yKey) {
  if (!points.length) {
    return null;
  }
  const point = points[points.length - 1];
  return { x: point[xKey], y: point[yKey] };
}

function resultDomain(meta, rows, cols) {
  const xleft = finiteNumber(meta.xleft) ?? 0;
  const xright = finiteNumber(meta.xright) ?? Math.max(rows - 1, 1);
  const yleft = finiteNumber(meta.yleft) ?? 0;
  const yright = finiteNumber(meta.yright) ?? Math.max(cols - 1, 1);
  return { xleft, xright, yleft, yright };
}

function formatValue(value) {
  if (value === null || value === undefined || value === "") {
    return "-";
  }
  const number = Number(value);
  if (!Number.isFinite(number)) {
    return "-";
  }
  const abs = Math.abs(number);
  if ((abs !== 0 && abs < 0.001) || abs >= 10000) {
    return number.toExponential(3);
  }
  return Number(number.toPrecision(5)).toString();
}

function formatScientific(value, digits = 6) {
  if (value === null || value === undefined || value === "") {
    return "-";
  }
  const number = Number(value);
  if (!Number.isFinite(number)) {
    return "-";
  }
  return number.toExponential(digits);
}

function formatTick(value) {
  const number = Number(value);
  if (!Number.isFinite(number)) {
    return "";
  }
  if (Math.abs(number) >= 1000 || (number !== 0 && Math.abs(number) < 0.01)) {
    return number.toExponential(1);
  }
  return Number(number.toFixed(3)).toString();
}

function formatTriple(pe, pe2, lam) {
  return `Pe=${formatValue(pe)}, Pe2=${formatValue(pe2)}, lambda=${formatValue(lam)}`;
}

function formatMesh(meta) {
  return `nx=${formatValue(meta.nx)}, ny=${formatValue(meta.ny)}, h=${formatValue(meta.h)}`;
}

function formatRange(left, right) {
  return `[${formatValue(left)}, ${formatValue(right)}]`;
}

function formatTitle(meta) {
  return `Pe=${formatValue(meta.Pe)}  Pe2=${formatValue(meta.Pe2)}  lambda=${formatValue(meta.lam)}`;
}

function drawAdsorptionBands(ctx, plotX, plotY, plotW, plotH, bandH, domain, meta) {
  const xpoL = finiteNumber(meta.xpo_l);
  const xpoR = finiteNumber(meta.xpo_r);
  const xSpan = domain.xright - domain.xleft || 1;
  const xFor = (x) => plotX + ((x - domain.xleft) / xSpan) * plotW;
  const leftEdge = plotX;
  const rightEdge = plotX + plotW;
  const adsorbLeft = xpoL === null ? plotX + plotW / 3 : xFor(xpoL);
  const adsorbRight = xpoR === null ? plotX + (plotW * 2) / 3 : xFor(xpoR);

  const segments = [
    [leftEdge, adsorbLeft, "#c9c9c9"],
    [adsorbLeft, adsorbRight, "#166534"],
    [adsorbRight, rightEdge, "#c9c9c9"],
  ];
  [plotY - bandH - 8, plotY + plotH + 8].forEach((y) => {
    segments.forEach(([x1, x2, color]) => {
      ctx.fillStyle = color;
      ctx.fillRect(x1, y, Math.max(0, x2 - x1), bandH);
      ctx.strokeStyle = "#111827";
      ctx.strokeRect(x1, y, Math.max(0, x2 - x1), bandH);
    });
  });
}


function drawAxisTicks(ctx, plotX, plotY, plotW, plotH, domain) {
  ctx.fillStyle = "#475569";
  ctx.strokeStyle = "#cbd5e1";
  ctx.font = "11px ui-monospace, SFMono-Regular, Menlo, monospace";
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i += 1) {
    const x = plotX + (plotW * i) / 4;
    const value = domain.xleft + ((domain.xright - domain.xleft) * i) / 4;
    ctx.beginPath();
    ctx.moveTo(x, plotY + plotH);
    ctx.lineTo(x, plotY + plotH + 4);
    ctx.stroke();
    ctx.fillText(formatTick(value), x - 12, plotY + plotH + 18);
  }
  for (let i = 0; i <= 2; i += 1) {
    const y = plotY + plotH - (plotH * i) / 2;
    const value = domain.yleft + ((domain.yright - domain.yleft) * i) / 2;
    ctx.beginPath();
    ctx.moveTo(plotX, y);
    ctx.lineTo(plotX - 4, y);
    ctx.stroke();
    ctx.save();
    ctx.translate(plotX - 8, y + 4);
    ctx.fillText(formatTick(value), -32, 0);
    ctx.restore();
  }
}

function drawColorbar(ctx, x, y, width) {
  const height = 8;
  const steps = 64;
  for (let i = 0; i < steps; i += 1) {
    ctx.fillStyle = heatColor(i / (steps - 1));
    ctx.fillRect(x + (width * i) / steps, y, Math.ceil(width / steps) + 1, height);
  }
  ctx.strokeStyle = "#94a3b8";
  ctx.lineWidth = 1;
  ctx.strokeRect(x, y, width, height);
  ctx.fillStyle = "#475569";
  ctx.font = "10px ui-monospace, monospace";
  ctx.fillText("low", x, y + height + 12);
  ctx.fillText("high", x + width - 22, y + height + 12);
}

async function handleSave() {
  try {
    await saveCurrentCase();
    showMessage(`Case ${state.currentCaseId} saved.`);
  } catch (error) {
    setRunError(error.message);
  }
}

async function handleBuildRun() {
  try {
    if (state.caseFormDirty) {
      await saveCurrentCase();
    }
    setRunError("");
    const force = forceRestartEl ? forceRestartEl.checked : false;
    await api("/api/build-and-run", {
      method: "POST",
      body: JSON.stringify({ caseId: state.currentCaseId, forceRestart: force }),
    });
    await refreshTaskStatus();
  } catch (error) {
    setRunError(error.message);
  }
}

async function handleBatchRun() {
  try {
    setRunError("");
    const force = forceRestartEl ? forceRestartEl.checked : false;
    await api("/api/build-and-run", {
      method: "POST",
      body: JSON.stringify({
        mode: "search",
        caseQuery: state.caseQuery || "",
        forceRestart: force,
      }),
    });
    await refreshTaskStatus();
  } catch (error) {
    setRunError(error.message);
  }
}

async function handleStop() {
  try {
    await api("/api/run/stop", {
      method: "POST",
      body: JSON.stringify({}),
    });
    await refreshTaskStatus();
  } catch (error) {
    setRunError(error.message);
  }
}

function installEvents() {
  let caseSearchTimer = null;
  workflowTabEls.forEach((tab) => {
    tab.addEventListener("click", () => selectWorkflowTab(tab.dataset.tabTarget));
  });
  document.getElementById("refresh-env-btn").addEventListener("click", () => refreshEnv().catch((error) => setRunError(error.message)));
  document.getElementById("refresh-cases-btn").addEventListener("click", () => refreshCases().catch((error) => setRunError(error.message)));
  caseSearchInputEl.addEventListener("input", () => {
    window.clearTimeout(caseSearchTimer);
    caseSearchTimer = window.setTimeout(() => {
      state.caseQuery = caseSearchInputEl.value.trim();
      refreshCases().catch((error) => setRunError(error.message));
    }, 250);
  });
  caseSearchInputEl.addEventListener("keydown", (event) => {
    if (event.key !== "Enter") return;
    event.preventDefault();
    const query = caseSearchInputEl.value.trim();
    state.caseQuery = query;
    if (/^\d+$/.test(query)) {
      loadCase(Number(query)).catch((error) => setRunError(error.message));
      return;
    }
    refreshCases().catch((error) => setRunError(error.message));
  });
  clearCaseSearchBtnEl.addEventListener("click", () => {
    caseSearchInputEl.value = "";
    state.caseQuery = "";
    refreshCases().catch((error) => setRunError(error.message));
  });
  loadMoreCasesBtnEl.addEventListener("click", () => {
    loadMoreCases().catch((error) => setRunError(error.message));
  });
  document.getElementById("case-form").addEventListener("input", updateCaseFormDirty);
  document.getElementById("case-form").addEventListener("change", updateCaseFormDirty);
  document.getElementById("save-case-btn").addEventListener("click", handleSave);
  document.getElementById("build-run-btn").addEventListener("click", handleBuildRun);
  batchRunBtnEl.addEventListener("click", handleBatchRun);
  document.getElementById("stop-btn").addEventListener("click", handleStop);
  previewGenerateBtnEl.addEventListener("click", () => {
    previewGeneratedCases().catch((error) => showGenerateMessage(error.message));
  });
  generateCasesBtnEl.addEventListener("click", () => {
    generateCasesFromForm().catch((error) => showGenerateMessage(error.message));
  });
  if (warmupCandidatesInputEl) {
    warmupCandidatesInputEl.addEventListener("input", () => {
      state.warmupCandidatesTouched = true;
    });
  }
  if (startWarmupBtnEl) {
    startWarmupBtnEl.addEventListener("click", () => {
      handleStartWarmup().catch((error) => setWarmupError(error.message));
    });
  }
  if (stopWarmupBtnEl) {
    stopWarmupBtnEl.addEventListener("click", () => {
      handleStopWarmup().catch((error) => setWarmupError(error.message));
    });
  }
  if (applyWarmupBtnEl) {
    applyWarmupBtnEl.addEventListener("click", () => {
      handleApplyWarmup().catch((error) => setWarmupError(error.message));
    });
  }
  if (warmupSkipInputEl) {
    warmupSkipInputEl.addEventListener("change", () => {
      handleSkipWarmupChange().catch((error) => setWarmupError(error.message));
    });
  }
  if (warmupManualInputEl) {
    warmupManualInputEl.addEventListener("input", () => {
      if (warmupManualInputEl.value.trim()) {
        state.selectedWarmupConcurrency = null;
      }
      renderWarmup(state.warmupStatus || {});
    });
  }
  document.getElementById("refresh-results-btn").addEventListener("click", () => refreshResults().catch((error) => setRunError(error.message)));
  if (demo4RefreshBtnEl) demo4RefreshBtnEl.addEventListener("click", () => refreshDemo4().catch((error) => setDemo4Message(error.message)));
  if (demo4PrecheckBtnEl) demo4PrecheckBtnEl.addEventListener("click", () => handleDemo4Precheck().catch((error) => setDemo4Message(error.message)));
  if (demo4ValidationDtBtnEl) demo4ValidationDtBtnEl.addEventListener("click", () => handleDemo4Validation("dt").catch((error) => setDemo4Message(error.message)));
  if (demo4ValidationNyBtnEl) demo4ValidationNyBtnEl.addEventListener("click", () => handleDemo4Validation("ny").catch((error) => setDemo4Message(error.message)));
  if (demo4BuildBtnEl) demo4BuildBtnEl.addEventListener("click", () => handleDemo4Build().catch((error) => setDemo4Message(error.message)));
  if (demo4ReportBtnEl) demo4ReportBtnEl.addEventListener("click", () => handleDemo4Report().catch((error) => setDemo4Message(error.message)));
  if (demo4StopBtnEl) demo4StopBtnEl.addEventListener("click", () => handleDemo4Stop().catch((error) => setDemo4Message(error.message)));
  document.getElementById("new-case-btn").addEventListener("click", () => {
    const freshId = nextCaseId();
    fillCaseForm({
      id: freshId,
      canonicalPath: `demo4/input/input_parameter_${String(freshId).padStart(4, "0")}.toml`,
      lam: 0.033333,
      Pe: 10,
      Pe2: 10,
      eps: 0.1,
      Da: 100,
      K0: 1,
      ny: 50,
      xpo_l: 0.33333,
      xpo_r: 0.66667,
      endT: 60,
      total_count: 300,
      coeff_dt: 0.1,
      x_ini_posi: 5,
      alpha: 0.01,
      Sc: 16667,
    });
    state.caseFormDirty = true;
  });
  snapshotSelectEl.addEventListener("change", (event) => {
    loadSnapshot(event.target.value).catch((error) => setRunError(error.message));
  });

  // Run sub-tab switching (overview / build log / run log)
  runSubtabEls.forEach((btn) => {
    btn.addEventListener("click", () => selectRunSubtab(btn.dataset.runSubtab));
  });

  // Per-log toolbar controls
  ["build", "run"].forEach((role) => {
    const onlyEl = document.getElementById(`${role}-log-only-errors`);
    const followEl = document.getElementById(`${role}-log-follow`);
    const copyEl = document.getElementById(`${role}-log-copy`);
    const jumpEl = document.getElementById(`${role}-log-jump-error`);
    if (onlyEl) {
      onlyEl.addEventListener("change", () => {
        state.logOnlyErrors[role] = onlyEl.checked;
        renderLog(role, role === "build" ? state.lastBuildLog : state.lastRunLog);
      });
    }
    if (followEl) {
      followEl.addEventListener("change", () => {
        state.logFollow[role] = followEl.checked;
        if (followEl.checked) {
          const el = role === "build" ? buildLogEl : runLogEl;
          el.scrollTop = el.scrollHeight;
        }
      });
    }
    if (copyEl) copyEl.addEventListener("click", () => copyLogToClipboard(role));
    if (jumpEl) jumpEl.addEventListener("click", () => jumpToFirstError(role));
  });

  // User scroll on log panels: pause / resume auto-follow
  ["build", "run"].forEach((role) => {
    const el = role === "build" ? buildLogEl : runLogEl;
    if (!el) return;
    el.addEventListener("scroll", () => {
      const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 24;
      const checkbox = document.getElementById(`${role}-log-follow`);
      if (!atBottom && state.logFollow[role]) {
        state.logFollow[role] = false;
        if (checkbox) checkbox.checked = false;
      } else if (atBottom && checkbox && !checkbox.checked) {
        checkbox.checked = true;
        state.logFollow[role] = true;
      }
    });
  });
}

async function init() {
  restoreSkipWarmup();
  installEvents();
  await Promise.all([refreshEnv(), refreshCases(), refreshTaskStatus(), refreshDemo4()]);
  if (isSkipWarmupEnabled()) {
    await ensureDefaultRunSettings();
    await refreshWarmupStatus();
  }
  fillGeneratorDefaults();
  if (state.cases.length) {
    await loadCase(state.cases[0].id);
  } else {
    document.getElementById("new-case-btn").click();
  }
}

init().catch((error) => {
  setRunError(error.message);
});

