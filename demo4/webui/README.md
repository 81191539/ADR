# demo4 Web UI

This is the dedicated Web UI copy for demo4. It keeps the normal case editing, build/run, warmup, and result views, and adds a `demo4 evaluation` tab.

The demo4 tab can:

- run case-level prechecks through `demo4/tools/precheck.py`;
- run short baseline refinement validation through `demo4/tools/validate_cases.py`;
- generate a combined summary through `demo4/tools/report.py`;
- display candidate schemes, precheck rows, scheme result rows, and validation rows.

The UI lists scheme rows and result rows only. It does not choose a scheme.

The demo4 evaluation tab defaults to `demo4/input`, a local input snapshot copied from `demo3/input`. `demo4/output` is reserved for demo4-local output artifacts.