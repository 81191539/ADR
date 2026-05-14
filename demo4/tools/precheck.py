#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from demo4_common import (
    RESULTS_ROOT,
    discover_cases,
    flatten_precheck,
    parse_case_selection,
    precheck_metrics,
    read_case,
    resolve_path,
    write_csv,
    write_json,
)


SUMMARY_FIELDS = [
    "case_id",
    "source_path",
    "lam",
    "Pe",
    "Pe2",
    "alpha",
    "Sc",
    "ny",
    "nx",
    "h",
    "dt_current",
    "dt_diffusion_limit",
    "dt_advection_limit",
    "dt_oscillation_T_over_20",
    "dt_eta_estimate",
    "ratio_dt_to_diffusion",
    "ratio_dt_to_advection",
    "ratio_dt_to_oscillation",
    "ratio_dt_to_eta",
    "max_abs_ff_alpha",
    "u_poiseuille_estimate",
    "u_oscillatory_estimate",
    "u_max_estimate",
    "mesh_pe_poiseuille",
    "mesh_pe_oscillatory",
    "poiseuille_upwind_diffusion",
    "oscillatory_upwind_diffusion",
    "total_upwind_diffusion",
    "risk_items",
]


def main() -> int:
    parser = argparse.ArgumentParser(description="demo4 case precheck metrics.")
    parser.add_argument("--input-dir", default="demo4/input", help="Directory containing input_parameter_XXXX files.")
    parser.add_argument("--cases", default="", help="Case list or ranges, for example 1,2,5-8. Empty means all.")
    parser.add_argument("--output-dir", default=str(RESULTS_ROOT / "precheck"), help="Per-case JSON output directory.")
    parser.add_argument("--summary", default=str(RESULTS_ROOT / "precheck_summary.csv"), help="CSV summary path.")
    args = parser.parse_args()

    input_dir = resolve_path(args.input_dir)
    available = discover_cases(input_dir)
    selected = parse_case_selection(args.cases, available)
    output_dir = Path(args.output_dir)
    if not output_dir.is_absolute():
        output_dir = resolve_path(output_dir)
    summary_path = Path(args.summary)
    if not summary_path.is_absolute():
        summary_path = resolve_path(summary_path)

    rows = []
    for case in selected:
        values = read_case(case.path)
        metrics = precheck_metrics(case.case_id, values)
        metrics["source_path"] = str(case.path)
        write_json(output_dir / f"case_{case.case_id:04d}.json", metrics)
        rows.append(flatten_precheck(metrics, case.path))

    write_csv(summary_path, rows, SUMMARY_FIELDS)
    print(f"Prechecked {len(rows)} case(s).")
    print(f"JSON output: {output_dir}")
    print(f"Summary CSV: {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
