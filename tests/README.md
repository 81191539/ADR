# tests

This directory contains Catch2-style C++ tests.

- `catch.hpp`: test framework header.
- `solver_cpu_test.cpp`: tests for CPU solving, parameter parsing, stability checks, CLI validation, and small end-to-end runs.

Common verification commands:

```powershell
cmake -S . -B build
cmake --build build --config Release --target adr_solver_tests
ctest --test-dir build -C Release --output-on-failure
```