---
version: alpha
name: ADR Solver Web UI
description: A restrained engineering control-room interface for a local 2D diffusion-convection-adsorption solver. The system favors dense, readable operational screens over marketing composition: white and gray surfaces, charcoal text, teal as the single primary action/focus accent, semantic status colors for run state, monospace logs, compact tables, and chart-first result inspection.

colors:
  canvas: "#f9fafb"
  surface: "#ffffff"
  surface-muted: "#f9fafb"
  surface-sunken: "#f3f4f6"
  ink: "#111827"
  ink-soft: "#1f2937"
  ink-muted: "#4b5563"
  ink-faint: "#6b7280"
  ink-disabled: "#9ca3af"
  border: "#e5e7eb"
  border-strong: "#d1d5db"
  accent: "#0f766e"
  accent-strong: "#115e59"
  accent-soft: "#5eead4"
  accent-bg: "#f0fdfa"
  success-bg: "#d1fae5"
  success-fg: "#047857"
  success-border: "#a7f3d0"
  running-bg: "#dbeafe"
  running-fg: "#1d4ed8"
  running-border: "#bfdbfe"
  warning-bg: "#fef3c7"
  warning-fg: "#b45309"
  warning-border: "#fde68a"
  error-bg: "#fee2e2"
  error-fg: "#b91c1c"
  error-border: "#fecaca"
  stopped-bg: "#e7e5e4"
  stopped-fg: "#57534e"
  stopped-border: "#d6d3d1"
  log-canvas: "#0f172a"
  log-ink: "#e2e8f0"

typography:
  display:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 30px
    fontWeight: 800
    lineHeight: 1.15
    letterSpacing: "-0.02em"
  section-title:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 18px
    fontWeight: 700
    lineHeight: 1.3
    letterSpacing: "-0.01em"
  card-title:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 14px
    fontWeight: 600
    lineHeight: 1.4
    letterSpacing: 0
  body:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 14px
    fontWeight: 400
    lineHeight: 1.5
    letterSpacing: 0
  body-sm:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 13px
    fontWeight: 400
    lineHeight: 1.45
    letterSpacing: 0
  caption:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 12px
    fontWeight: 600
    lineHeight: 1.3
    letterSpacing: 0
  metric:
    fontFamily: "-apple-system, BlinkMacSystemFont, Inter, Segoe UI, Roboto, PingFang SC, Microsoft YaHei, sans-serif"
    fontSize: 16px
    fontWeight: 700
    lineHeight: 1.25
    letterSpacing: "-0.01em"
  mono:
    fontFamily: "SF Mono, Monaco, Cascadia Code, Consolas, ui-monospace, monospace"
    fontSize: 12.5px
    fontWeight: 400
    lineHeight: 1.6
    letterSpacing: 0

rounded:
  sm: 6px
  md: 8px
  lg: 12px
  full: 9999px

spacing:
  xxs: 4px
  xs: 8px
  sm: 12px
  md: 16px
  lg: 24px
  xl: 32px
  page: 48px

components:
  page-shell:
    backgroundColor: "{colors.canvas}"
    maxWidth: 1560px
    padding: "32px 0 48px"
  panel:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.lg}"
    padding: 20px
    border: "1px solid {colors.border}"
  compact-card:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.md}"
    padding: "12px 14px"
    border: "1px solid {colors.border}"
  button-primary:
    backgroundColor: "{colors.ink}"
    textColor: "#ffffff"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "9px 14px"
    border: "1px solid {colors.ink}"
  button-accent:
    backgroundColor: "{colors.accent}"
    textColor: "#ffffff"
    rounded: "{rounded.md}"
    padding: "9px 14px"
    border: "1px solid {colors.accent}"
  button-secondary:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.md}"
    padding: "9px 14px"
    border: "1px solid {colors.border-strong}"
  button-danger:
    backgroundColor: "#dc2626"
    textColor: "#ffffff"
    rounded: "{rounded.md}"
    padding: "9px 14px"
    border: "1px solid #dc2626"
  text-input:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "8px 12px"
    border: "1px solid {colors.border-strong}"
  text-input-focused:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    border: "1px solid {colors.ink}"
    focusRing: "0 0 0 3px rgba(17, 24, 39, 0.08)"
  tab:
    backgroundColor: "transparent"
    textColor: "{colors.ink-faint}"
    typography: "{typography.body}"
    padding: "14px 12px"
    borderBottom: "2px solid transparent"
  tab-active:
    backgroundColor: "transparent"
    textColor: "{colors.ink}"
    borderBottom: "2px solid {colors.ink}"
  status-badge:
    rounded: "{rounded.full}"
    typography: "{typography.caption}"
    padding: "4px 10px"
  log-panel:
    backgroundColor: "{colors.log-canvas}"
    textColor: "{colors.log-ink}"
    typography: "{typography.mono}"
    rounded: "{rounded.md}"
    padding: "8px 0"
    border: "1px solid #1f2937"
  chart-card:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.md}"
    padding: 14px
    border: "1px solid {colors.border}"
---

## Overview

ADR's UI should feel like a local numerical experiment control room. It is not a landing page, product brochure, or decorative dashboard. The user is editing cases, compiling/running a solver, scanning logs, and inspecting field outputs, so the design must prioritize repeatable work: fast scanning, clear status, compact controls, stable chart dimensions, and no visual drama that competes with numerical data.

The closest references from `awesome-design-md` are IBM/Carbon for restrained enterprise controls and Mintlify for documentation/code readability. Use those ideas as a method, not as brand imitation.

## Principles

### Operational Density

- Favor dense but organized panels, tables, tabs, log panes, and chart regions.
- Do not create marketing-style hero sections, oversized cards, decorative gradients, or atmospheric backgrounds.
- Put the active task and current case above decorative context.
- Keep repeated workflows stable: selecting a case, editing parameters, running, watching logs, viewing results.

### Color Roles

- Use white and gray surfaces for the default UI.
- Use teal only for primary actions, selected case emphasis, focus, and live progress accents.
- Use semantic state colors only for run/build status: running blue, success green, warning amber, error red, stopped stone.
- Do not introduce a second brand accent. Data visualizations may use scientific color ramps when needed, but app chrome should remain restrained.

### Typography

- Use the system sans stack with Chinese fallbacks for all UI prose.
- Use the mono stack for logs, code, paths, case ids, and numeric diagnostics where alignment matters.
- Keep headings compact. Reserve the largest type for the page title only.
- Use tabular numerals for KPIs, progress, and result metadata when implementing CSS.

### Components

- Panels are one level deep. Do not put decorative cards inside decorative cards.
- Tabs use underline selection for top-level workflow navigation.
- Buttons use 8px radius. Primary action can be charcoal; run/batch actions can use teal; destructive actions use red.
- Inputs are compact, 40px-ish controls with clear focus rings.
- Logs are dark monospace panes with line classification, gutters, and copy/jump controls.
- Charts sit in bordered white cards with fixed/responsive heights so updates do not shift layout.

## Layout

- Desktop shell: two-column layout with a case browser sidebar and a main workflow area.
- Main workflow: tabbed panels for environment, warmup, case parameters, case generation, build/run, and results.
- Result views should be chart-first: concentration field gets the largest region; profiles and time series stack beside or below it.
- Tables should support horizontal scrolling rather than shrinking text below readable sizes.
- On mobile/tablet, collapse the sidebar above the workflow and stack chart regions.

## Responsive Behavior

- Below 1100px, collapse the two-column shell to one column.
- Below 640px, reduce panel padding, stack stats, make stage chips single-column, and preserve readable tap targets.
- Do not scale fonts with viewport width. Use fixed type sizes with breakpoint-specific adjustments only where necessary.
- Preserve chart aspect and control heights; avoid layout jumps when logs or status text update.

## Do

- Show actual solver artifacts: case ids, parameter names, build/run stages, logs, heatmaps, eta profiles, and output metadata.
- Keep labels close to controls and preserve domain names like `Pe`, `Pe2`, `Da`, `K0`, `eta`, and `coeff_dt`.
- Use concise bilingual labels where the current UI already does.
- Keep scrollable regions explicit for long case lists, tables, and logs.
- Use icons only when they make repeated controls faster to scan; pair unfamiliar icons with accessible labels or tooltips.

## Don't

- Do not turn the app into a marketing page.
- Do not use full-bleed photography, brand gradients, decorative blobs, glassmorphism, or large empty hero layouts.
- Do not use multiple competing accent colors in the UI chrome.
- Do not hide logs, parameters, or result details behind overly decorative summaries.
- Do not change numerical terms for friendlier copy if that makes them less precise.

## Agent Prompt Guide

When asking an agent to modify the UI, say:

```text
Use DESIGN.md. Keep the ADR Web UI as a dense engineering control room:
white/gray surfaces, teal as the single action accent, semantic status colors,
compact controls, monospace logs, and chart-first result inspection.
```

For new frontend work, implement one component or workflow at a time and verify it in the browser at desktop and mobile widths.
