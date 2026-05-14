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
