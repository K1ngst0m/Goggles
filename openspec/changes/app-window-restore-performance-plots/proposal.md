## Why

The recent compositor-metrics update replaced the legacy `Render` / `Source` FPS labels with
`Game FPS` and `Compositor Latency`, but it also removed the historical plot lines from the
Application performance panel. That leaves the panel less useful during live debugging because the
numeric readouts no longer show short-term metric trends.

This change restores the missing plots now so the new gamer-facing metrics keep their current
compositor-based semantics without regressing the panel's visual history.

## Problem

- The Application performance panel currently shows only scalar `Game FPS` and `Compositor Latency`
  values.
- The compositor metrics path already retains bounded timing history internally, but the UI contract
  no longer exposes enough history to render live plots.
- Reintroducing the removed legacy metric path would conflict with the new compositor-owned metric
  model.

## Scope

- Restore a live `Game FPS` plot beneath the existing `Game FPS` text readout.
- Restore a live `Compositor Latency` plot beneath the existing latency text readout.
- Keep the compositor-sourced metric path as the only source of truth for both values and plots.
- Extend the runtime metric contract as needed so the Application window can render bounded history
  without depending on compositor-internal event logic.

## What Changes

- Update the Application performance panel requirements so it reports the current metrics and renders
  one live history plot for each metric.
- Update the compositor capture requirements so the runtime metrics contract includes the bounded
  history needed to drive those plots for the active capture target.
- Keep the current metric names and gamer-facing meanings unchanged.
- Preserve the removal of the old legacy `Render` / `Source` metrics path and plots.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `app-window`: the Application performance panel requirements change from text-only
  `Game FPS` / `Compositor Latency` reporting to text plus live historical plots for both metrics.
- `compositor-capture`: the compositor capture requirements change to publish the bounded history
  needed for those plots without reintroducing any legacy timing path.

## Non-goals

- Reintroduce the legacy `Render` / `Source` FPS labels, plots, or timing pipeline.
- Add new performance metrics beyond `Game FPS` and `Compositor Latency`.
- Redesign unrelated Application window layout or performance-panel controls.
- Expand `Compositor Latency` into end-to-end display or input latency.

## Impact

- Affected modules: `src/ui`, `src/app`, `src/compositor`, and `src/util`.
- Likely affected files: `src/ui/imgui_layer.cpp`, `src/ui/imgui_layer.hpp`,
  `src/app/application.cpp`, `src/compositor/compositor_state.hpp`,
  `src/compositor/compositor_present.cpp`, and `src/util/runtime_metrics.hpp`.
- Impacted OpenSpec specs: `openspec/specs/app-window/spec.md` and
  `openspec/specs/compositor-capture/spec.md`.
- No packaging, dependency, or external API changes are expected.

## Risks

- The runtime contract can become UI-shaped if history is exposed without a boundary-owned metrics
  structure.
- Plot restoration can accidentally drift back toward legacy semantics if the panel recomputes its
  own history instead of consuming compositor-owned history.
- The panel can show misleading history if the active-target reset behavior is not explicit when the
  capture target changes.

## Validation Plan

Verification contract:
- Baseline gates:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- Environment-agnostic automated checks:
  - `pixi run test -p asan`
- Environment-sensitive checks:
  - `pixi run test -p test` when the local runtime supports the compositor/UI path
- Manual fallback:
  - allowed only for validating visible Performance panel plot behavior when automated UI coverage is
    unavailable
  - record prerequisites, target-switch observations, and proof location
- Mandatory checks with no fallback:
  - build and static-analysis gates above
- Pass criteria:
  - the Application performance UI shows `Game FPS` and `Compositor Latency` text plus one live plot
    for each metric
  - the implementation continues to use the compositor-sourced metrics path only
  - `Game FPS` and `Compositor Latency` histories reset before accumulating samples for a new capture
    target
  - no legacy `Render` / `Source` metric labels or legacy timing fallback path return
