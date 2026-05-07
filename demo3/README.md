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

## Case TOML Reference

New cases should use TOML files named `input/input_parameter_XXXX.toml`.
`case_id` is metadata for humans and tools; the solver still uses the id from
the filename or `--case/--cases`.

`Sc` is optional and defaults to `16667`, but new generated cases write it
explicitly. The `[runtime]` table is also optional. If it is omitted, defaults
come from `include/config.h`. If both CLI flags and `[runtime]` set the same
field, the CLI flag wins.

Copyable reference:

```toml
case_id = 1
lam = 0.033333
Pe = 1000
Pe2 = 0.1
eps = 0.1
Da = 100
K0 = 1
ny = 8
xpo_l = 0.333333
xpo_r = 0.666667
endT = 0.002
total_count = 1
coeff_dt = 0.1
x_ini_posi = 5
alpha = 0.001
Sc = 16667

[runtime]
stats_interval = 1000
stability_check_interval = 100
checkpoint_interval = 50000
output_matlab = true
output_tecplot = false
enable_dense_dump = true
dense_dump_start = 1.0
dense_dump_count = 8
convergence_threshold = 0.001
```

The same reference is available as
`input/input_parameter_reference.toml`. Remove any `[runtime]` line you do not
want to override.
