# Demo5 Development Plan

## Purpose

Demo5 shifts the project from broad numerical-scheme exploration back to the
existing baseline solver. The goal is to determine which baseline results are
reliable by using local grid refinement, not by introducing a new time
integrator or advection algorithm.

The core question for demo5 is:

```text
Given the current baseline method, where and how much local mesh refinement is
needed before a result can be trusted?
```

## Motivation

Demo4 showed that the baseline upwind route can fail grid-independence checks
in oscillatory-advection cases. In particular, Pe2 sweep evidence showed a
systematic tendency for coarse baseline runs to overestimate `eta_ave` relative
to refined-grid references.

At the same time, project time and implementation risk make it impractical to
continue deep algorithmic development immediately. Larger route changes such
as Semi-Lagrangian / IMEX are therefore deferred.

Demo5 should use the current baseline as the fixed numerical method and focus
on validation through local resolution.

## Scope

In scope:

- local grid refinement around regions that control transport or adsorption;
- baseline-vs-refined comparisons for `eta_ave`, convergence behavior, and
  runtime;
- reliability classification for existing baseline results;
- compact case sets that can be run repeatedly.

Out of scope for demo5:

- new IMEX or Semi-Lagrangian implementations;
- broad replacement of the advection discretization;
- speculative solver architecture changes;
- full long-running batch validation unless a small representative set has
  already justified it.

## Reliability Classes

Demo5 should classify baseline results into three practical categories:

| Class | Meaning |
|---|---|
| Reliable baseline | The baseline result is effectively grid-independent for the output being inspected. |
| Locally refinable | The baseline result is not reliable as-is, but local grid refinement can produce a stable reference at acceptable cost. |
| Unreliable pending algorithm work | Local refinement is insufficient or impractical; the case should wait for later numerical-route development. |

## Validation Strategy

For each selected case, demo5 should compare:

- original baseline grid;
- one or more local-refinement configurations;
- a stronger reference when affordable.

The minimum reported metrics should be:

- final `eta_ave` and signed difference from reference;
- raw relative error when the reference is not near zero;
- absolute error and signal scale when the reference is near zero;
- runtime and iteration rate;
- dt adjustment or stability events;
- notes on whether refinement changes the physical interpretation.

Relative-error columns must state their denominator. Guarded denominators such
as `max(abs(reference), eps)` may be useful for display, but they must not be
presented as mathematical relative error.

## First Targets

Use the demo4 Pe2 evidence as the starting point:

- retain low-signal cases as near-zero sanity checks;
- use high-Pe2 cases to test whether local refinement can remove baseline
  overestimation of `eta_ave`;
- prioritize cases where absolute `eta_ave` is visible enough that numerical
  differences matter physically, not only as ratios against near-zero values.

The first demo5 milestone is a small table that answers:

```text
For each tested case, is the current baseline result reliable, locally
refinable, or unreliable pending future algorithm work?
```
