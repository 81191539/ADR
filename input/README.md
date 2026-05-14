# input

This directory contains the main project simulation input files.

- `input_parameter_XXXX.toml`: preferred case format, also written by the Web UI.
- `input_parameter_XXXX.txt`: legacy single-line case format kept for compatibility.
- `input_parameter_reference.toml`: copyable TOML parameter template.

The solver scans this directory for `input_parameter_*.toml` and `input_parameter_*.txt`. If the same case exists in both formats, the TOML file takes precedence.