# Environment Setup

This project needs a C++ build environment for the solver and Python for the
local Web UI. CUDA and Node.js tooling are optional.

## Required

- CMake 3.21 or newer.
- A C++17 compiler with OpenMP support.
  - Windows: Visual Studio Build Tools or Visual Studio with the C++ desktop workload.
  - Linux/WSL: GCC or Clang with OpenMP support.
- Python 3 for the local Web UI.

## Optional

- CUDA Toolkit, only if you want to build the CUDA executable `df2d_cuda`.
- WSL with CMake, used by the Web UI as a fallback when local Windows CMake is not available.
- Node.js, only if you need the Playwright developer dependency from `package.json`.

## Windows Quick Setup

1. Install CMake:

   ```powershell
   winget install Kitware.CMake
   ```

2. Install Visual Studio Build Tools with the C++ desktop workload:

   ```powershell
   winget install Microsoft.VisualStudio.2022.BuildTools
   ```

   In the installer, select **Desktop development with C++**.

3. Install Python:

   ```powershell
   winget install Python.Python.3
   ```

4. Optional, install CUDA Toolkit if GPU builds are needed:

   ```powershell
   winget install Nvidia.CUDA
   ```

After installation, open a new terminal so `cmake`, the compiler, and `python`
are available on `PATH`.

## Linux or WSL Quick Setup

Ubuntu or Debian:

```bash
sudo apt update
sudo apt install -y build-essential cmake python3
```

If OpenMP headers or libraries are missing:

```bash
sudo apt install -y libomp-dev
```

## Verify The Environment

From the project root:

```powershell
cmake --version
python --version
cmake -S . -B build
cmake --build build --config Release --target df2d
```

Run tests when needed:

```powershell
cmake --build build --config Release --target adr_solver_tests
ctest --test-dir build -C Release --output-on-failure
```

## Start The Web UI

On Windows, prefer the provided launcher because it sets UTF-8 terminal options:

```bat
start_webui.bat
```

Or run the server directly:

```bat
python webui\server.py --open-browser
```

The default address is:

```text
http://127.0.0.1:8000
```

## Notes

- Source files and docs are UTF-8.
- `.bat` and `.cmd` files use CRLF line endings.
- The Web UI builds with local CMake first, then falls back to WSL + CMake.
- If CUDA is not installed, CMake still builds the CPU executable `df2d`; it only skips `df2d_cuda`.
