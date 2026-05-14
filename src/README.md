# src

This directory contains the C++ solver implementation.

- `main.cpp`: command-line entry point.
- `runtime.cpp`: case scanning, parameter loading, run orchestration, and stability handling.
- `solver.cpp`: core time-stepping and solve logic.
- `cpu_backend.cpp` and `cuda_backend.cu`: CPU/OpenMP and CUDA backend implementations.
- `io.cpp`, `checkpoint.cpp`, and `util.cpp`: output, checkpoint, and utility functions.

This is the main behavior surface of the program. After changing it, run the CMake build and `adr_solver_tests` at minimum.