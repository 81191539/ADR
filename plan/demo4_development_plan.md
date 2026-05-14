# Demo4 Development Plan

## Purpose

Demo4 is the scheme evaluation platform for ADR numerical-route work. Its core function is not just running validation cases, but making competing numerical schemes visible, measurable, and comparable.

The platform should help a human answer:

- What numerical scheme is being considered?
- What benefit is it expected to bring?
- What cost and implementation risk does it introduce?
- What evidence has demo4 measured for or against it?

Demo4 should not silently choose a winner. It should present comparable evidence.

## Core Feature: Candidate Schemes

Candidate Schemes is the central demo4 feature and must become a first-class development surface.

| scheme | benefit | cost | risk |
|---|---|---|---|
| Baseline Explicit | No numerical-code changes; provides current behavior and a reference row. | Keeps explicit CFL limits and first-order upwind diffusion. | Low |
| High-Resolution Advection | Targets artificial diffusion in convection-dominated cases. | Does not by itself remove explicit time-step limits. | Medium |
| Semi-Lagrangian / IMEX | Targets advection CFL and diffusion step-size limits. | Adds interpolation and implicit-solver complexity. | High |

## Current Gap

Candidate Schemes exists conceptually in `demo4/docs/technical_route_analysis.md`, but it has not been brought fully onto the development table:

- The UI currently treats scheme rows as secondary descriptive output.
- Validation mostly measures baseline refinement behavior.
- Non-baseline candidate schemes are not connected to concrete implementation status, measured evidence, or pass/fail diagnostics.
- The user cannot yet compare scheme candidates through a clear workflow.

## Current Baseline Evidence

Baseline Explicit has preliminary Ny-refinement evidence for selected cases:

| case | status | evidence |
|---|---|---|
| case1 | basically passed | User visually inspected eta-t curves at different Ny values and saw almost no visible difference. |
| case3 | basically passed | User visually inspected eta-t curves at different Ny values and saw almost no visible difference. |
| case2 | confirmed Ny-sensitive | User checked part of the Ny-refinement path. The eta-t curves for `ny = 50` and `ny = 71` differ greatly. Although `ny = 150` is still unavailable, reducing `endT` allowed `ny = 114` to complete, and that result is still far from baseline. This confirms grid size has a very large impact on high-Pe2 case accuracy. |
| case4 | basically passed | User visually inspected eta-t curves at different Ny values and saw almost no visible difference. |

These observations are useful working evidence, but they should later be backed by clearer demo4 metrics and plots: maximum error, final eta difference, runtime, and readable overlay curves.

Case2 changes the development priority. Baseline Explicit is no longer just a performance problem there; it exposes a real grid-sensitivity / numerical-accuracy problem for high-Pe2 cases. Future work should therefore shift toward new numerical schemes rather than continuing to treat Baseline Explicit as sufficient after more expensive Ny refinement.

## Development Workstreams

### 1. Scheme Model

Define a stable demo4 scheme model used by tools, reports, and WebUI.

Minimum fields:

- scheme id
- display name
- benefit
- cost
- risk
- implementation status: theoretical, implemented, measured, blocked
- expected target: artificial diffusion, CFL limit, diffusion step-size limit, stability, runtime
- evidence summary
- result links

Acceptance:

- Candidate scheme data is generated from one source of truth.
- WebUI, report JSON, and markdown report show the same scheme names and statuses.

### 2. Baseline Evidence

Make Baseline Explicit the measured reference row.

Required evidence:

- current `dt` and limiting ratios
- max error and refinement differences
- runtime and iterations per second
- stability or nonphysical-state events
- output path for reproducible inspection

Acceptance:

- Baseline row clearly states what was actually measured.
- Validation output makes it obvious which baseline result is the reference.
- Case1, case3, and case4 are recorded as preliminary Ny-validation passes.
- Case2 is recorded as confirmed Ny-sensitive: `ny = 50`, `ny = 71`, and shortened `ny = 114` evidence shows large eta-t differences, so high-Pe2 accuracy requires a better numerical route.

### 3. High-Resolution Advection Track

Plan and implement this only after confirming the numerical change scope.

Questions to resolve before implementation:

- Which limiter or high-resolution stencil is acceptable for the ADR boundary conditions?
- How should positivity be preserved?
- Which cases demonstrate reduced artificial diffusion without destabilizing the run?

Required evidence once implemented:

- same case, same physical end time comparison against Baseline Explicit
- max error / relative error versus refined reference
- front shape or dense snapshot comparison where relevant
- runtime overhead

Acceptance:

- Candidate row moves from theoretical to measured only when actual solver output exists.

### 4. Semi-Lagrangian / IMEX Track

Plan and implement this only after confirming the numerical route and validation target.

Questions to resolve before implementation:

- Which part moves implicit first: diffusion, advection, or coupled bulk-boundary update?
- What interpolation strategy is acceptable near walls and adsorption boundaries?
- What validation case proves larger stable time steps without unacceptable smoothing?

Required evidence once implemented:

- stable run at larger effective time step
- wall-clock comparison per physical time
- max error / dense snapshot comparison against refined reference
- notes on interpolation diffusion or implicit solve residuals

Acceptance:

- Candidate row shows measured benefit, measured cost, and remaining implementation risk.

### 5. Evaluation UI

Candidate Schemes should be a primary view in demo4 Evaluation.

UI requirements:

- Candidate scheme table appears as a top-level summary, not buried under logs.
- Each scheme row shows status: theoretical, measured, failed, or blocked.
- Baseline evidence is visually separated from future-route hypotheses.
- Metrics use intuitive labels first: max error, runtime, iterations per second, stability events.
- Advanced metrics such as RMS, AUC, and relative variants move to details or tooltips.
- Variant names use concrete values such as `ny_50`, `ny_100`, `ny_150`, not opaque labels such as `ny_refined_3`.
- Validation Progress includes actual elapsed `time (s)`.

Acceptance:

- A user can compare all three schemes without reading raw JSON or logs.
- The UI makes clear which rows are measured and which rows are only proposed.

### 6. Validation Performance

Demo4 validation must be fast enough to support routine scheme comparison.

Current concern:

```text
ny = 50 * 3
iterations_per_second = 566.6
```

This is too slow for interactive validation and must be optimized by at least one order of magnitude before broad scheme comparisons are practical.

Acceptance:

- `ny = 150` validation has a recorded before/after benchmark.
- Runtime bottleneck is identified before deeper numerical route work starts.
- Validation progress reports actual elapsed time per variant.

## Updated Development Priority

The development center of gravity should move from Baseline Explicit validation toward new numerical schemes. Baseline Explicit remains the measured reference, but case2 shows it is not reliable enough for high-Pe2 cases without much finer grids and impractical computation cost.

Recommended priority:

1. Keep Candidate Schemes and validation archival stable as the comparison framework.
2. Use case1, case3, and case4 as Baseline Explicit sanity references.
3. Use case2 as the stress case that motivates new scheme development.
4. Start with High-Resolution Advection because it directly targets first-order upwind artificial diffusion and grid sensitivity.
5. Move to Semi-Lagrangian / IMEX after the advection-discretization route is measurable, because it introduces larger changes in time stepping, interpolation, and implicit solve behavior.

## Suggested Build Order

1. Promote Candidate Schemes to the top of demo4 Evaluation.
2. Make Baseline Explicit evidence clean, readable, and reproducible.
3. Fix variant naming and progress timing.
4. Add scheme status and evidence fields to report JSON.
5. Use case2 high-Pe2 Ny sensitivity as the primary stress case for new scheme work.
6. Implement and measure High-Resolution Advection first.
7. Evaluate Semi-Lagrangian / IMEX after High-Resolution Advection has measured evidence.

## Open Questions

- Should Candidate Schemes live in a JSON/YAML source file, Python constant, or markdown-derived table?
- Should demo4 keep three fixed schemes, or allow experimental scheme rows to be added later?
- What is the minimum benchmark set for moving a candidate from theoretical to measured?
- Which refined result should be treated as the reference for each metric?
