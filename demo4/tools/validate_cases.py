#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import shlex
import shutil
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Any

from demo4_common import (
    CASES_ROOT,
    RESULTS_ROOT,
    copy_or_clean_dir,
    count_dt_adjustments,
    detect_elf,
    discover_cases,
    ensure_dir,
    locate_solver,
    parse_case_selection,
    parse_eta_series,
    read_case,
    resolve_path,
    run_solver,
    serialize_case,
    to_wsl_path,
    write_csv,
    write_json,
)


VARIANTS = [
    "baseline",
    "dt_refined",
    "ny_refined_1",
    "ny_refined_2",
    "ny_refined_3",
    "ny_refined",
    "dt_ny_refined",
]

NY_SWEEP_VARIANTS = [
    "baseline",
    "ny_refined_1",
    "ny_refined_2",
    "ny_refined_3",
    "ny_refined",
]

ADVECTION_SCHEME_LABELS = {
    "upwind": "Baseline Upwind",
    "tvd-mc": "High-Resolution (TVD-MC)",
}

SUMMARY_FIELDS = [
    "run_id",
    "validation_kind",
    "case_id",
    "advection_scheme",
    "scheme_display",
    "variant",
    "display_variant",
    "reference_variant",
    "reference_display_variant",
    "exit_code",
    "elapsed_seconds",
    "ny",
    "coeff_dt",
    "actual_iterations",
    "iterations_per_second",
    "final_time",
    "final_eta",
    "final_deta_dt",
    "final_eta_diff_abs",
    "relative_final_eta_vs_dt_ny_refined",
    "eta_point_count",
    "dt_adjustment_count",
    "work_dir",
    "eta_path",
    "remarks_path",
]

CURVE_METRIC_FIELDS = [
    "run_id",
    "validation_kind",
    "case_id",
    "advection_scheme",
    "scheme_display",
    "variant",
    "display_variant",
    "reference_variant",
    "reference_display_variant",
    "comparison_status",
    "ny",
    "coeff_dt",
    "exit_code",
    "elapsed_seconds",
    "iterations_per_second",
    "final_time",
    "final_eta_diff_abs",
    "eta_point_count",
    "rmse_abs",
    "rmse_rel",
    "linf_abs",
    "linf_rel",
    "auc_abs_diff",
    "auc_rel_diff",
    "common_time_end",
]

SCHEME_COMPARISON_FIELDS = [
    "run_id",
    "validation_kind",
    "case_id",
    "variant",
    "display_variant",
    "ny",
    "coeff_dt",
    "baseline_scheme",
    "comparison_scheme",
    "comparison_status",
    "upwind_exit_code",
    "tvd_mc_exit_code",
    "final_time_upwind",
    "final_time_tvd_mc",
    "final_eta_upwind",
    "final_eta_tvd_mc",
    "final_eta_delta",
    "final_eta_delta_abs",
    "final_eta_delta_rel",
    "rmse_abs",
    "rmse_rel",
    "linf_abs",
    "linf_rel",
    "auc_abs_diff",
    "auc_rel_diff",
    "common_time_end",
    "upwind_elapsed_seconds",
    "tvd_mc_elapsed_seconds",
    "runtime_ratio_tvd_mc_vs_upwind",
]

REMARK_NUMERIC_FIELDS = {
    "actual_iterations",
    "iterations_per_second",
    "time_total",
}

VALIDATION_CHECKPOINT_INTERVAL = 2147483647
CUDA_NY_AUTO_THRESHOLD = 100

def geometric_ny_levels(base_ny: int, target_ny: int, count: int = 5) -> list[int]:
    if count < 2:
        return [base_ny]
    if target_ny <= base_ny:
        raise ValueError("ny-refine target must be greater than baseline ny.")
    if target_ny - base_ny + 1 <= count:
        return list(range(base_ny, target_ny + 1))
    levels = [
        int(round(base_ny * (target_ny / base_ny) ** (index / (count - 1))))
        for index in range(count)
    ]
    levels[0] = base_ny
    levels[-1] = target_ny
    if target_ny - base_ny >= count - 1:
        for index in range(1, count - 1):
            lower = levels[index - 1] + 1
            upper = target_ny - (count - 1 - index)
            levels[index] = min(max(levels[index], lower), upper)
    return levels


def ny_variant_name(index: int, total: int) -> str:
    if index == 0:
        return "baseline"
    if index == total - 1:
        return "ny_refined"
    return f"ny_refined_{index}"


def ny_sweep_plan(values: dict[str, Any], ny_refine: int) -> list[tuple[str, int]]:
    base_ny = int(values["ny"])
    target_ny = int(round(base_ny * ny_refine))
    levels = geometric_ny_levels(base_ny, target_ny, len(NY_SWEEP_VARIANTS))
    return [(ny_variant_name(index, len(levels)), ny) for index, ny in enumerate(levels)]


def ny_override_for_variant(values: dict[str, Any], variant: str, ny_refine: int) -> int | None:
    if variant not in NY_SWEEP_VARIANTS:
        return None
    for name, ny in ny_sweep_plan(values, ny_refine):
        if name == variant:
            return ny
    return None


def planned_variants_for_case(
    selected_variants: list[str],
    values: dict[str, Any],
    ny_refine: int,
) -> list[str]:
    if selected_variants and all(variant in NY_SWEEP_VARIANTS for variant in selected_variants):
        return [name for name, _ in ny_sweep_plan(values, ny_refine)]
    return selected_variants


def max_planned_ny_for_cases(
    selected_cases: list[Any],
    selected_variants: list[str],
    ny_refine: int,
) -> int:
    max_ny = 0
    for case in selected_cases:
        values = read_case(case.path)
        for variant in planned_variants_for_case(selected_variants, values, ny_refine):
            ny_override = ny_override_for_variant(values, variant, ny_refine)
            variant_case = variant_values(values, variant, 2.0, ny_refine, ny_override)
            max_ny = max(max_ny, int(variant_case["ny"]))
    return max_ny


def infer_validation_kind(selected_variants: list[str]) -> str:
    variants = set(selected_variants)
    if variants and variants.issubset(set(NY_SWEEP_VARIANTS)):
        return "ny"
    if variants and variants.issubset({"baseline", "dt_refined"}):
        return "dt"
    return "mixed"


def unique_run_root(kind: str) -> tuple[str, Path]:
    validation_root = RESULTS_ROOT / "validation" / kind
    ensure_dir(validation_root)
    base_run_id = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_id = base_run_id
    run_root = validation_root / run_id
    suffix = 2
    while run_root.exists():
        run_id = f"{base_run_id}_{suffix:02d}"
        run_root = validation_root / run_id
        suffix += 1
    ensure_dir(run_root)
    return run_id, run_root


def relative_result_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(RESULTS_ROOT.resolve()))
    except ValueError:
        return str(path)


def variant_values(
    values: dict[str, Any],
    variant: str,
    dt_refine: float,
    ny_refine: int,
    ny_override: int | None = None,
) -> dict[str, Any]:
    updated = dict(values)
    if variant in {"dt_refined", "dt_ny_refined"}:
        updated["coeff_dt"] = float(updated["coeff_dt"]) / dt_refine
    if ny_override is not None:
        updated["ny"] = ny_override
    elif variant in {"ny_refined", "dt_ny_refined"}:
        updated["ny"] = int(updated["ny"]) * ny_refine
    return updated


def prepare_variant(
    case_id: int,
    advection_scheme: str,
    values: dict[str, Any],
    variant: str,
    run_root: Path,
    dt_refine: float,
    ny_refine: int,
    max_end_t: float | None,
    use_scheme_subdir: bool = False,
) -> tuple[Path, Path, dict[str, Any]]:
    case_variant_root = CASES_ROOT / f"case_{case_id:04d}"
    if use_scheme_subdir:
        case_variant_root = case_variant_root / advection_scheme
    case_variant_root = case_variant_root / variant
    copy_or_clean_dir(case_variant_root)
    input_dir = ensure_dir(case_variant_root / "input")
    ensure_dir(case_variant_root / "output")
    ny_override = ny_override_for_variant(values, variant, ny_refine)
    variant_case = variant_values(values, variant, dt_refine, ny_refine, ny_override)
    result_variant_name = display_variant_name(
        variant,
        int(variant_case["ny"]),
        ny_sweep_only=variant in NY_SWEEP_VARIANTS,
    )
    result_case_root = run_root / f"case_{case_id:04d}"
    if use_scheme_subdir:
        result_case_root = result_case_root / advection_scheme
    result_variant_root = result_case_root / result_variant_name
    ensure_dir(result_variant_root)
    if max_end_t is not None:
        variant_case["endT"] = max_end_t
        variant_case["total_count"] = max(1, min(int(variant_case["total_count"]), 10))
    (input_dir / f"input_parameter_{case_id:04d}.toml").write_text(
        serialize_case(case_id, variant_case),
        encoding="utf-8",
        newline="\n",
    )
    return case_variant_root, result_variant_root, variant_case


def newest_source_mtime() -> float:
    roots = [resolve_path("src"), resolve_path("include")]
    candidates = [resolve_path("CMakeLists.txt")]
    for root in roots:
        if root.exists():
            candidates.extend(path for path in root.rglob("*") if path.is_file())
    return max((path.stat().st_mtime for path in candidates if path.exists()), default=0.0)


def ensure_auto_solver_current(solver: Path) -> Path:
    if solver.exists() and solver.stat().st_mtime >= newest_source_mtime():
        return solver
    project_root = resolve_path(".")
    print("Auto-building missing or stale df2d before validation...")
    if solver.exists() and detect_elf(solver):
        project_root_wsl = to_wsl_path(project_root)
        command = [
            "wsl",
            "-e",
            "bash",
            "-lc",
            (
                f"cd {shlex.quote(project_root_wsl)} && "
                "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && "
                "cmake --build build --config Release --target df2d"
            ),
        ]
        completed = subprocess.run(
            command,
            cwd=str(project_root),
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    else:
        configure = ["cmake", "-S", str(project_root), "-B", str(project_root / "build"), "-DCMAKE_BUILD_TYPE=Release"]
        completed = subprocess.run(
            configure,
            cwd=str(project_root),
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if completed.returncode == 0:
            command = ["cmake", "--build", str(project_root / "build"), "--config", "Release", "--target", "df2d"]
            completed = subprocess.run(
                command,
                cwd=str(project_root),
                text=True,
                encoding="utf-8",
                errors="replace",
            )
    if completed.returncode != 0:
        raise RuntimeError("Auto-build failed before demo4 validation.")
    return locate_solver(None)


def locate_cuda_solver() -> Path | None:
    candidates = [
        resolve_path("build/Release/df2d_cuda.exe"),
        resolve_path("build/Debug/df2d_cuda.exe"),
        resolve_path("build/RelWithDebInfo/df2d_cuda.exe"),
        resolve_path("build/df2d_cuda.exe"),
        resolve_path("build/df2d_cuda"),
    ]
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate.resolve()
    return None


def resolve_validation_solver(explicit_solver: str | None, solver_backend: str = "auto", prefer_cuda: bool = False) -> Path:
    if explicit_solver:
        return locate_solver(explicit_solver)
    if solver_backend == "cuda":
        solver = locate_cuda_solver()
        if solver is None:
            raise FileNotFoundError("Could not find df2d_cuda solver for --solver-backend cuda.")
        return solver
    if solver_backend == "auto" and prefer_cuda:
        solver = locate_cuda_solver()
        if solver is not None:
            return solver
    try:
        solver = locate_solver(None)
    except FileNotFoundError:
        solver = resolve_path("build/df2d")
    return ensure_auto_solver_current(solver)


def copy_outputs(work_dir: Path, result_dir: Path, case_id: int) -> None:
    output_dir = work_dir / "output"
    if not output_dir.exists():
        return
    ensure_dir(result_dir)
    for name in [f"eta_ave_{case_id}.m", f"remarks_{case_id}.m", f"checkpoint_{case_id}.bin"]:
        src = output_dir / name
        if src.exists():
            shutil.copy2(src, result_dir / name)
    data_dir = output_dir / f"data_{case_id}"
    if data_dir.exists():
        dst = result_dir / f"data_{case_id}"
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(data_dir, dst)


def parse_remarks_metrics(remarks_path: Path) -> dict[str, float | int | None]:
    metrics: dict[str, float | int | None] = {name: None for name in REMARK_NUMERIC_FIELDS}
    if not remarks_path.exists():
        return metrics
    for raw_line in remarks_path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.split("%", 1)[0].strip()
        if "=" not in line:
            continue
        key, value_text = line.split("=", 1)
        key = key.strip()
        if key not in REMARK_NUMERIC_FIELDS:
            continue
        raw_value = value_text.strip().rstrip(";")
        try:
            numeric = float(raw_value)
        except ValueError:
            continue
        if key == "actual_iterations":
            metrics[key] = int(round(numeric))
        else:
            metrics[key] = numeric
    return metrics


def display_variant_name(variant: str, ny: int, *, ny_sweep_only: bool) -> str:
    if variant == "baseline":
        return "baseline"
    if ny_sweep_only:
        return f"ny_{ny}"
    if variant.startswith("ny_refined"):
        return f"ny_{ny}"
    if variant == "dt_ny_refined":
        return f"dt+ny_{ny}"
    return variant


def assign_display_variants(rows: list[dict[str, Any]]) -> None:
    ny_sweep_only = bool(rows) and all(str(row.get("variant", "")) in NY_SWEEP_VARIANTS for row in rows)
    for row in rows:
        ny = int(row.get("ny") or 0)
        row["display_variant"] = display_variant_name(str(row.get("variant", "")), ny, ny_sweep_only=ny_sweep_only)


def variant_metrics(
    case_id: int,
    advection_scheme: str,
    variant: str,
    run_info: dict[str, Any],
    result_dir: Path,
    variant_case: dict[str, Any],
) -> dict[str, Any]:
    eta_path = result_dir / f"eta_ave_{case_id}.m"
    remarks_path = result_dir / f"remarks_{case_id}.m"
    points = parse_eta_series(eta_path)
    remarks_metrics = parse_remarks_metrics(remarks_path)
    final_time = points[-1][0] if points else None
    final_eta = points[-1][1] if points else None
    final_deta = points[-1][2] if points else None
    elapsed_seconds = run_info["elapsed_seconds"]
    actual_iterations = remarks_metrics.get("actual_iterations")
    iterations_per_second = remarks_metrics.get("iterations_per_second")
    if iterations_per_second is None and isinstance(actual_iterations, int) and elapsed_seconds > 0:
        iterations_per_second = actual_iterations / elapsed_seconds
    return {
        "case_id": case_id,
        "advection_scheme": advection_scheme,
        "scheme_display": ADVECTION_SCHEME_LABELS.get(advection_scheme, advection_scheme),
        "variant": variant,
        "exit_code": run_info["exit_code"],
        "elapsed_seconds": elapsed_seconds,
        "ny": int(variant_case["ny"]),
        "coeff_dt": float(variant_case["coeff_dt"]),
        "actual_iterations": actual_iterations,
        "iterations_per_second": iterations_per_second,
        "final_time": final_time,
        "final_eta": final_eta,
        "final_deta_dt": final_deta,
        "eta_point_count": len(points),
        "dt_adjustment_count": count_dt_adjustments(remarks_path),
        "work_dir": run_info["cwd"],
        "eta_path": str(eta_path),
        "remarks_path": str(remarks_path),
        "stdout_tail": "\n".join(run_info["stdout"].splitlines()[-40:]),
        "stderr_tail": "\n".join(run_info["stderr"].splitlines()[-40:]),
    }


def reference_row_for(rows: list[dict[str, Any]]) -> dict[str, Any] | None:
    reference = None
    for variant in ["dt_ny_refined", "ny_refined", "dt_refined"]:
        reference = next((row for row in rows if row["variant"] == variant), None)
        if reference is not None:
            break
    return reference


def add_reference_differences(rows: list[dict[str, Any]]) -> None:
    assign_display_variants(rows)
    reference = reference_row_for(rows)
    reference_eta = reference.get("final_eta") if reference else None
    reference_variant = reference.get("variant") if reference else None
    reference_display_variant = reference.get("display_variant") if reference else None
    for row in rows:
        row["reference_variant"] = reference_variant
        row["reference_display_variant"] = reference_display_variant
        eta = row.get("final_eta")
        if isinstance(reference_eta, (int, float)) and isinstance(eta, (int, float)):
            row["final_eta_diff_abs"] = abs(eta - reference_eta)
            denom = max(abs(reference_eta), 1e-14)
            row["relative_final_eta_vs_dt_ny_refined"] = abs(eta - reference_eta) / denom
        else:
            row["final_eta_diff_abs"] = None
            row["relative_final_eta_vs_dt_ny_refined"] = None


def eta_points_for(row: dict[str, Any]) -> list[tuple[float, float, float]]:
    eta_path = row.get("eta_path")
    if not eta_path:
        return []
    return parse_eta_series(Path(str(eta_path)))


def interpolate_eta(points: list[tuple[float, float, float]], t_value: float) -> float | None:
    if not points:
        return None
    if t_value <= points[0][0]:
        return points[0][1]
    if t_value >= points[-1][0]:
        return points[-1][1]
    lo = 0
    hi = len(points) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if points[mid][0] <= t_value:
            lo = mid
        else:
            hi = mid
    t0, y0, _ = points[lo]
    t1, y1, _ = points[hi]
    if t1 == t0:
        return y0
    weight = (t_value - t0) / (t1 - t0)
    return y0 + weight * (y1 - y0)


def trapezoid_integral(times: list[float], values: list[float]) -> float:
    if len(times) < 2:
        return 0.0
    total = 0.0
    for index in range(1, len(times)):
        total += 0.5 * (values[index - 1] + values[index]) * (times[index] - times[index - 1])
    return total


def curve_metric_row(
    row: dict[str, Any],
    reference: dict[str, Any] | None,
    max_end_t: float | None,
) -> dict[str, Any]:
    metric: dict[str, Any] = {
        "case_id": row.get("case_id"),
        "advection_scheme": row.get("advection_scheme"),
        "scheme_display": row.get("scheme_display"),
        "variant": row.get("variant"),
        "display_variant": row.get("display_variant"),
        "reference_variant": reference.get("variant") if reference else None,
        "reference_display_variant": reference.get("display_variant") if reference else None,
        "comparison_status": "missing_reference" if reference is None else "missing_curve",
        "ny": row.get("ny"),
        "coeff_dt": row.get("coeff_dt"),
        "exit_code": row.get("exit_code"),
        "elapsed_seconds": row.get("elapsed_seconds"),
        "iterations_per_second": row.get("iterations_per_second"),
        "final_time": row.get("final_time"),
        "final_eta_diff_abs": row.get("final_eta_diff_abs"),
        "eta_point_count": row.get("eta_point_count"),
        "rmse_abs": None,
        "rmse_rel": None,
        "linf_abs": None,
        "linf_rel": None,
        "auc_abs_diff": None,
        "auc_rel_diff": None,
        "common_time_end": None,
    }
    if reference is None:
        return metric

    points = eta_points_for(row)
    ref_points = eta_points_for(reference)
    if len(points) < 1 or len(ref_points) < 1:
        return metric

    common_end = min(points[-1][0], ref_points[-1][0])
    metric["common_time_end"] = common_end
    if common_end <= 0.0:
        metric["comparison_status"] = "zero_common_time"
        return metric

    times = [point[0] for point in ref_points if 0.0 <= point[0] <= common_end]
    if not times or times[0] > 0.0:
        times.insert(0, 0.0)
    if times[-1] < common_end:
        times.append(common_end)
    times = sorted(set(times))
    if len(times) < 2:
        metric["comparison_status"] = "too_few_points"
        return metric

    diffs: list[float] = []
    rel_diffs: list[float] = []
    abs_ref_values: list[float] = []
    abs_diff_values: list[float] = []
    for t_value in times:
        eta = interpolate_eta(points, t_value)
        ref_eta = interpolate_eta(ref_points, t_value)
        if eta is None or ref_eta is None:
            continue
        diff = eta - ref_eta
        denom = max(abs(ref_eta), 1e-14)
        diffs.append(diff)
        rel_diffs.append(diff / denom)
        abs_ref_values.append(abs(ref_eta))
        abs_diff_values.append(abs(diff))

    if not diffs:
        metric["comparison_status"] = "no_overlap_samples"
        return metric

    metric["rmse_abs"] = math.sqrt(sum(value * value for value in diffs) / len(diffs))
    metric["rmse_rel"] = math.sqrt(sum(value * value for value in rel_diffs) / len(rel_diffs))
    metric["linf_abs"] = max(abs(value) for value in diffs)
    metric["linf_rel"] = max(abs(value) for value in rel_diffs)
    metric["auc_abs_diff"] = trapezoid_integral(times, abs_diff_values)
    ref_auc = trapezoid_integral(times, abs_ref_values)
    metric["auc_rel_diff"] = metric["auc_abs_diff"] / max(ref_auc, 1e-14)
    exit_code = row.get("exit_code")
    final_time = row.get("final_time")
    complete_by_time = (
        max_end_t is None or
        (isinstance(final_time, (int, float)) and final_time + 1e-12 >= float(max_end_t))
    )
    metric["comparison_status"] = "complete" if exit_code == 0 and complete_by_time else "partial"
    return metric


def add_curve_metrics(rows: list[dict[str, Any]], max_end_t: float | None) -> list[dict[str, Any]]:
    reference = reference_row_for(rows)
    return [curve_metric_row(row, reference, max_end_t) for row in rows]


def condition_key(row: dict[str, Any]) -> tuple[Any, str, int | None, float | None]:
    ny = row.get("ny")
    coeff_dt = row.get("coeff_dt")
    return (
        row.get("case_id"),
        str(row.get("variant", "")),
        int(ny) if isinstance(ny, (int, float)) else None,
        round(float(coeff_dt), 14) if isinstance(coeff_dt, (int, float)) else None,
    )


def scheme_comparison_row(
    upwind_row: dict[str, Any],
    tvd_row: dict[str, Any],
    max_end_t: float | None,
) -> dict[str, Any]:
    upwind_eta = upwind_row.get("final_eta")
    tvd_eta = tvd_row.get("final_eta")
    final_delta = (
        tvd_eta - upwind_eta
        if isinstance(upwind_eta, (int, float)) and isinstance(tvd_eta, (int, float))
        else None
    )
    metric = curve_metric_row(tvd_row, upwind_row, max_end_t)
    upwind_elapsed = upwind_row.get("elapsed_seconds")
    tvd_elapsed = tvd_row.get("elapsed_seconds")
    runtime_ratio = (
        tvd_elapsed / upwind_elapsed
        if isinstance(upwind_elapsed, (int, float))
        and isinstance(tvd_elapsed, (int, float))
        and upwind_elapsed > 0.0
        else None
    )
    return {
        "case_id": tvd_row.get("case_id"),
        "variant": tvd_row.get("variant"),
        "display_variant": tvd_row.get("display_variant"),
        "ny": tvd_row.get("ny"),
        "coeff_dt": tvd_row.get("coeff_dt"),
        "baseline_scheme": "upwind",
        "comparison_scheme": "tvd-mc",
        "comparison_status": metric.get("comparison_status"),
        "upwind_exit_code": upwind_row.get("exit_code"),
        "tvd_mc_exit_code": tvd_row.get("exit_code"),
        "final_time_upwind": upwind_row.get("final_time"),
        "final_time_tvd_mc": tvd_row.get("final_time"),
        "final_eta_upwind": upwind_eta,
        "final_eta_tvd_mc": tvd_eta,
        "final_eta_delta": final_delta,
        "final_eta_delta_abs": abs(final_delta) if isinstance(final_delta, (int, float)) else None,
        "final_eta_delta_rel": (
            abs(final_delta) / max(abs(upwind_eta), 1e-14)
            if isinstance(final_delta, (int, float)) and isinstance(upwind_eta, (int, float))
            else None
        ),
        "rmse_abs": metric.get("rmse_abs"),
        "rmse_rel": metric.get("rmse_rel"),
        "linf_abs": metric.get("linf_abs"),
        "linf_rel": metric.get("linf_rel"),
        "auc_abs_diff": metric.get("auc_abs_diff"),
        "auc_rel_diff": metric.get("auc_rel_diff"),
        "common_time_end": metric.get("common_time_end"),
        "upwind_elapsed_seconds": upwind_elapsed,
        "tvd_mc_elapsed_seconds": tvd_elapsed,
        "runtime_ratio_tvd_mc_vs_upwind": runtime_ratio,
    }


def add_scheme_comparisons(rows: list[dict[str, Any]], max_end_t: float | None) -> list[dict[str, Any]]:
    upwind_rows = {
        condition_key(row): row
        for row in rows
        if row.get("advection_scheme") == "upwind"
    }
    comparisons: list[dict[str, Any]] = []
    for tvd_row in rows:
        if tvd_row.get("advection_scheme") != "tvd-mc":
            continue
        upwind_row = upwind_rows.get(condition_key(tvd_row))
        if upwind_row is None:
            continue
        comparisons.append(scheme_comparison_row(upwind_row, tvd_row, max_end_t))
    return comparisons


def parse_advection_schemes(advection_scheme: str, advection_schemes: str | None) -> list[str]:
    raw_items = advection_schemes if advection_schemes is not None else advection_scheme
    schemes = [item.strip() for item in str(raw_items).split(",") if item.strip()]
    if not schemes:
        schemes = [advection_scheme]
    unknown = [scheme for scheme in schemes if scheme not in ADVECTION_SCHEME_LABELS]
    if unknown:
        raise ValueError(f"Unknown advection scheme(s): {', '.join(unknown)}")
    ordered: list[str] = []
    for scheme in schemes:
        if scheme not in ordered:
            ordered.append(scheme)
    return ordered


def validation_solver_args(advection_scheme: str, keep_solver_artifacts: bool) -> list[str]:
    args = [
        "--cpu-thread-mode", "internal",
        "--advection-scheme", advection_scheme,
    ]
    if not keep_solver_artifacts:
        args.extend([
            "--no-output-matlab",
            "--no-output-tecplot",
            "--disable-dense-dump",
            "--checkpoint-interval", str(VALIDATION_CHECKPOINT_INTERVAL),
        ])
    return args


def main() -> int:
    parser = argparse.ArgumentParser(description="demo4 baseline refinement validation.")
    parser.add_argument("--input-dir", default="demo4/input", help="Directory containing input_parameter_XXXX files.")
    parser.add_argument("--case", dest="cases", default="", help="Case list or ranges, for example 1,2,5-8.")
    parser.add_argument("--solver", default="", help="Path to df2d executable. Auto-detected when omitted.")
    parser.add_argument("--dt-refine", type=float, default=4.0, help="Divide coeff_dt by this factor.")
    parser.add_argument("--ny-refine", type=int, default=2, help="Multiply ny by this factor.")
    parser.add_argument("--variants", default=",".join(VARIANTS), help="Comma-separated variants to run.")
    parser.add_argument("--max-endT", type=float, default=None, help="Validation end time applied to generated validation cases.")
    parser.add_argument("--timeout-seconds", type=float, default=300.0, help="Per-variant solver timeout.")
    parser.add_argument("--kind", choices=["dt", "ny", "mixed"], default="", help="Validation run kind used for timestamped result archival.")
    parser.add_argument("--advection-scheme", choices=sorted(ADVECTION_SCHEME_LABELS), default="upwind", help="CPU advection scheme passed to the solver.")
    parser.add_argument("--advection-schemes", default="", help="Comma-separated CPU advection schemes to run under identical case/variant conditions.")
    parser.add_argument("--solver-backend", choices=["auto", "cpu", "cuda"], default="auto", help="Solver executable family used when --solver is omitted. Auto prefers CUDA for fine upwind grids when available.")
    parser.add_argument("--keep-solver-artifacts", action="store_true", help="Keep full field snapshots, dense dumps, and checkpoints during validation.")
    args = parser.parse_args()

    selected_variants = [item.strip() for item in args.variants.split(",") if item.strip()]
    unknown_variants = [item for item in selected_variants if item not in VARIANTS]
    if unknown_variants:
        raise ValueError(f"Unknown validation variant(s): {', '.join(unknown_variants)}")
    if not selected_variants:
        raise ValueError("--variants must contain at least one variant.")

    needs_dt_refine = any(variant in {"dt_refined", "dt_ny_refined"} for variant in selected_variants)
    needs_ny_refine = any(variant in {"ny_refined_1", "ny_refined_2", "ny_refined_3", "ny_refined", "dt_ny_refined"} for variant in selected_variants)
    if needs_dt_refine and (args.dt_refine <= 1.0 or not math.isfinite(args.dt_refine)):
        raise ValueError("--dt-refine must be greater than 1.")
    if needs_ny_refine and args.ny_refine <= 1:
        raise ValueError("--ny-refine must be greater than 1.")
    advection_schemes = parse_advection_schemes(args.advection_scheme, args.advection_schemes or None)
    use_scheme_subdir = len(advection_schemes) > 1
    input_dir = resolve_path(args.input_dir)
    available = discover_cases(input_dir)
    selected = parse_case_selection(args.cases, available)
    max_planned_ny = max_planned_ny_for_cases(selected, selected_variants, args.ny_refine)
    prefer_cuda = (
        args.solver_backend == "auto" and
        max_planned_ny >= CUDA_NY_AUTO_THRESHOLD and
        advection_schemes == ["upwind"]
    )
    solver = resolve_validation_solver(args.solver or None, args.solver_backend, prefer_cuda)
    validation_kind = args.kind or infer_validation_kind(selected_variants)
    run_id, run_root = unique_run_root(validation_kind)
    started_at = datetime.now().isoformat(timespec="seconds")
    run_summary_path = run_root / "run_summary.json"
    write_json(run_summary_path, {
        "run_id": run_id,
        "kind": validation_kind,
        "status": "running",
        "started_at": started_at,
        "finished_at": None,
        "input_dir": str(args.input_dir),
        "resolved_input_dir": str(input_dir),
        "cases": [case.case_id for case in selected],
        "max_endT": args.max_endT,
        "dt_refine": args.dt_refine,
        "ny_refine": args.ny_refine,
        "variants": selected_variants,
        "advectionScheme": advection_schemes[0],
        "advectionSchemes": advection_schemes,
        "schemeDisplay": ", ".join(ADVECTION_SCHEME_LABELS.get(scheme, scheme) for scheme in advection_schemes),
        "solverBackend": args.solver_backend,
        "maxPlannedNy": max_planned_ny,
        "solverArtifactMode": "full" if args.keep_solver_artifacts else "validation_minimal",
        "summary_files": {
            "validation_csv": "validation_summary.csv",
            "curve_metrics_csv": "curve_metrics_summary.csv",
            "scheme_comparison_csv": "scheme_comparison_summary.csv",
        },
    })

    all_rows: list[dict[str, Any]] = []
    all_curve_rows: list[dict[str, Any]] = []
    all_scheme_comparison_rows: list[dict[str, Any]] = []
    for case in selected:
        values = read_case(case.path)
        case_rows: list[dict[str, Any]] = []
        case_curve_rows: list[dict[str, Any]] = []
        case_result_root = run_root / f"case_{case.case_id:04d}"
        ensure_dir(case_result_root)
        case_variants = planned_variants_for_case(selected_variants, values, args.ny_refine)
        for advection_scheme in advection_schemes:
            scheme_rows: list[dict[str, Any]] = []
            for variant in case_variants:
                work_dir, result_dir, variant_case = prepare_variant(
                    case.case_id,
                    advection_scheme,
                    values,
                    variant,
                    run_root,
                    args.dt_refine,
                    args.ny_refine,
                    args.max_endT,
                    use_scheme_subdir,
                )
                extra = validation_solver_args(advection_scheme, args.keep_solver_artifacts)
                run_info = run_solver(solver, work_dir, case.case_id, extra, args.timeout_seconds)
                write_json(result_dir / "run_info.json", run_info)
                copy_outputs(work_dir, result_dir, case.case_id)
                row = variant_metrics(case.case_id, advection_scheme, variant, run_info, result_dir, variant_case)
                row["run_id"] = run_id
                row["validation_kind"] = validation_kind
                scheme_rows.append(row)
            add_reference_differences(scheme_rows)
            scheme_curve_rows = add_curve_metrics(scheme_rows, args.max_endT)
            for row in scheme_curve_rows:
                row["run_id"] = run_id
                row["validation_kind"] = validation_kind
            case_rows.extend(scheme_rows)
            case_curve_rows.extend(scheme_curve_rows)
        case_scheme_comparison_rows = add_scheme_comparisons(case_rows, args.max_endT)
        for row in case_scheme_comparison_rows:
            row["run_id"] = run_id
            row["validation_kind"] = validation_kind
        write_json(case_result_root / "validation_summary.json", {
            "run_id": run_id,
            "kind": validation_kind,
            "case_id": case.case_id,
            "source_path": str(case.path),
            "dt_refine": args.dt_refine,
            "ny_refine": args.ny_refine,
            "max_endT": args.max_endT,
            "advection_scheme": advection_schemes[0],
            "advection_schemes": advection_schemes,
            "scheme_display": ", ".join(ADVECTION_SCHEME_LABELS.get(scheme, scheme) for scheme in advection_schemes),
            "solver_artifact_mode": "full" if args.keep_solver_artifacts else "validation_minimal",
            "variants": case_rows,
            "curve_metrics": case_curve_rows,
            "scheme_comparisons": case_scheme_comparison_rows,
            "candidate_scheme_rows": [
                "Baseline Explicit",
                "High-Resolution Advection",
                "Semi-Lagrangian / IMEX",
            ],
            "selection_policy": "No automatic scheme selection is performed.",
        })
        all_rows.extend(case_rows)
        all_curve_rows.extend(case_curve_rows)
        all_scheme_comparison_rows.extend(case_scheme_comparison_rows)

    flat_rows = [
        {field: row.get(field, "") for field in SUMMARY_FIELDS}
        for row in all_rows
    ]
    flat_curve_rows = [
        {field: row.get(field, "") for field in CURVE_METRIC_FIELDS}
        for row in all_curve_rows
    ]
    flat_scheme_comparison_rows = [
        {field: row.get(field, "") for field in SCHEME_COMPARISON_FIELDS}
        for row in all_scheme_comparison_rows
    ]
    run_validation_csv = run_root / "validation_summary.csv"
    run_curve_csv = run_root / "curve_metrics_summary.csv"
    run_scheme_comparison_csv = run_root / "scheme_comparison_summary.csv"
    write_csv(run_validation_csv, flat_rows, SUMMARY_FIELDS)
    write_csv(run_curve_csv, flat_curve_rows, CURVE_METRIC_FIELDS)
    write_csv(run_scheme_comparison_csv, flat_scheme_comparison_rows, SCHEME_COMPARISON_FIELDS)
    write_csv(RESULTS_ROOT / "validation_summary.csv", flat_rows, SUMMARY_FIELDS)
    write_csv(RESULTS_ROOT / "curve_metrics_summary.csv", flat_curve_rows, CURVE_METRIC_FIELDS)
    write_csv(RESULTS_ROOT / "scheme_comparison_summary.csv", flat_scheme_comparison_rows, SCHEME_COMPARISON_FIELDS)
    finished_at = datetime.now().isoformat(timespec="seconds")
    run_summary = {
        "run_id": run_id,
        "kind": validation_kind,
        "status": "complete",
        "started_at": started_at,
        "finished_at": finished_at,
        "input_dir": str(args.input_dir),
        "resolved_input_dir": str(input_dir),
        "cases": [case.case_id for case in selected],
        "max_endT": args.max_endT,
        "dt_refine": args.dt_refine,
        "ny_refine": args.ny_refine,
        "variants": selected_variants,
        "advectionScheme": advection_schemes[0],
        "advectionSchemes": advection_schemes,
        "schemeDisplay": ", ".join(ADVECTION_SCHEME_LABELS.get(scheme, scheme) for scheme in advection_schemes),
        "solverBackend": args.solver_backend,
        "maxPlannedNy": max_planned_ny,
        "solverArtifactMode": "full" if args.keep_solver_artifacts else "validation_minimal",
        "run_root": str(run_root),
        "summary_files": {
            "validation_csv": "validation_summary.csv",
            "curve_metrics_csv": "curve_metrics_summary.csv",
            "scheme_comparison_csv": "scheme_comparison_summary.csv",
        },
    }
    write_json(run_summary_path, run_summary)
    latest_path = RESULTS_ROOT / "validation" / validation_kind / "latest.json"
    write_json(latest_path, {
        "run_id": run_id,
        "kind": validation_kind,
        "path": str(run_root),
        "relative_path": relative_result_path(run_root),
        "finished_at": finished_at,
        "status": "complete",
    })
    print(f"Validated {len(selected)} case(s) with {len(selected_variants)} variant(s) and {len(advection_schemes)} scheme(s) each.")
    print(f"Solver: {solver}")
    print(f"Run: {validation_kind}/{run_id}")
    print(f"Summary CSV: {run_validation_csv}")
    print(f"Curve metrics CSV: {run_curve_csv}")
    print(f"Scheme comparison CSV: {run_scheme_comparison_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
