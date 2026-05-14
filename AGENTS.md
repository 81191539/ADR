# Agent Instructions

This file is the project-level instruction entry for Codex-style coding agents. Keep changes small, verifiable, and aligned with the existing ADR solver structure.

## Core Rules

- Clarify before changing physics, numerical defaults, input semantics, output formats, checkpoint behavior, or batch run scope.
- Prefer the smallest change that satisfies the request. Do not add speculative abstractions, new frameworks, or unused configurability.
- Touch only files directly related to the task. Do not drive-by refactor comments, formatting, generated outputs, backups, demo copies, or unrelated code.
- Follow the existing layout: headers in `include/`, implementations in `src/`, cases in `input/`, Catch tests in `tests/`, local UI in `webui/`.
- Preserve encoding conventions: UTF-8/LF for source and docs, CRLF for `.bat` and `.cmd`.
- If a change creates unused includes, variables, functions, scripts, or docs references, clean up only those introduced by the change.
- Report unrelated dead code or suspicious behavior instead of deleting it unless explicitly asked.

## Local Tooling

- Prefer direct project tools such as `rg`, `cmake`, `ctest`, `git`, and `apply_patch` in this repository.
- Avoid PowerShell text pipelines and redirects for file edits when encoding could matter; initialize PowerShell sessions as UTF-8 before command-line work.

## Verification

For non-trivial work, define the target behavior and a verification path before editing:

```text
1. Identify the behavior to change -> verify with a test, build, or minimal reproduction.
2. Make the smallest implementation change -> verify with the narrowest relevant command.
3. Check side effects -> verify the diff only contains necessary changes.
```

Useful checks:

- C++ core or parameter parsing: `cmake --build build --target adr_solver_tests`, then `ctest --test-dir build --output-on-failure`.
- Input files or runtime flow: run a minimal selected case; do not default to full long-running batches.
- Web UI: start the local server and check the affected interaction.
- Docs or scripts: check paths, commands, and encoding.

## UI Design

For Web UI or demo interface changes, read `DESIGN.md` before editing. Keep the UI as a dense engineering control room: compact controls, white/gray surfaces, teal action accent, semantic status colors, monospace logs, and chart-first result views.

See `docs/codex_project_guidelines.md` for the longer archived rationale.
