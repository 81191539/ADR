# Codex Project Guidelines

These guidelines archive the project-specific application of the Karpathy-inspired agent rules from `andrej-karpathy-skills`. They are written for Codex-style agents working in this ADR repository.

The goal is to reduce four common failure modes:

- wrong assumptions about numerical behavior or project intent;
- overcomplicated code and premature abstractions;
- unrelated edits to code, comments, formatting, generated files, or backups;
- changes that appear complete but have not been verified.

## 1. Clarify Before Coding

Do not silently assume behavior when a request touches:

- physical model equations or parameter meaning;
- default values in `include/config.h`;
- TOML or legacy input parsing;
- output file naming or MATLAB-compatible formats;
- checkpoint save/load behavior;
- CUDA versus CPU backend behavior;
- Web UI build/run flow;
- which cases should be run.

If a request could mean C++ core work, CUDA backend work, MATLAB plotting work, or Web UI work, state the interpretation before making changes. Ask when the impact could alter numerical results or user workflow.

## 2. Simplicity First

Use the smallest implementation that solves the current task.

- Do not add abstractions for one-off behavior.
- Do not add new frameworks, configuration systems, or build entry points unless the task requires them.
- Do not add flexibility for future use unless the future use is part of the request.
- Prefer existing project patterns over new style.

Project structure to preserve:

- `include/`: public declarations and shared types;
- `src/`: solver runtime, IO, checkpointing, and backend implementations;
- `input/`: case definitions;
- `tests/`: Catch-based tests;
- `webui/`: local Python/HTML/CSS/JS interface;
- `docs/`: project notes and archived guidance.

## 3. Surgical Changes

Every changed line should trace back to the user's request.

- Do not reformat unrelated code.
- Do not rewrite comments unless they are wrong because of the change.
- Do not touch generated outputs, backup directories, build directories, or demo copies unless the task explicitly targets them.
- Match existing style, even when another style would also be reasonable.
- Clean up unused code only when your own change made it unused.
- Mention unrelated dead code or suspicious behavior instead of deleting it.

Encoding matters in this repository:

- source and docs should stay UTF-8/LF;
- `.bat` and `.cmd` files should stay CRLF;
- do not rewrite files through tools that may convert them to GBK/ANSI.

## 4. Goal-Driven Verification

For non-trivial tasks, translate the request into a concrete goal and verification loop:

```text
1. Identify the behavior to change -> verify with a test, build, or minimal reproduction.
2. Make the smallest implementation change -> verify with the narrowest relevant command.
3. Check side effects -> verify the diff only contains necessary changes.
```

Recommended checks:

- C++ core, parser, or runtime behavior:
  `cmake --build build --target adr_solver_tests`
  `ctest --test-dir build --output-on-failure`
- Input files or runtime case selection:
  run the smallest selected case that exercises the change.
- Web UI:
  start the local server and check the affected interaction, not only static files.
- Documentation or scripts:
  check paths, commands, and encoding.

Avoid defaulting to full long-running case batches when a smaller case proves the change.

## 5. Working With Existing Local State

This repository may contain untracked generated files, local demo copies, logs, build outputs, and user edits.

- Inspect status before editing.
- Do not revert user changes unless explicitly asked.
- Ignore unrelated dirty files.
- If an existing local change touches the same file, read carefully and work with it.

The short operational version of this document lives in the root `AGENTS.md`.
