# demo4: Scheme Evaluation Platform

demo4 is an independent evaluation workspace for ADR numerical-route analysis.
It does not implement or select a new numerical scheme automatically. Instead,
it lists candidate schemes, computes case-level precheck metrics, runs baseline
refinement validation, and emits comparable results for manual review.

## Layout

- `docs/technical_route_analysis.md` explains the candidate routes and the
  metrics to inspect.
- `tools/precheck.py` reads root or demo input cases and writes diagnostic
  JSON/CSV output.
- `tools/validate_cases.py` creates baseline refinement variants and runs the
  current solver against them.
- `tools/report.py` summarizes precheck and validation outputs.
- `input/` is demo4's local input snapshot, initially copied from
  `demo3/input/`.
- `output/` is reserved for demo4-local solver or WebUI output artifacts.
- `cases/` stores generated validation inputs.
- `results/` stores generated diagnostics and validation outputs.
- `webui/` records the intended future WebUI integration surface.

## Quick Start

Run precheck on demo4's local input cases:

```powershell
python demo4/tools/precheck.py --input-dir demo4/input --cases 1,2,3,4,5,6
```

Run precheck on demo3 cases:

```powershell
python demo4/tools/precheck.py --input-dir demo3/input --cases 1,31,101,1001
```

Run baseline refinement validation for a small case:

```powershell
python demo4/tools/validate_cases.py --input-dir demo4/input --case 1 --solver build/Release/df2d.exe --max-endT 0.001
```

Create a combined summary:

```powershell
python demo4/tools/report.py
```

All outputs are descriptive. The tools intentionally do not emit an automatic
"best scheme" decision.

Each precheck JSON lists all candidate schemes and all scheme result rows.
Rows for schemes not implemented in the first pass are marked as theoretical
expected results rather than measured winners.
