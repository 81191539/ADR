#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import math
import os
import re
import shlex
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEMO4_ROOT = PROJECT_ROOT / "demo4"
CASES_ROOT = DEMO4_ROOT / "cases"
RESULTS_ROOT = DEMO4_ROOT / "results"

PARAM_ORDER: list[tuple[str, type]] = [
    ("lam", float),
    ("Pe", float),
    ("Pe2", float),
    ("eps", float),
    ("Da", float),
    ("K0", float),
    ("ny", int),
    ("xpo_l", float),
    ("xpo_r", float),
    ("endT", float),
    ("total_count", int),
    ("coeff_dt", float),
    ("x_ini_posi", float),
    ("alpha", float),
]

OPTIONAL_DEFAULTS: dict[str, float] = {
    "Sc": 16667.0,
}

RUNTIME_ORDER: list[tuple[str, type]] = [
    ("stats_interval", int),
    ("stability_check_interval", int),
    ("checkpoint_interval", int),
    ("enable_dense_dump", bool),
    ("dense_dump_start", float),
    ("dense_dump_count", int),
    ("convergence_threshold", float),
    ("output_matlab", bool),
    ("output_tecplot", bool),
]

CASE_RE = re.compile(r"input_parameter_(\d+)\.(toml|txt)$")
ASSIGNMENT_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^#;%]+)")
DT_ADJUST_RE = re.compile(r"dt_adjustments\s*=\s*\[", re.IGNORECASE)


@dataclass(frozen=True)
class CaseFile:
    case_id: int
    path: Path


def ensure_dir(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def resolve_path(path_text: str | Path) -> Path:
    path = Path(path_text)
    if path.is_absolute():
        return path
    return (PROJECT_ROOT / path).resolve()


def discover_cases(input_dir: Path) -> dict[int, Path]:
    input_dir = resolve_path(input_dir)
    found: dict[int, list[Path]] = {}
    if not input_dir.exists():
        raise FileNotFoundError(f"Input directory not found: {input_dir}")
    for entry in input_dir.iterdir():
        if not entry.is_file():
            continue
        match = CASE_RE.match(entry.name)
        if not match:
            continue
        found.setdefault(int(match.group(1)), []).append(entry)
    selected: dict[int, Path] = {}
    for case_id, paths in found.items():
        toml = [path for path in paths if path.suffix == ".toml"]
        selected[case_id] = sorted(toml or paths)[0]
    return dict(sorted(selected.items()))


def parse_case_selection(text: str | None, available: dict[int, Path]) -> list[CaseFile]:
    if not text:
        return [CaseFile(case_id, path) for case_id, path in available.items()]
    selected: set[int] = set()
    for raw in re.split(r"[\s,]+", text.strip()):
        if not raw:
            continue
        if "-" in raw:
            left, right = raw.split("-", 1)
            start = int(left)
            end = int(right)
            if start > end:
                raise ValueError(f"Invalid case range: {raw}")
            selected.update(range(start, end + 1))
        else:
            selected.add(int(raw))
    missing = sorted(case_id for case_id in selected if case_id not in available)
    if missing:
        raise ValueError(f"Case id(s) not found: {', '.join(map(str, missing[:20]))}")
    return [CaseFile(case_id, available[case_id]) for case_id in sorted(selected)]


def _parse_number(raw: str) -> float:
    value = raw.strip().strip('"').strip("'")
    return float(value)


def parse_toml_case(path: Path) -> dict[str, Any]:
    values: dict[str, Any] = {}
    runtime: dict[str, Any] = {}
    section = ""
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.split("#", 1)[0].strip()
        if not stripped:
            continue
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped[1:-1].strip()
            continue
        if section == "runtime":
            match = ASSIGNMENT_RE.match(stripped)
            if not match:
                continue
            key, raw = match.groups()
            spec = dict(RUNTIME_ORDER).get(key)
            if spec is None:
                continue
            raw_value = raw.strip()
            if spec is bool:
                lowered = raw_value.lower()
                if lowered in {"true", "false"}:
                    runtime[key] = lowered == "true"
                continue
            number = _parse_number(raw_value)
            runtime[key] = int(number) if spec is int else number
            continue
        if section:
            continue
        match = ASSIGNMENT_RE.match(stripped)
        if not match:
            continue
        key, raw = match.groups()
        if key == "case_id":
            values[key] = int(_parse_number(raw))
        elif key in dict(PARAM_ORDER) or key in OPTIONAL_DEFAULTS:
            values[key] = _parse_number(raw)
    if runtime:
        values["runtime"] = runtime
    return normalize_case_values(values, path)


def parse_legacy_case(path: Path) -> dict[str, Any]:
    tokens = path.read_text(encoding="utf-8", errors="replace").split()
    if len(tokens) < 1 + len(PARAM_ORDER):
        raise ValueError(f"{path} has too few tokens for legacy format.")
    values: dict[str, Any] = {}
    for index, (name, caster) in enumerate(PARAM_ORDER, start=1):
        number = float(tokens[index])
        values[name] = int(number) if caster is int else number
    return normalize_case_values(values, path)


def normalize_case_values(values: dict[str, Any], path: Path | None = None) -> dict[str, Any]:
    normalized: dict[str, Any] = {}
    missing: list[str] = []
    for name, caster in PARAM_ORDER:
        if name not in values:
            missing.append(name)
            continue
        number = float(values[name])
        normalized[name] = int(number) if caster is int else number
    if missing:
        source = f" in {path}" if path else ""
        raise ValueError(f"Missing case field(s){source}: {', '.join(missing)}")
    for name, default in OPTIONAL_DEFAULTS.items():
        normalized[name] = float(values.get(name, default))
    runtime = values.get("runtime")
    if isinstance(runtime, dict):
        normalized_runtime: dict[str, Any] = {}
        for name, caster in RUNTIME_ORDER:
            if name not in runtime:
                continue
            value = runtime[name]
            if caster is bool:
                normalized_runtime[name] = bool(value)
            elif caster is int:
                normalized_runtime[name] = int(value)
            else:
                normalized_runtime[name] = float(value)
        if normalized_runtime:
            normalized["runtime"] = normalized_runtime
    return normalized


def read_case(path: Path) -> dict[str, Any]:
    return parse_toml_case(path) if path.suffix == ".toml" else parse_legacy_case(path)


def serialize_case(case_id: int, values: dict[str, Any]) -> str:
    lines = [f"case_id = {case_id}"]
    for name, caster in PARAM_ORDER:
        value = values[name]
        if caster is int:
            lines.append(f"{name} = {int(value)}")
        else:
            lines.append(f"{name} = {float(value):.12g}")
    for name, default in OPTIONAL_DEFAULTS.items():
        lines.append(f"{name} = {float(values.get(name, default)):.12g}")
    runtime = values.get("runtime")
    if isinstance(runtime, dict) and runtime:
        lines.append("")
        lines.append("[runtime]")
        for name, caster in RUNTIME_ORDER:
            if name not in runtime:
                continue
            value = runtime[name]
            if caster is bool:
                lines.append(f"{name} = {'true' if value else 'false'}")
            elif caster is int:
                lines.append(f"{name} = {int(value)}")
            else:
                lines.append(f"{name} = {float(value):.12g}")
    return "\n".join(lines) + "\n"


def oscillatory_value(alpha: float, y: float, phase: float) -> float:
    ca = math.cos(alpha)
    sa = math.sin(alpha)
    ch = math.cosh(alpha)
    sh = math.sinh(alpha)
    c2 = math.cos(2.0 * alpha)
    ch2 = math.cosh(2.0 * alpha)
    a2 = alpha * alpha
    dcc = 1.0 / (c2 + ch2)
    da2 = 1.0 / a2
    sat = math.sin(phase)
    cat = math.cos(phase)
    arg = 2.0 * alpha * (y - 0.5)
    say = math.sin(arg)
    cay = math.cos(arg)
    shay = math.sinh(arg)
    chay = math.cosh(arg)
    return dcc * da2 * (
        (c2 + ch2) * sat
        + 2.0 * sa * sh * cay * cat * chay
        - 2.0 * sa * sh * say * sat * shay
        - 2.0 * ca * ch * (cay * chay * sat + cat * say * shay)
    )


def estimate_max_ff(alpha: float, y_samples: int = 161, phase_samples: int = 160) -> float:
    if abs(alpha) < 1e-12:
        raise ValueError("alpha must not be zero.")
    max_value = 0.0
    for yi in range(y_samples):
        y = yi / (y_samples - 1)
        for ti in range(phase_samples):
            phase = 2.0 * math.pi * ti / phase_samples
            max_value = max(max_value, abs(oscillatory_value(alpha, y, phase)))
    return max_value


def finite_ratio(numerator: float, denominator: float) -> float | None:
    if not math.isfinite(numerator) or not math.isfinite(denominator) or denominator <= 0.0:
        return None
    return numerator / denominator


def precheck_metrics(case_id: int, values: dict[str, Any]) -> dict[str, Any]:
    lam = float(values["lam"])
    ny = int(values["ny"])
    h = 1.0 / ny
    nx = int(ny * (1.0 / lam))
    xright = nx / ny
    dt = float(values["coeff_dt"]) * h * h
    alpha = float(values["alpha"])
    sc = float(values["Sc"])
    pe = float(values["Pe"])
    pe2 = float(values["Pe2"])
    max_ff = estimate_max_ff(alpha)
    u_poiseuille = abs(pe) * 0.25
    u_oscillatory = abs(pe2) * max_ff
    u_max_estimate = u_poiseuille + u_oscillatory
    dt_diffusion = 0.25 * h * h
    dt_advection = h / u_max_estimate if u_max_estimate > 0.0 else math.inf
    t_osc = math.pi / (alpha * alpha * sc)
    dt_osc_20 = t_osc / 20.0
    eps = float(values["eps"])
    da = float(values["Da"])
    k0 = float(values["K0"])
    dt_eta_estimate = 1.0 / (eps * da * (1.0 + 1.0 / k0)) if eps * da > 0.0 and k0 > 0.0 else math.inf
    numerical_diffusion_poiseuille = u_poiseuille * h / 2.0
    numerical_diffusion_oscillatory = u_oscillatory * h / 2.0
    risk_items = risk_item_list(
        dt,
        dt_diffusion,
        dt_advection,
        dt_osc_20,
        dt_eta_estimate,
        numerical_diffusion_poiseuille,
        numerical_diffusion_oscillatory,
    )
    schemes = candidate_scheme_rows()
    scheme_results = scheme_result_rows(
        dt,
        dt_diffusion,
        dt_advection,
        dt_osc_20,
        numerical_diffusion_poiseuille,
        numerical_diffusion_oscillatory,
    )
    return {
        "case_id": case_id,
        "source_values": values,
        "grid": {
            "h": h,
            "nx": nx,
            "ny": ny,
            "xright": xright,
        },
        "time_step": {
            "dt_current": dt,
            "dt_diffusion_limit": dt_diffusion,
            "dt_advection_limit": dt_advection,
            "oscillation_period": t_osc,
            "dt_oscillation_T_over_20": dt_osc_20,
            "dt_eta_estimate": dt_eta_estimate,
            "ratio_dt_to_diffusion": finite_ratio(dt, dt_diffusion),
            "ratio_dt_to_advection": finite_ratio(dt, dt_advection),
            "ratio_dt_to_oscillation": finite_ratio(dt, dt_osc_20),
            "ratio_dt_to_eta": finite_ratio(dt, dt_eta_estimate),
        },
        "velocity": {
            "max_abs_ff_alpha": max_ff,
            "u_poiseuille_estimate": u_poiseuille,
            "u_oscillatory_estimate": u_oscillatory,
            "u_max_estimate": u_max_estimate,
            "mesh_pe_poiseuille": abs(pe) * h,
            "mesh_pe_oscillatory": abs(pe2) * max_ff * h,
        },
        "numerical_diffusion": {
            "poiseuille_upwind_estimate": numerical_diffusion_poiseuille,
            "oscillatory_upwind_estimate": numerical_diffusion_oscillatory,
            "total_upwind_estimate": numerical_diffusion_poiseuille + numerical_diffusion_oscillatory,
        },
        "risk_items": risk_items,
        "candidate_scheme_rows": schemes,
        "scheme_result_rows": scheme_results,
    }


def risk_item_list(
    dt: float,
    dt_diffusion: float,
    dt_advection: float,
    dt_osc: float,
    dt_eta: float,
    diff_poiseuille: float,
    diff_oscillatory: float,
) -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    checks = [
        ("diffusion_cfl", dt, dt_diffusion, "current dt exceeds explicit diffusion estimate"),
        ("advection_cfl", dt, dt_advection, "current dt exceeds explicit advection estimate"),
        ("oscillation_sampling", dt, dt_osc, "current dt exceeds T_osc/20 sampling estimate"),
        ("eta_explicit", dt, dt_eta, "current dt exceeds eta explicit estimate"),
    ]
    for key, value, limit, message in checks:
        ratio = finite_ratio(value, limit)
        if ratio is not None and ratio > 1.0:
            items.append({"key": key, "ratio": ratio, "message": message})
    if diff_poiseuille > 0.05:
        items.append({
            "key": "poiseuille_numerical_diffusion",
            "value": diff_poiseuille,
            "message": "Poiseuille first-order-upwind diffusion estimate exceeds 0.05",
        })
    if diff_oscillatory > 0.05:
        items.append({
            "key": "oscillatory_numerical_diffusion",
            "value": diff_oscillatory,
            "message": "Oscillatory first-order-upwind diffusion estimate exceeds 0.05",
        })
    return items


def candidate_scheme_rows() -> list[dict[str, Any]]:
    return [
        {
            "name": "Baseline Explicit",
            "expected_benefit": "No numerical-code changes; provides current behavior and a reference row.",
            "expected_cost": "Keeps explicit CFL limits and first-order upwind diffusion.",
            "implementation_risk": "Low",
            "status": "measured",
            "evidence": "Baseline Ny validation basically passed for case1, case3, and case4 by eta-t visual inspection. Case2 remains unresolved: ny=50 and ny=71 differ greatly, while larger Ny runs are blocked by computation cost.",
            "metrics_to_inspect": [
                "dt ratios",
                "upwind numerical diffusion estimates",
                "stability events",
                "refinement differences",
            ],
        },
        {
            "name": "High-Resolution Advection",
            "expected_benefit": "Targets artificial diffusion in convection-dominated cases.",
            "expected_cost": "Does not by itself remove explicit time-step limits.",
            "implementation_risk": "Medium",
            "status": "theoretical",
            "evidence": "No measured solver output yet; keep as a candidate route until implementation and validation evidence exist.",
            "metrics_to_inspect": [
                "grid refinement sensitivity",
                "negative concentration events",
                "front oscillation",
                "eta_ave differences",
            ],
        },
        {
            "name": "Semi-Lagrangian / IMEX",
            "expected_benefit": "Targets advection CFL and diffusion step-size limits.",
            "expected_cost": "Adds interpolation and implicit-solver complexity.",
            "implementation_risk": "High",
            "status": "theoretical",
            "evidence": "No measured solver output yet; expected benefit must be proven by stable larger-step runs and refined-reference comparisons.",
            "metrics_to_inspect": [
                "step-size sensitivity",
                "interpolation smoothing",
                "wall-clock per physical time",
                "dense snapshot consistency",
            ],
        },
    ]


def scheme_result_rows(
    dt: float,
    dt_diffusion: float,
    dt_advection: float,
    dt_osc: float,
    diff_poiseuille: float,
    diff_oscillatory: float,
) -> list[dict[str, Any]]:
    return [
        {
            "scheme": "Baseline Explicit",
            "result_type": "current_precheck_metrics",
            "observed_or_expected_result": "Current solver metrics are directly measurable before running validation.",
            "metrics": {
                "dt_current": dt,
                "ratio_dt_to_diffusion": finite_ratio(dt, dt_diffusion),
                "ratio_dt_to_advection": finite_ratio(dt, dt_advection),
                "ratio_dt_to_oscillation": finite_ratio(dt, dt_osc),
                "poiseuille_upwind_diffusion": diff_poiseuille,
                "oscillatory_upwind_diffusion": diff_oscillatory,
                "total_upwind_diffusion": diff_poiseuille + diff_oscillatory,
            },
        },
        {
            "scheme": "High-Resolution Advection",
            "result_type": "theoretical_expected_result",
            "observed_or_expected_result": "Expected to reduce first-order-upwind artificial diffusion; no new implementation is measured in demo4 first pass.",
            "metrics": {
                "baseline_poiseuille_upwind_diffusion_to_reduce": diff_poiseuille,
                "baseline_oscillatory_upwind_diffusion_to_reduce": diff_oscillatory,
                "explicit_time_step_ratios_unchanged_without_time_integrator_change": True,
            },
        },
        {
            "scheme": "Semi-Lagrangian / IMEX",
            "result_type": "theoretical_expected_result",
            "observed_or_expected_result": "Expected to target advection/diffusion step-size limits; no new implementation is measured in demo4 first pass.",
            "metrics": {
                "baseline_ratio_dt_to_diffusion_to_relax": finite_ratio(dt, dt_diffusion),
                "baseline_ratio_dt_to_advection_to_relax": finite_ratio(dt, dt_advection),
                "baseline_ratio_dt_to_oscillation_observed": finite_ratio(dt, dt_osc),
            },
        },
    ]


def write_json(path: Path, payload: Any) -> None:
    ensure_dir(path.parent)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_csv(path: Path, rows: list[dict[str, Any]], fieldnames: list[str]) -> None:
    ensure_dir(path.parent)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({name: row.get(name, "") for name in fieldnames})


def flatten_precheck(metrics: dict[str, Any], source_path: Path | None = None) -> dict[str, Any]:
    time_step = metrics["time_step"]
    velocity = metrics["velocity"]
    diffusion = metrics["numerical_diffusion"]
    grid = metrics["grid"]
    values = metrics["source_values"]
    return {
        "case_id": metrics["case_id"],
        "source_path": str(source_path) if source_path else "",
        "lam": values["lam"],
        "Pe": values["Pe"],
        "Pe2": values["Pe2"],
        "alpha": values["alpha"],
        "Sc": values["Sc"],
        "ny": grid["ny"],
        "nx": grid["nx"],
        "h": grid["h"],
        "dt_current": time_step["dt_current"],
        "dt_diffusion_limit": time_step["dt_diffusion_limit"],
        "dt_advection_limit": time_step["dt_advection_limit"],
        "dt_oscillation_T_over_20": time_step["dt_oscillation_T_over_20"],
        "dt_eta_estimate": time_step["dt_eta_estimate"],
        "ratio_dt_to_diffusion": time_step["ratio_dt_to_diffusion"],
        "ratio_dt_to_advection": time_step["ratio_dt_to_advection"],
        "ratio_dt_to_oscillation": time_step["ratio_dt_to_oscillation"],
        "ratio_dt_to_eta": time_step["ratio_dt_to_eta"],
        "max_abs_ff_alpha": velocity["max_abs_ff_alpha"],
        "u_poiseuille_estimate": velocity["u_poiseuille_estimate"],
        "u_oscillatory_estimate": velocity["u_oscillatory_estimate"],
        "u_max_estimate": velocity["u_max_estimate"],
        "mesh_pe_poiseuille": velocity["mesh_pe_poiseuille"],
        "mesh_pe_oscillatory": velocity["mesh_pe_oscillatory"],
        "poiseuille_upwind_diffusion": diffusion["poiseuille_upwind_estimate"],
        "oscillatory_upwind_diffusion": diffusion["oscillatory_upwind_estimate"],
        "total_upwind_diffusion": diffusion["total_upwind_estimate"],
        "risk_items": "; ".join(item["key"] for item in metrics["risk_items"]),
    }


def locate_solver(explicit_solver: str | None = None) -> Path:
    candidates: list[Path] = []
    if explicit_solver:
        candidates.append(resolve_path(explicit_solver))
    candidates.extend([
        PROJECT_ROOT / "build" / "Release" / "df2d.exe",
        PROJECT_ROOT / "build" / "Debug" / "df2d.exe",
        PROJECT_ROOT / "build" / "RelWithDebInfo" / "df2d.exe",
        PROJECT_ROOT / "build" / "df2d.exe",
        PROJECT_ROOT / "build" / "df2d",
    ])
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError("Could not find df2d solver. Pass --solver explicitly.")


def detect_elf(path: Path) -> bool:
    try:
        return path.read_bytes()[:4] == b"\x7fELF"
    except OSError:
        return False


def to_wsl_path(path: Path) -> str:
    completed = subprocess.run(
        ["wsl", "-e", "wslpath", "-a", str(path)],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=10,
    )
    if completed.returncode != 0:
        message = completed.stderr.strip() or completed.stdout.strip() or "wslpath failed"
        raise RuntimeError(f"Could not convert path for WSL: {path}: {message}")
    return completed.stdout.strip()


def solver_command(
    solver: Path,
    cwd: Path,
    case_id: int,
    extra_args: list[str] | None = None,
) -> tuple[list[str], str | None]:
    solver_args = ["--case", str(case_id), "--force-restart"]
    if extra_args:
        solver_args.extend(extra_args)
    if os.name == "nt" and detect_elf(solver):
        linux_solver = to_wsl_path(solver)
        linux_cwd = to_wsl_path(cwd)
        script = (
            f"cd {shlex.quote(linux_cwd)} && "
            + " ".join(shlex.quote(part) for part in [linux_solver, *solver_args])
        )
        return ["wsl", "-e", "bash", "-lc", script], str(PROJECT_ROOT)
    return [str(solver), *solver_args], str(cwd)


def parse_eta_series(path: Path) -> list[tuple[float, float, float]]:
    if not path.exists():
        return []
    points: list[tuple[float, float, float]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        try:
            points.append((float(parts[0]), float(parts[1]), float(parts[2])))
        except ValueError:
            continue
    return points


def count_dt_adjustments(remarks_path: Path) -> int:
    if not remarks_path.exists():
        return 0
    in_block = False
    count = 0
    for line in remarks_path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if DT_ADJUST_RE.search(stripped):
            in_block = True
            continue
        if in_block and stripped.startswith("]"):
            break
        if in_block and stripped and not stripped.startswith("%"):
            count += 1
    return count


def run_solver(
    solver: Path,
    cwd: Path,
    case_id: int,
    extra_args: list[str] | None = None,
    timeout_seconds: float | None = None,
) -> dict[str, Any]:
    args, run_cwd = solver_command(solver, cwd, case_id, extra_args)
    start = time.perf_counter()
    try:
        completed = subprocess.run(
            args,
            cwd=run_cwd,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=timeout_seconds,
        )
        exit_code = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
        timed_out = False
    except subprocess.TimeoutExpired as exc:
        exit_code = 124
        stdout = exc.stdout or ""
        stderr = (exc.stderr or "") + f"\nTimed out after {timeout_seconds} seconds."
        timed_out = True
    elapsed = time.perf_counter() - start
    return {
        "exit_code": exit_code,
        "elapsed_seconds": elapsed,
        "stdout": stdout,
        "stderr": stderr,
        "timed_out": timed_out,
        "command": args,
        "cwd": str(cwd),
        "process_cwd": run_cwd,
    }


def copy_or_clean_dir(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)
