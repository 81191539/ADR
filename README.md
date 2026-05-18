# ADR

ADR is a 2D diffusion-convection-adsorption PDE solver. The core solver is written in C++17, supports CPU/OpenMP execution, and can optionally build a CUDA executable when a CUDA compiler is available. The repository also includes a local Python Web UI for editing cases, building/running the solver, and viewing generated results.

For installation prerequisites and quick setup commands, see [ENVIRONMENT.md](ENVIRONMENT.md).

## Features

- 2D concentration-field simulation with diffusion, advection, and surface adsorption.
- Explicit time stepping with automatic time-step shrink/restart when unstable values are detected.
- CPU/OpenMP backend.
- Optional CUDA backend through CMake when CUDA is available.
- MATLAB-style output files for concentration snapshots, eta profiles, and eta convergence history.
- Binary checkpoint save/load support.
- Local Web UI for case editing, build/run control, logs, and simple result visualization.

## Repository Layout

```text
.
+-- include/          Core headers, configuration, field types, backend interfaces
+-- src/              Solver runtime, CPU/CUDA backends, IO, checkpoint logic
+-- input/            Case parameter files: input_parameter_XXXX.toml or .txt
+-- tests/            Catch-based CPU solver smoke tests
+-- webui/            Local Python/HTML/CSS/JS Web UI
+-- docs/             Project notes and improvement recommendations
+-- analysis/         MATLAB analysis and plotting helpers
+-- demo1..demo4/     Preserved demo workspaces and validation scenarios
+-- CMakeLists.txt    Cross-platform CMake build entry
`-- makefile          CMake shim for common build/test targets
```

Generated files such as object files, executables, `output/`, `results_old/`,
generated Doxygen HTML, and local archives are intentionally ignored by Git.
Historical build products, run outputs, backup snapshots, and report PDFs may
be kept under `archive/` for local reference; `archive/` is not part of the
source layout.

## Build

### CMake

```bash
cmake -S . -B build
cmake --build build --config Release --target df2d
```

The CPU executable is built under `build/`. If CUDA is available, CMake can also build `df2d_cuda`.

### Makefile Shim

On systems with `make` available:

```bash
make
make test
```

The Makefile delegates to CMake so source lists are maintained in one place.

## Run

```bash
./df2d
```

The program reads case files from `input/` and writes results to `output/`.
By default, it scans every `input_parameter_*.toml` and `input_parameter_*.txt` file under `input/`. If both formats exist for the same case id, the TOML file is used.

You can also select cases at runtime:

```bash
./df2d --case 1
./df2d --cases 1,2,3
./df2d --case 1 --force-restart
```

`--force-restart` ignores any existing checkpoint and starts the selected case from scratch.
The static case list in `include/config.h` remains available as a compatibility mode when `USE_CASE_LIST` is enabled.

## Input Format

TOML is the preferred case format:

```toml
case_id = 1
lam = 0.033333
Pe = 10
Pe2 = 10
eps = 0.1
Da = 100
K0 = 1
ny = 50
xpo_l = 0.33333
xpo_r = 0.66667
endT = 60
total_count = 300
coeff_dt = 0.1
x_ini_posi = 5
alpha = 0.01
Sc = 16667

[runtime]
stats_interval = 1000
stability_check_interval = 100
checkpoint_interval = 50000
output_matlab = true
output_tecplot = false
enable_dense_dump = true
dense_dump_start = 4.0
dense_dump_count = 16
convergence_threshold = 0.001
```

`case_id` is metadata; the solver uses the id from the filename or from
`--case/--cases`. `Sc` is optional and defaults to `16667`, but new TOML files
should write it explicitly. The `[runtime]` table is optional. If a field is
omitted, the default comes from `include/config.h`; if a CLI flag sets the same
field, the CLI value wins. A copyable reference lives at
`input/input_parameter_reference.toml`.

The legacy single-line parameter record remains supported:

```text
legacy_marker lam Pe Pe2 eps Da K0 ny xpo_l xpo_r endT total_count coeff_dt x_ini_posi alpha
```

Example:

```text
1 0.033333 10 10 0.1 100 1 50 0.33333 0.66667 60 300 0.1 5 0.01
```

The Web UI writes TOML files using the canonical name format:

```text
input/input_parameter_0001.toml
```

## Output

Typical output files are written under `output/`:

- `eta_ave_X.m`: eta average over time for case `X`.
- `remarks_X.m`: run summary and diagnostics.
- `data_X/cc_N.m`: concentration field snapshot.
- `data_X/ee_N.m`: surface coverage profile snapshot.
- `data_X_dense/run_TIMESTAMP/cc_INDEX_tTIME_itITER.m`: dense dump snapshots when enabled.
- `checkpoint_X.bin`: binary checkpoint for restart/continuation.

## Web UI

Start the local UI from the project root:

```bat
start_webui.bat
```

Or run it directly:

```bat
python webui\server.py --open-browser
```

Default address:

```text
http://127.0.0.1:8000
```

The Web UI builds with CMake first. It uses native `cmake` when available, then falls back to WSL with `cmake -S . -B build` and `cmake --build build --config Release --target df2d`. It runs the selected case with `--case <id>`, and the "run from scratch" option passes `--force-restart`.

## Tests

With CMake and testing enabled:

```bash
cmake -S . -B build
cmake --build build --target adr_solver_tests
ctest --test-dir build --output-on-failure
```

The tests cover field storage, CPU finite-state stepping, stability checks
including eta x-dimension scanning, TOML runtime overrides, CLI validation,
tiny end-to-end runs, and dense dump run directory behavior.

## Agent Guidelines

Codex-style project instructions live in `AGENTS.md`. The longer archived
rationale is in `docs/codex_project_guidelines.md`. UI design guidance for
agents lives in `DESIGN.md`.

## Encoding

Source files and docs are UTF-8. The checked-in `.editorconfig` and `.gitattributes` keep text files on UTF-8/LF, while `.bat` and `.cmd` files stay CRLF for Windows.

On Windows, launch the Web UI through the provided `.bat` files so `chcp 65001`, `PYTHONUTF8=1`, and `PYTHONIOENCODING=utf-8` are set consistently. If Chinese comments display as mojibake in an interactive PowerShell session, run:

```powershell
chcp 65001
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()
```

Check the repository with:

```powershell
powershell -ExecutionPolicy Bypass -File tools\check_encoding.ps1
```

Do not convert repository files to GBK/ANSI or use legacy PowerShell redirection to rewrite source files.

## Improvement Notes

See [docs/robustness_recommendations.md](docs/robustness_recommendations.md) for recommended changes to improve usability and robustness, including runtime case selection, stronger parameter validation, checkpoint defaults, build unification, and Web UI cleanup.
