# PDE Solver Web UI

## Start

From the $demo workspace root, run the matching start script from the repository root, or run the server directly inside this demo:

`at
python webui\server.py --open-browser
`

The default address is http://127.0.0.1:8000 unless the wrapper script selects another port.

## What It Does

- Browse and edit input/input_parameter_XXXX.toml, while still reading legacy .txt cases.
- Configure and build the demo with CMake when available.
- Run the selected case with df2d --case <id>.
- Optionally force a fresh run by adding --force-restart.
- Show environment checks, build logs, run logs, and parsed result files under output/.

## Notes

This Web UI copy belongs to demo2. It may preserve historical behavior that differs from the main webui/ directory.