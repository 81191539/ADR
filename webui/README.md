# PDE Solver Web UI

## Start

From the project root, run:

```bat
start_webui.bat
```

Or run the server directly:

```bat
python webui\server.py --open-browser
```

The default address is `http://127.0.0.1:8000`.

## What It Does

- Browse and edit `input/input_parameter_XXXX.toml`, while still reading legacy `.txt` cases.
- Configure and build `build/` with local CMake first, then fall back to WSL + CMake when needed.
- Run the selected case after a successful build with `df2d --case <id>`.
- Optionally force a fresh run by adding `--force-restart`.
- Show environment checks, build logs, and run logs.
- Parse `output/eta_ave_X.m`, `output/remarks_X.m`, `output/data_X/cc_N.m`, and `output/data_X/ee_N.m`.

## Notes

- `makefile` is kept only as a legacy Linux/WSL entry point; the Web UI no longer calls `make clean && make`.
- If neither local CMake nor WSL CMake is available, the UI can still edit cases and view historical results, but it cannot start new solver runs.
- Files should stay UTF-8. If PowerShell displays text incorrectly, adjust terminal encoding instead of rewriting file contents.