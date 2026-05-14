# include

This directory contains C++ headers shared across solver modules.

- `config.h` and `params.h`: default parameters, runtime configuration, and input parameter structures.
- `types.h`: core data structures for grids, fields, and run logs.
- `backend.h`: CPU/CUDA backend abstraction.
- `solver.h` and `runtime.h`: solver-step and run-flow entry points.
- `io.h`, `checkpoint.h`, and `file_utils.h`: input/output, checkpoint, and filesystem helpers.

Changes here usually affect several `.cpp` files and tests, so update implementations and call sites together.