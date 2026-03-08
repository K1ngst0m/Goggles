## Context

Goggles moved from the old layer-oriented performance interpretation to a pure compositor capture path,
but the Application performance panel still derives its numbers from ImGui-layer frame deltas and a
viewer-sampled source update hook. That architecture mismatch is why the UI can report `60 FPS`
while the captured game is presenting much faster.

This change crosses `src/ui`, `src/app`, and `src/compositor`, so the design MUST establish one
authoritative metric source that matches the compositor capture model and removes the obsolete
layer-era timing path entirely.

## Goals / Non-Goals

**Goals:**
- Make `Game FPS` reflect active captured game-surface presents or commits only.
- Make `Compositor Latency` reflect commit-to-capture delay.
- Remove the old `Render` / `Source` FPS bookkeeping, plots, and hooks once the new metrics are in
  place.
- Keep the new metric path simple enough that `/goggles-apply` can implement it without transitional
  compatibility code.

**Non-Goals:**
- Measuring Goggles viewer FPS.
- Measuring end-to-end display latency or input latency.
- Introducing new external dependencies, packaging changes, or persistent telemetry systems.

## Decisions

### Decision: Move metric source-of-truth to compositor-side capture telemetry

The active metric source-of-truth SHALL move out of `ImGuiLayer` timing buffers and into the
compositor capture path.

Rationale:
- The compositor already observes the active surface commit boundary and the capture publication
  boundary.
- The UI cannot observe game cadence honestly from its own render loop.

Alternatives considered:
- Keep computing metrics in `ImGuiLayer`: rejected because it preserves the misleading viewer-loop
  sampling model.
- Compute metrics in `Application` from `get_presented_frame()` polling: rejected because it only
  sees retained frames after sampling loss.

### Decision: Publish a bounded runtime metrics snapshot from compositor to app/UI

The compositor path SHALL maintain the rolling state needed for `Game FPS` and `Compositor
Latency`, and the application/UI path SHALL consume a compact snapshot rather than recomputing from
legacy frame histories.

That snapshot contract SHALL be owned by a compositor-to-application runtime boundary outside
`src/ui`. The UI SHALL only render already-computed values and SHALL NOT depend on compositor event
types, commit semantics, or compositor-internal storage details.

Rationale:
- A single bounded snapshot prevents duplicate timing logic across compositor, app, and UI.
- It keeps ownership aligned with where the authoritative events occur.

Alternatives considered:
- Expose raw event streams to the UI: rejected because it recreates timing logic in the wrong layer.
- Push values directly into `ImGuiLayer` through ad-hoc callbacks: rejected because it couples UI to
  compositor internals without a stable runtime contract.
- Reuse a UI-shaped data model as the shared contract: rejected because it makes compositor/app
  boundaries depend on presentation concerns.

### Decision: Count only active captured game-surface commits toward `Game FPS`

`Game FPS` SHALL ignore viewer redraw cadence, secondary surfaces, and non-game refresh paths such
as unrelated compositor updates.

Rationale:
- The user explicitly wants a gamer-facing metric.
- Counting secondary or viewer-side activity would reintroduce the same semantic drift the change is
  trying to remove.

Alternatives considered:
- Count every compositor-captured frame: rejected because overlay, cursor, or other compositor
  refreshes can inflate the metric.

### Decision: Remove legacy metric plumbing completely after replacement

The implementation SHALL delete the old `Render` / `Source` FPS plots, timing buffers,
`notify_source_frame` path, and any other now-unused code instead of hiding them.

Rationale:
- The user requested a direct replacement with no deprecation note or compatibility fallback.
- Keeping the old path would preserve misleading terminology and dead maintenance burden.

Alternatives considered:
- Keep a hidden fallback: rejected because it violates the no-legacy-remnants constraint.

## Risks / Trade-offs

- [Active-surface ambiguity] -> Require the metric source to key off the currently captured game
  surface only and define that in the delta specs.
- [Indirect dead-code survivors] -> Require tasks to remove old labels, plots, timing buffers, and
  update hooks together, then verify no legacy references remain.
- [Metric instability from very short sampling] -> Use a bounded rolling aggregation contract owned
  by the compositor metric snapshot rather than UI-frame deltas.

## Migration Plan

1. Add compositor-side metric collection for active-surface cadence and commit-to-capture latency.
2. Publish the runtime metrics snapshot to the application/UI path.
3. Replace the Application performance panel labels and displayed values.
4. Remove legacy `Render` / `Source` metric state, plots, and update hooks.
5. Verify the new panel behavior and confirm legacy metric paths are gone.

Rollback strategy:
- Revert the change as one unit if the new metric contract proves incorrect; do not preserve a dual
  path in-tree.

## Open Questions

- None.
