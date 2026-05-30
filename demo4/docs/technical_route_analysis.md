# demo4 Technical Route Analysis

Generated for demo4 on 2026-05-08.

demo4 evaluates the existing ADR route and candidate future routes without
automatically selecting a winner. Each case should show theoretical indicators
and validation measurements so a human can compare routes.

## Current Baseline

The current solver advances the coupled bulk-boundary system with:

- explicit Euler time integration,
- centered second-order diffusion,
- first-order upwind advection,
- explicit Langmuir surface coverage update,
- runtime stability checks that shrink `dt` and restart only after a nonphysical
  state is detected.

The runtime derives:

```text
h  = 1 / ny
nx = floor(ny / lam)
dt = coeff_dt * h^2
u(y,t) = Pe*y*(1-y) + Pe2*f(alpha, y, t)
T_osc = pi / (alpha^2 * Sc)
```

The important correction for demo4 is that the oscillatory contribution must be
estimated with `max|f(alpha)|`, not with a fixed value of one. For small alpha
this maximum is near one; for larger alpha it can be much smaller.

## Candidate Scheme Rows

The platform reports these rows for each case. It does not choose among them.

| Scheme row | Motivation | Expected benefit | Expected cost | Implementation risk | Metrics to inspect |
|---|---|---|---|---|---|
| Baseline Explicit | Preserve current solver exactly and quantify its limits. | Fastest to run for low-risk cases; no numerical-code changes. | Keeps first-order upwind diffusion and explicit CFL limits. | Low. Existing behavior remains the reference. | `dt/dt_diffusion`, `dt/dt_advection`, `dt/dt_osc`, numerical diffusion estimates, stability events, refinement differences. |
| High-Resolution Advection | Reduce artificial diffusion from first-order upwind while staying near the current stencil solver. | Lower spatial truncation error for high Pe or high Pe2 cases. | Still constrained by explicit time stepping unless paired with a later time integrator change. | Medium. Limiters or high-order boundary handling can affect positivity and ghost cells. | Grid-refinement sensitivity, negative concentration events, oscillation near fronts, final `eta_ave` differences. |
| Semi-Lagrangian / IMEX | Decouple advection transport from explicit CFL and move diffusion to a more permissive time integrator. | Potentially much larger stable steps in convection-dominated cases. | More code and interpolation machinery; harder CUDA migration. | High. Boundary handling, adsorption-zone transitions, interpolation diffusion, and validation complexity all increase. | Step-size sensitivity, interpolation smoothing, wall-clock per physical time, mass/front-position differences, dense snapshot consistency. |

## Precheck Metrics

For every case, demo4 computes and stores:

- `h`, `nx`, `xright`, and current `dt`,
- diffusion explicit limit `h^2/4`,
- advection explicit limit `h / (|Pe|/4 + |Pe2|*max|ff|)`,
- oscillatory sampling limit `T_osc/20`,
- eta explicit estimate `1 / (eps*Da*(1 + 1/K0))`,
- `max|ff(alpha)|` sampled from the same formula used by the solver,
- Poiseuille numerical diffusion estimate `|Pe|*h/8`,
- oscillatory numerical diffusion estimate `|Pe2|*max|ff|*h/2`,
- ratio columns comparing current `dt` to each limit,
- risk-item text describing which indicators exceeded thresholds.
- scheme result rows for all three candidates. Baseline rows contain current
  precheck metrics; the other rows contain theoretical expected results and
  explicitly state that no new implementation has been measured in demo4 first
  pass.

Risk items are descriptive flags only. They are not scheme-selection rules.

## Validation Design

The first validation reference is refinement-based. For a selected case,
demo4 creates four baseline runs:

| Variant | Change |
|---|---|
| baseline | Original case values. |
| dt_refined | `coeff_dt` divided by the dt refine factor. |
| ny_refined | `ny` multiplied by the ny refine factor; `coeff_dt` unchanged. |
| dt_ny_refined | Both refinements together. |

The comparison report lists:

- solver exit code and elapsed wall time,
- final `eta_ave` and last `d eta_ave / dt`,
- relative difference against `dt_ny_refined` when available,
- parsed stability/dt-adjustment counts from `remarks_XXXX.m`,
- output file locations.

For smoke tests, `validate_cases.py --max-endT VALUE` may cap only the generated
demo4 validation inputs. This keeps quick checks practical while leaving the
source case files unchanged.

These measurements provide evidence. They do not select the final numerical
scheme.

## Measured Pe2 Sweep Evidence

Run `ny/20260519_104848` and follow-up run `ny/20260526_175202` measured a
focused Pe2 sweep for the same baseline conditions:

```text
Pe = 10
alpha = 10
Sc = 16667
ny = 50 baseline
coeff_dt = 0.000236
maxEndT = 0.02
advection scheme = Baseline Upwind
```

The sweep isolates the oscillatory-flow strength `Pe2`. The first run covered
`Pe2 = 1e3 ... 1e8`; the second added
`Pe2 = 1.5e5, 2.620741e5, 4.578857e5, 8e5`.

Important correction: the UI/CSV column previously described as final-eta
relative error used a guarded denominator,
`max(abs(reference_eta), 1e-14)`. That value is useful as a near-zero scaled
error, but it is not the mathematical relative error when the refined
reference is much smaller than `1e-14`.

For `ny/20260519_104848`, the raw mathematical relative error
`abs(eta - eta_ref) / abs(eta_ref)` for the `ny = 50` baseline row is:

| Case | Pe2 | Reference ny | Baseline final eta | Baseline final eta relative error | Interpretation |
|---:|---:|---:|---:|---:|---|
| 3733 | `1e3` | 250 | `2.10416e-108` | `7.29408e30` | Reference is effectively zero; raw relative error explodes and is not useful by itself. |
| 3734 | `1e4` | 250 | `5.13197e-81` | `5.79903e45` | Reference is effectively zero; raw relative error explodes and is not useful by itself. |
| 3735 | `1e5` | 250 | `3.39177e-26` | `5.15049e41` | Reference is effectively zero; raw relative error explodes and is not useful by itself. |
| 3736 | `1e6` | 250 | `5.38095e-8` | `1.57874e9` | Absolute signal becomes visible, but the refined reference is still near zero. |
| 3737 | `1e7` | 250 | `1.18087e-4` | `42.5654` | Raw relative error is finite and clearly shows grid sensitivity. |
| 3738 | `1e8` | 250 | `1.95773e-3` | `11.9561` | Raw relative error remains O(10), with a larger absolute signal. |

Corrected conclusion for this parameter family:

```text
The current guarded-relative metric is not suitable for locating the onset
threshold while eta_ref is near zero. For Pe2 <= 1e6, the refined reference
is so small that raw relative error is dominated by division by a near-zero
quantity. Stronger evidence starts when the absolute eta signal is also
visible: Pe2 = 1e7 and 1e8 are clearly grid-sensitive in run
ny/20260519_104848.
```

This is a threshold statement for the measured early-time validation window,
not a universal Pe2 rule for every alpha, Sc, time horizon, or output metric.
Near-zero reference values require both absolute eta scale and raw relative
error to be inspected together; a guarded relative error alone can make the
onset look artificially harmless.

## Transition to Demo5

After the Pe2 sweep evidence above, demo4 has served its immediate purpose:
it exposed that the baseline upwind route can systematically overestimate
`eta_ave` and fail grid-independence checks for oscillatory-advection cases.

Because project time and development efficiency do not currently allow a deep
algorithmic investigation, further new-scheme work is deferred:

- High-resolution advection remains useful evidence for future work, but is
  not the next project focus.
- Semi-Lagrangian / IMEX and other larger numerical-route changes are left as
  later development topics.
- demo4 should be treated as the archived scheme-evaluation surface rather
  than the active implementation target.

The next workspace is demo5. Its focus is deliberately narrower:

```text
Use the existing baseline solver and local grid refinement to determine which
baseline results are reliable, and under what local-resolution conditions.
```

demo5 should not attempt to prove a new algorithm. Instead, it should build a
practical reliability map for current baseline output:

- identify cases where the existing baseline result is grid-independent enough
  to trust;
- identify cases where the baseline result is only usable after local mesh
  refinement around the active transport or adsorption region;
- identify cases where even local refinement is insufficient and the result
  must be marked unreliable until a later numerical scheme is developed.

This makes demo5 a validation-and-triage project for the current solver, while
IMEX and other algorithmic routes remain deferred research/development work.
