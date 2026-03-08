## Why

The current `Application -> Performance` panel still reports legacy `Render` and `Source` FPS
metrics that were designed for the old layer-based approach. In the pure compositor path those
numbers no longer describe what players care about, so the UI is misleading exactly when users use
Goggles as intended.

This change updates the performance panel now so it reports gamer-facing metrics aligned with the
 current compositor architecture and removes the obsolete metric plumbing instead of carrying both
 systems forward.

## Problem

- The current `Render` FPS reports Goggles viewer cadence, not game cadence.
- The current `Source` FPS reports viewer-sampled source updates, not active game presents.
- Legacy frame-history plots and timing hooks keep layer-era measurement code alive after the
  underlying product model changed.

## Scope

- Replace the legacy `Render` / `Source` FPS panel entries with `Game FPS` and
  `Compositor Latency`.
- Define `Game FPS` as presents or commits from the currently captured game surface only.
- Define `Compositor Latency` as commit-to-capture delay.
- Remove the old plots, timing buffers, source-frame notify path, and any dead code that exists
  only for the retired metrics.

## What Changes

- Replace the visible Application performance metrics with gamer-facing labels and semantics.
- Source `Game FPS` from the active captured surface commit/present cadence instead of the viewer
  loop.
- Source `Compositor Latency` from compositor commit-to-capture timing instead of layer-era frame
  delta sampling.
- Delete obsolete `Render` / `Source` FPS plots and their supporting timing state and update hooks.
- Keep the new metric path direct and final: no deprecation banner, no hidden compatibility mode,
  and no legacy fallback path.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `app-window`: the Application performance panel requirements change from legacy viewer/source FPS
  reporting to `Game FPS` and `Compositor Latency` with gamer-facing semantics.
- `compositor-capture`: the compositor capture path requirements change to expose the timing data
  needed for active-surface present cadence and commit-to-capture latency reporting.

## Non-goals

- Keep legacy `Render` / `Source` FPS metrics available anywhere in the UI or code path.
- Rebrand Goggles viewer FPS as `Game FPS`.
- Expand `Compositor Latency` into end-to-end display, presentation, or input latency.
- Add migration banners, deprecation notes, or hidden fallback behavior for the old metrics.

## Impact

- Affected modules: `src/ui`, `src/app`, and `src/compositor`.
- Likely affected files: `src/ui/imgui_layer.cpp`, `src/ui/imgui_layer.hpp`,
  `src/app/application.cpp`, `src/compositor/compositor_present.cpp`,
  `src/compositor/compositor_xdg.cpp`, and `src/compositor/compositor_xwayland.cpp`.
- Impacted OpenSpec specs: `openspec/specs/app-window/spec.md` and
  `openspec/specs/compositor-capture/spec.md`.
- No dependency, packaging, or external API changes are expected.

## Risks

- The compositor metric path could accidentally count non-game updates if active-surface ownership
  is not defined precisely.
- The cleanup could leave hidden dead code behind if the old timing path has indirect consumers.
- The UI can become more honest but less stable if timing windows are not defined consistently.

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
  - allowed only for validating the visible performance panel behavior when automated UI coverage is
    unavailable
  - record prerequisites, observations, and proof location
- Mandatory checks with no fallback:
  - build and static-analysis gates above
- Pass criteria:
  - the Application performance UI exposes only `Game FPS` and `Compositor Latency`
  - the legacy labels and their dead code path are absent
