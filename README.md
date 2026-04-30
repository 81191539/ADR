# ADR

ADR is a 2D diffusion-convection-adsorption PDE solver. The core solver is written in C++17, supports CPU/OpenMP execution, and can optionally build a CUDA executable when a CUDA compiler is available. The repository also includes a local Python Web UI for editing cases, building/running the solver, and viewing generated results.

## Features

- 2D concentration-field simulation with diffusion, advection, and surface adsorption.
- Explicit time stepping with automatic time-step shrink/restart when NaNs are detected.
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
+-- input/            Case parameter files: input_parameter_XXXX.txt
+-- tests/            Catch-based CPU solver smoke tests
+-- webui/            Local Python/HTML/CSS/JS Web UI
+-- docs/             Project notes and improvement recommendations
+-- CMakeLists.txt    Cross-platform CMake build entry
`-- makefile          Lightweight Linux/WSL build entry
```

Generated files such as object files, executables, `output/`, `results_old/`, generated Doxygen HTML, and local archives are intentionally ignored by Git.

## Build

### CMake

```bash
cmake -S . -B build
cmake --build build
```

The CPU executable is built as `df2d`. If CUDA is available, CMake can also build `df2d_cuda`.

### Makefile

On Linux or WSL:

```bash
make clean
make
```

This builds the CPU executable `df2d` in the project root.

## Run

```bash
./df2d
```

The program reads case files from `input/` and writes results to `output/`.

Important: the current configuration in `include/config.h` uses a fixed case list. If `USE_CASE_LIST` is `true`, make sure `get_case_list()` points to case IDs that exist under `input/`. The repository currently includes `input_parameter_0001.txt` through `input_parameter_0004.txt`.

## Input Format

Each case file is a single-line legacy parameter record:

```text
legacy_marker lam Pe Pe2 eps Da K0 ny xpo_l xpo_r endT total_count coeff_dt x_ini_posi alpha
```

Example:

```text
1 0.033333 10 10 0.1 100 1 50 0.33333 0.66667 60 300 0.1 5 0.01
```

The Web UI writes files using the canonical name format:

```text
input/input_parameter_0001.txt
```

## Output

Typical output files are written under `output/`:

- `eta_ave_X.m`: eta average over time for case `X`.
- `remarks_X.m`: run summary and diagnostics.
- `data_X/cc_N.m`: concentration field snapshot.
- `data_X/ee_N.m`: surface coverage profile snapshot.
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
http://127.0.0.1:8123
```

The Web UI expects a WSL/Linux toolchain for building and running through `make clean && make` and `./df2d`.

## Tests

With CMake and testing enabled:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The current tests cover ghost-cell storage behavior and a basic finite-state CPU explicit step.

## Improvement Notes

See [docs/robustness_recommendations.md](docs/robustness_recommendations.md) for recommended changes to improve usability and robustness, including runtime case selection, stronger parameter validation, checkpoint defaults, build unification, and Web UI cleanup.
