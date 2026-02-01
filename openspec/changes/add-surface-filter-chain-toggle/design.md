## Context
The ImGui Surface List already exposes per-surface metadata for input routing. Users need a per-surface way to bypass shader processing while keeping the rest of the pipeline unchanged. The change crosses UI state, surface metadata, and render pipeline routing.

## Goals / Non-Goals
- Goals:
  - Provide a per-surface checkbox in the Surface List to enable or bypass the filter chain.
  - Provide a global Window Management checkbox to bypass the filter chain for all surfaces in the
    session.
  - Bypass mode uses a real maximize-style resize so the client re-renders at the new size (no stretch-blit).
  - Popups/subsurfaces inherit the parent xdg_toplevel setting.
- Non-Goals:
  - Persisting per-surface toggles across app restarts.
  - Adding new global shader toggles (reuse existing passthrough behavior if present).

## Decisions
- Decision: Store the toggle in app-side surface state keyed by surface id; default to enabled when a new surface appears.
- Decision: Default the per-surface toggle to enabled for Vulkan capture path surfaces and disabled for non-Vulkan capture path surfaces.
- Decision: Add a Window Management global toggle; effective filter-chain usage is
  `window_mgmt_filter_chain_enabled && surface_filter_chain_enabled` (global passthrough still
  overrides per-surface settings).
- Decision: The Window Management toggles control filter-chain enablement, while the effect stage
  is additionally gated by the Effect Stage "Enable Shader" control.
- Decision: Render pipeline accepts a per-surface render mode and switches to a compositor-style resize path when bypassed, so the client renders at the window size instead of stretching the captured image.
- Decision: Surface metadata includes a stable reference to the parent xdg_toplevel id so popups inherit the parent setting.

## Risks / Trade-offs
- UI clarity: the Surface List grows more complex. Mitigation: keep checkbox label minimal and add a tooltip.
- Render routing complexity: per-surface path selection could introduce branching in the render loop. Mitigation: resolve the mode once per surface/frame and keep the hot path simple.

## Migration Plan
- No migration; defaults preserve current behavior (filter chain enabled for all surfaces).

## Open Questions
- Should the Surface List show an explicit tooltip describing maximize-style resize behavior when unchecked?
