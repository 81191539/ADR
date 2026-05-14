#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Any

from demo4_common import RESULTS_ROOT, candidate_scheme_rows, read_json, resolve_path, write_json


ADVECTION_SCHEME_LABELS = {
    "upwind": "Baseline Upwind",
    "tvd-mc": "High-Resolution (TVD-MC)",
}


def read_csv_rows(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def numeric_value(value: Any) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number


def reference_variant(rows: list[dict[str, Any]]) -> dict[str, Any] | None:
    for target in ["dt_ny_refined", "ny_refined", "dt_refined"]:
        for row in rows:
            if str(row.get("variant", "") or "") == target:
                return row
    return None


def display_variant_name(variant: str, ny: int | None, *, ny_sweep_only: bool) -> str:
    if variant == "baseline":
        return "baseline"
    if ny is None:
        return variant
    if ny_sweep_only or variant.startswith("ny_refined"):
        return f"ny_{ny}"
    if variant == "dt_ny_refined":
        return f"dt+ny_{ny}"
    return variant


def enrich_validation_rows(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    sweep_variants = {"baseline", "ny_refined_1", "ny_refined_2", "ny_refined_3", "ny_refined"}
    grouped: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        grouped.setdefault(str(row.get("case_id", "") or ""), []).append(row)
    for group_rows in grouped.values():
        ny_sweep_only = bool(group_rows) and all(str(row.get("variant", "")) in sweep_variants for row in group_rows)
        for row in group_rows:
            ny_value = numeric_value(row.get("ny"))
            ny_int = int(ny_value) if ny_value is not None else None
            scheme_name = str(row.get("advection_scheme", "") or "").strip() or "upwind"
            row["advection_scheme"] = scheme_name
            row["scheme_display"] = row.get("scheme_display") or ADVECTION_SCHEME_LABELS.get(scheme_name, scheme_name)
            if not str(row.get("display_variant", "") or "").strip():
                row["display_variant"] = display_variant_name(
                    str(row.get("variant", "") or ""),
                    ny_int,
                    ny_sweep_only=ny_sweep_only,
                )
        reference = reference_variant(group_rows)
        reference_eta = numeric_value(reference.get("final_eta")) if reference else None
        reference_variant_name = str(reference.get("variant", "") or "") if reference else ""
        reference_display_variant = str(reference.get("display_variant", "") or "") if reference else ""
        for row in group_rows:
            row["reference_variant"] = row.get("reference_variant") or reference_variant_name
            row["reference_display_variant"] = row.get("reference_display_variant") or reference_display_variant
            if not str(row.get("final_eta_diff_abs", "") or "").strip():
                eta_value = numeric_value(row.get("final_eta"))
                if reference_eta is not None and eta_value is not None:
                    row["final_eta_diff_abs"] = abs(eta_value - reference_eta)
    return rows


def enrich_curve_metric_rows(
    curve_rows: list[dict[str, Any]],
    validation_rows: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    validation_lookup = {
        (str(row.get("case_id", "") or ""), str(row.get("variant", "") or "")): row
        for row in validation_rows
    }
    for row in curve_rows:
        validation_row = validation_lookup.get((str(row.get("case_id", "") or ""), str(row.get("variant", "") or "")))
        if validation_row is None:
            continue
        for field in [
            "advection_scheme",
            "scheme_display",
            "display_variant",
            "reference_display_variant",
            "elapsed_seconds",
            "iterations_per_second",
            "final_eta_diff_abs",
        ]:
            if not str(row.get(field, "") or "").strip():
                row[field] = validation_row.get(field, "")
    return curve_rows


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a combined demo4 summary artifact.")
    parser.add_argument("--results-dir", default=str(RESULTS_ROOT), help="demo4 results directory.")
    parser.add_argument("--output", default=str(RESULTS_ROOT / "demo4_summary.json"), help="JSON output path.")
    args = parser.parse_args()

    results_dir = resolve_path(args.results_dir)
    output = resolve_path(args.output)
    precheck_summary = read_csv_rows(results_dir / "precheck_summary.csv")
    validation_summary = enrich_validation_rows(read_csv_rows(results_dir / "validation_summary.csv"))
    curve_metrics_summary = enrich_curve_metric_rows(
        read_csv_rows(results_dir / "curve_metrics_summary.csv"),
        validation_summary,
    )
    validation_details = []
    validation_root = results_dir / "validation"
    if validation_root.exists():
        for summary in sorted(validation_root.glob("case_*/validation_summary.json")):
            validation_details.append(read_json(summary))

    payload = {
        "candidate_scheme_rows": candidate_scheme_rows(),
        "precheck_summary_path": str(results_dir / "precheck_summary.csv"),
        "precheck_rows": precheck_summary,
        "validation_summary_path": str(results_dir / "validation_summary.csv"),
        "validation_rows": validation_summary,
        "curve_metrics_summary_path": str(results_dir / "curve_metrics_summary.csv"),
        "curve_metric_rows": curve_metrics_summary,
        "validation_details": validation_details,
        "selection_policy": "No automatic scheme selection is performed. Compare rows manually.",
    }
    write_json(output, payload)
    print(f"Wrote demo4 summary: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
