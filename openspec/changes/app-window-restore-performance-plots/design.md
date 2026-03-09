## Context

The Application performance panel now reports compositor-sourced `Game FPS` and
`Compositor Latency`, but the recent replacement removed the historical plot lines that previously
helped operators spot jitter and transient spikes. The compositor still owns bounded timing history,
so the missing behavior is no longer raw measurement but the runtime contract needed to present that
history safely in the UI.

This change crosses `src/compositor`, `src/util`, `src/app`, and `src/ui`, so the design MUST keep
the compositor as the timing source of truth while restoring the plots without reviving any
legacy ImGui-side metric path.

## Goals / Non-Goals

**Goals:**
- Restore one live `Game FPS` plot beneath the existing `Game FPS` text readout.
- Restore one live `Compositor Latency` plot beneath the existing latency text readout.
- Keep the compositor capture path as the only metric source of truth.
- Extend the compositor-to-application runtime metrics contract so the UI consumes plot-ready,
  bounded history instead of recomputing timing state.
- Reset history cleanly when the active capture target changes so plots do not mix unrelated
  surfaces.

**Non-Goals:**
- Reintroducing legacy `Render` / `Source` metric code, labels, or plots.
- Adding new metrics, persistent telemetry, or external dependencies.
- Turning the shared runtime metrics boundary into an ImGui-specific data model.
- Changing unrelated Application window layout or controls.

## Decisions

### Decision: Keep compositor-owned history as the authoritative metrics source

The compositor capture path SHALL continue to own runtime metric collection, aggregation, reset, and
history retention for both `Game FPS` and `Compositor Latency`.

Rationale:
- The compositor alone sees the active target's commit and capture boundaries accurately.
- Recomputing history in `src/ui` or `src/app` would drift back toward the retired viewer-loop model.

Alternatives considered:
- Rebuild history in `ImGuiLayer`: rejected because it reintroduces presentation-layer timing logic.
- Reconstruct history in `Application` from sampled snapshots: rejected because it can miss commit
  cadence and creates a second timing interpretation.

### Decision: Publish plot-ready bounded series through the shared runtime metrics contract

The shared runtime metrics contract outside `src/ui` SHALL carry both current scalar values and the
bounded history needed to draw the two plots. The exported history SHALL be plot-ready for the UI:
`Game FPS` history SHALL be represented in FPS units and `Compositor Latency` history SHALL be
represented in milliseconds, with bounded counts that identify the valid sample window.

Rationale:
- The UI should render data, not reinterpret compositor timing internals.
- A boundary-owned snapshot keeps module ownership explicit and avoids leaking compositor-specific
  ring-buffer semantics into `src/ui`.

Alternatives considered:
- Expose raw interval buffers and let the UI convert them to FPS: rejected because it duplicates
  timing semantics outside the compositor boundary.
- Expose compositor internals directly to `ImGuiLayer`: rejected because it breaks module isolation.

### Decision: Preserve bounded storage and explicit target resets

The implementation SHALL reuse a fixed-size bounded history window and SHALL clear the published
history whenever the active capture target changes.

Rationale:
- Fixed-size history keeps per-frame overhead predictable and avoids heap churn in the UI path.
- Explicit reset semantics prevent mixed-surface plots that would mislead operators.

Alternatives considered:
- Grow history dynamically: rejected because it adds unnecessary allocation and ownership churn.
- Keep old samples across target changes: rejected because the resulting plots would mix unrelated
  targets.

### Decision: Restore plots as an additive UI behavior under the existing text rows

The Application performance panel SHALL keep the current `Game FPS` and `Compositor Latency` text
readouts and SHALL render one live plot directly beneath each text row.

Rationale:
- This matches the intended user-facing done-state while minimizing layout churn.
- Keeping the text rows preserves the current numeric at-a-glance behavior.

Alternatives considered:
- Replace the text rows with plots only: rejected because it removes the direct numeric readout.
- Collapse both histories into one combined chart: rejected because the units differ and the user
  asked for both plot lines back under the current metrics.

## Risks / Trade-offs

- [Runtime contract growth] -> Keep the history bounded and transport only the fields required for
  the two current metrics.
- [Semantic drift back to legacy behavior] -> Require the UI to consume compositor-published history
  directly and forbid reintroducing legacy timing buffers.
- [Mixed-target histories] -> Reset published series whenever the active capture target changes.
- [UI clutter] -> Limit the layout change to one plot per current text row and avoid unrelated panel
  redesign.

## Migration Plan

1. Extend the shared runtime metrics contract to include bounded plot-ready history for the two
   existing metrics.
2. Populate and reset those histories in the compositor capture path for the active target only.
3. Thread the expanded metrics snapshot through the application boundary.
4. Restore one live plot beneath each existing Performance panel text readout.
5. Verify the plots are compositor-sourced and that no legacy metric path returns.

Rollback strategy:
- Revert the change as one unit if the shared metrics contract or plot behavior proves incorrect; do
  not preserve a dual metrics path.

## Open Questions

- None.
