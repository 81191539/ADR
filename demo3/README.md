# ADR demo3

`demo3` is a fast batch-monitoring sandbox for validating Web UI dashboard changes with thousands of cases.

It contains 3731 pre-generated TOML cases:

- `alpha`: 41 log-spaced points from `1e-3` to `10`
- `Pe2`: 91 log-spaced points from `1e-1` to `1e8`
- `Pe`: fixed at `1000`
- `ny`: `8`
- `endT`: `0.002`
- `total_count`: `1`
- `coeff_dt`: `0.1`

The tiny `endT` and coarse grid are intentional. This demo is meant to stress batch orchestration, progress accounting, case-state visualization, and failure handling without waiting on expensive physics runs.

Start the local UI from the repository root:

```bat
start_demo3_webui.bat
```

Default address:

```text
http://127.0.0.1:8003
```

Regenerate the demo case set:

```powershell
powershell -ExecutionPolicy Bypass -File demo3\generate_demo3_cases.ps1
```
