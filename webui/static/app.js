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
];

const state = {
  env: null,
  cases: [],
  currentCaseId: null,
  currentCase: null,
  pollHandle: null,
};

const caseListEl = document.getElementById("case-list");
const envGridEl = document.getElementById("env-grid");
const envBannerEl = document.getElementById("env-banner");
const currentCaseLabelEl = document.getElementById("current-case-label");
const taskStateBadgeEl = document.getElementById("task-state-badge");
const caseIdInputEl = document.getElementById("field-case-id");
const saveHintEl = document.getElementById("save-hint");
const buildRunBtnEl = document.getElementById("build-run-btn");
const stopBtnEl = document.getElementById("stop-btn");
const buildLogEl = document.getElementById("build-log");
const runLogEl = document.getElementById("run-log");
const runStageEl = document.getElementById("run-stage");
const runStartedEl = document.getElementById("run-started");
const runFinishedEl = document.getElementById("run-finished");
const buildExitEl = document.getElementById("build-exit");
const runExitEl = document.getElementById("run-exit");
const runErrorEl = document.getElementById("run-error");
const snapshotSelectEl = document.getElementById("snapshot-select");
const resultsEmptyEl = document.getElementById("results-empty");
const resultsBodyEl = document.getElementById("results-body");
const summaryGridEl = document.getElementById("summary-grid");

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: {
      "Content-Type": "application/json",
    },
    ...options,
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `Request failed: ${response.status}`);
  }
  return payload;
}

function showMessage(message) {
  envBannerEl.textContent = message;
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

function renderEnv(env) {
  state.env = env;
  envGridEl.innerHTML = "";
  showMessage(
    env.canRun
      ? "构建环境已就绪，可以保存 case 后直接执行“编译运行”。"
      : "构建环境未完全就绪。你仍然可以编辑 case 和查看历史结果，但运行按钮会被禁用。"
  );

  env.checks.forEach((item) => {
    const div = document.createElement("div");
    div.className = `status-card ${item.ok ? "ok" : "bad"}`;
    div.innerHTML = `
      <span>${item.label}</span>
      <strong>${item.ok ? "Ready" : "Missing"}</strong>
      <p>${item.details}</p>
    `;
    envGridEl.appendChild(div);
  });

  buildRunBtnEl.disabled = !env.canRun || state.currentCaseId === null;
}

function renderCases() {
  caseListEl.innerHTML = "";
  if (!state.cases.length) {
    caseListEl.innerHTML = `<div class="empty-state">还没有发现 case 文件，可以先点“新建下一个 Case”。</div>`;
    return;
  }

  state.cases.forEach((item) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `case-item ${item.id === state.currentCaseId ? "active" : ""}`;
    button.innerHTML = `
      <strong>Case ${item.id}</strong>
      <small>${item.path}</small>
      <small>${item.modifiedAt || ""}</small>
    `;
    button.addEventListener("click", () => loadCase(item.id));
    caseListEl.appendChild(button);
  });
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
  saveHintEl.textContent = `保存路径：${data.canonicalPath}`;
  buildRunBtnEl.disabled = !(state.env && state.env.canRun);
  renderCases();
}

function nextCaseId() {
  const highest = state.cases.reduce((max, item) => Math.max(max, item.id), 0);
  return highest + 1;
}

function collectCasePayload() {
  const payload = {
    legacy_marker: state.currentCase?.legacy_marker || 1,
  };
  PARAM_FIELDS.forEach((field) => {
    const input = document.querySelector(`[data-field="${field}"]`);
    payload[field] = input.value;
  });
  return payload;
}

async function refreshEnv() {
  const env = await api("/api/env");
  renderEnv(env);
}

async function refreshCases() {
  state.cases = await api("/api/cases");
  renderCases();
}

async function loadCase(caseId) {
  const data = await api(`/api/cases/${caseId}`);
  fillCaseForm(data);
  await refreshResults();
}

async function saveCurrentCase() {
  const caseId = Number(caseIdInputEl.value);
  if (!caseId || caseId < 1) {
    throw new Error("Case ID 必须是大于 0 的整数。");
  }
  const payload = collectCasePayload();
  await api(`/api/cases/${caseId}`, {
    method: "PUT",
    body: JSON.stringify(payload),
  });
  await refreshCases();
  await loadCase(caseId);
}

function formatStatus(status) {
  taskStateBadgeEl.textContent = status || "idle";
}

function renderTaskStatus(status) {
  formatStatus(status.status);
  runStageEl.textContent = status.stage || "-";
  runStartedEl.textContent = status.startedAt || "-";
  runFinishedEl.textContent = status.finishedAt || "-";
  buildExitEl.textContent = status.buildExitCode ?? "-";
  runExitEl.textContent = status.runExitCode ?? "-";
  buildLogEl.textContent = (status.buildLog || []).join("\n");
  runLogEl.textContent = (status.runLog || []).join("\n");
  setRunError(status.error || "");
  stopBtnEl.disabled = status.status !== "running";

  if (status.status === "running") {
    if (!state.pollHandle) {
      state.pollHandle = window.setInterval(refreshTaskStatus, 2000);
    }
  } else if (state.pollHandle) {
    window.clearInterval(state.pollHandle);
    state.pollHandle = null;
    if (state.currentCaseId) {
      refreshResults().catch((error) => setRunError(error.message));
    }
  }
}

async function refreshTaskStatus() {
  const status = await api("/api/run/status");
  renderTaskStatus(status);
}

function renderSummary(summary, meta) {
  summaryGridEl.innerHTML = "";
  const items = Object.entries(summary || {});
  if (!items.length) {
    const fallback = [
      ["eta 文件", meta.etaAvailable ? "可用" : "缺失"],
      ["remarks 文件", meta.remarksAvailable ? "可用" : "缺失"],
      ["快照数", String(meta.snapshotCount || 0)],
    ];
    fallback.forEach(([label, value]) => addSummaryCard(label, value));
    return;
  }
  items.forEach(([key, value]) => addSummaryCard(key, value));
  addSummaryCard("snapshotCount", String(meta.snapshotCount || 0));
}

function addSummaryCard(label, value) {
  const card = document.createElement("div");
  card.className = "summary-card";
  card.innerHTML = `<span>${label}</span><strong>${value}</strong>`;
  summaryGridEl.appendChild(card);
}

async function refreshResults() {
  if (!state.currentCaseId) {
    resultsBodyEl.classList.add("hidden");
    resultsEmptyEl.classList.remove("hidden");
    return;
  }

  const [meta, etaData, snapshotData] = await Promise.all([
    api(`/api/results/${state.currentCaseId}`),
    api(`/api/results/${state.currentCaseId}/eta`),
    api(`/api/results/${state.currentCaseId}/snapshots`),
  ]);

  renderSummary(meta.summary, meta);
  populateSnapshots(snapshotData.snapshots || []);
  drawLineChart(
    document.getElementById("eta-chart"),
    etaData.points || [],
    "time",
    "eta",
    "#c2410c"
  );

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
    drawLineChart(document.getElementById("profile-chart"), [], "x", "eta", "#7c2d12");
    drawHeatmap(document.getElementById("heatmap-canvas"), []);
  }
}

function populateSnapshots(snapshots) {
  snapshotSelectEl.innerHTML = "";
  if (!snapshots.length) {
    const option = document.createElement("option");
    option.textContent = "没有快照";
    option.value = "";
    snapshotSelectEl.appendChild(option);
    snapshotSelectEl.disabled = true;
    return;
  }

  snapshotSelectEl.disabled = false;
  snapshots.forEach((snapshot) => {
    const option = document.createElement("option");
    option.value = String(snapshot.count);
    option.textContent = `快照 ${snapshot.count}`;
    snapshotSelectEl.appendChild(option);
  });
}

async function loadSnapshot(count) {
  if (!count) {
    return;
  }
  const snapshot = await api(`/api/results/${state.currentCaseId}/snapshot/${count}`);
  drawHeatmap(document.getElementById("heatmap-canvas"), snapshot.matrix || []);
  drawLineChart(
    document.getElementById("profile-chart"),
    snapshot.profile || [],
    "x",
    "eta",
    "#7c2d12"
  );
}

function drawLineChart(canvas, points, xKey, yKey, color) {
  const ctx = canvas.getContext("2d");
  const { width, height } = canvas;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#fffaf2";
  ctx.fillRect(0, 0, width, height);

  if (!points.length) {
    ctx.fillStyle = "#7a6f60";
    ctx.font = "16px sans-serif";
    ctx.fillText("暂无数据", width / 2 - 30, height / 2);
    return;
  }

  const margin = { left: 54, right: 18, top: 18, bottom: 34 };
  const xs = points.map((point) => point[xKey]);
  const ys = points.map((point) => point[yKey]);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);
  const xSpan = maxX - minX || 1;
  const ySpan = maxY - minY || 1;

  ctx.strokeStyle = "#cfc3af";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(margin.left, margin.top);
  ctx.lineTo(margin.left, height - margin.bottom);
  ctx.lineTo(width - margin.right, height - margin.bottom);
  ctx.stroke();

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  points.forEach((point, index) => {
    const x = margin.left + ((point[xKey] - minX) / xSpan) * (width - margin.left - margin.right);
    const y = height - margin.bottom - ((point[yKey] - minY) / ySpan) * (height - margin.top - margin.bottom);
    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();

  ctx.fillStyle = "#4b4135";
  ctx.font = "12px sans-serif";
  ctx.fillText(minX.toFixed(3), margin.left, height - 10);
  ctx.fillText(maxX.toFixed(3), width - margin.right - 34, height - 10);
  ctx.fillText(maxY.toExponential(2), 6, margin.top + 8);
  ctx.fillText(minY.toExponential(2), 6, height - margin.bottom);
}

function drawHeatmap(canvas, matrix) {
  const ctx = canvas.getContext("2d");
  const { width, height } = canvas;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#fffaf2";
  ctx.fillRect(0, 0, width, height);

  if (!matrix.length || !matrix[0].length) {
    ctx.fillStyle = "#7a6f60";
    ctx.font = "16px sans-serif";
    ctx.fillText("暂无快照矩阵", width / 2 - 44, height / 2);
    return;
  }

  const rows = matrix.length;
  const cols = matrix[0].length;
  const values = matrix.flat();
  const min = Math.min(...values);
  const max = Math.max(...values);
  const span = max - min || 1;
  const cellW = width / cols;
  const cellH = height / rows;

  matrix.forEach((row, rowIndex) => {
    row.forEach((value, colIndex) => {
      const ratio = (value - min) / span;
      ctx.fillStyle = heatColor(ratio);
      ctx.fillRect(colIndex * cellW, rowIndex * cellH, Math.ceil(cellW), Math.ceil(cellH));
    });
  });

  ctx.fillStyle = "rgba(28, 26, 24, 0.86)";
  ctx.font = "12px sans-serif";
  ctx.fillText(`min ${min.toExponential(2)}`, 10, 18);
  ctx.fillText(`max ${max.toExponential(2)}`, width - 94, 18);
}

function heatColor(value) {
  const clamped = Math.max(0, Math.min(1, value));
  const r = Math.round(36 + clamped * 214);
  const g = Math.round(84 + clamped * 120);
  const b = Math.round(122 - clamped * 90);
  return `rgb(${r}, ${g}, ${b})`;
}

async function handleSave() {
  try {
    await saveCurrentCase();
    showMessage(`Case ${state.currentCaseId} 已保存。`);
  } catch (error) {
    setRunError(error.message);
  }
}

async function handleBuildRun() {
  try {
    await saveCurrentCase();
    setRunError("");
    await api("/api/build-and-run", {
      method: "POST",
      body: JSON.stringify({ caseId: state.currentCaseId }),
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
  document.getElementById("refresh-env-btn").addEventListener("click", () => refreshEnv().catch((error) => setRunError(error.message)));
  document.getElementById("refresh-cases-btn").addEventListener("click", () => refreshCases().catch((error) => setRunError(error.message)));
  document.getElementById("save-case-btn").addEventListener("click", handleSave);
  document.getElementById("build-run-btn").addEventListener("click", handleBuildRun);
  document.getElementById("stop-btn").addEventListener("click", handleStop);
  document.getElementById("refresh-results-btn").addEventListener("click", () => refreshResults().catch((error) => setRunError(error.message)));
  document.getElementById("new-case-btn").addEventListener("click", () => {
    const freshId = nextCaseId();
    fillCaseForm({
      id: freshId,
      canonicalPath: `input/input_parameter_${String(freshId).padStart(4, "0")}.txt`,
      legacy_marker: 1,
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
    });
  });
  snapshotSelectEl.addEventListener("change", (event) => {
    loadSnapshot(event.target.value).catch((error) => setRunError(error.message));
  });
}

async function init() {
  installEvents();
  await Promise.all([refreshEnv(), refreshCases(), refreshTaskStatus()]);
  if (state.cases.length) {
    await loadCase(state.cases[0].id);
  } else {
    document.getElementById("new-case-btn").click();
  }
}

init().catch((error) => {
  setRunError(error.message);
});
