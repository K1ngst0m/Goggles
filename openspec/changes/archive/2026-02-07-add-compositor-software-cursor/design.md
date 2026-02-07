## Context

- The compositor currently accepts SDL pointer motion and updates a cursor in surface coordinates.
  Viewer scaling and window resizing make absolute pointer mapping unreliable, causing the host
  cursor and surface cursor to diverge.
- Relative pointer and pointer constraints are already supported for game-style input. We need to
  make the compositor cursor fully independent of host cursor positioning.

## Goals / Non-Goals

- Goals:
  - Provide a software cursor rendered into compositor-presented frames.
  - Drive cursor movement using raw relative deltas only (no absolute mapping).
  - Preserve raw relative pointer deltas for `zwp_relative_pointer_v1` clients.
  - Respect pointer lock/confine constraints and cursor hints.
  - Use the Xcursor assets shipped in `assets/cursor`.
- Non-Goals:
  - Implement themed XCursor assets or per-client cursor images.
  - Change capture-layer behavior or Vulkan-layer cursor rendering.

## Decisions

- Decision: Track a compositor cursor in surface-local coordinates.
  - Maintain `cursor_x/y` and `cursor_visible` in `CompositorServer::Impl`.
  - Initialize cursor on focus change (center of surface) and clamp to surface bounds.

- Decision: Ignore absolute coordinates and use raw relative deltas for cursor updates.
  - SDL motion deltas update the compositor cursor directly.
  - Raw deltas are forwarded to `zwp_relative_pointer_v1` without scaling.

- Decision: Handle pointer constraints in cursor state updates.
  - For locked constraints, keep cursor stationary (or update to cursor hint if provided) and hide
    the software cursor.
  - For confined constraints, clamp cursor position using `wlr_region_confine` on the constraint
    region in surface coordinates.

- Decision: Render the cursor using the Breeze Light Xcursor assets.
  - Load the cursor theme via `wlr_xcursor_theme_load`.
  - Build a `wlr_texture` from the cursor image and draw it after the surface texture.
  - Apply hotspot offsets when positioning the cursor.

- Decision: Mirror cursor visibility to the viewer window.
  - Hide the host cursor and enable relative mouse mode when UI is hidden.
  - Show the host cursor and stop forwarding pointer events when UI is visible.

## Risks / Trade-offs

- Software cursor may overlap with apps that draw their own cursor without pointer lock. Mitigation:
  hide the cursor during pointer lock and consider a future toggle if needed.

## Migration Plan

- No data migration. Changes are runtime-only.

## Open Questions

- Should cursor size/color be configurable in `config/goggles.toml`?
- Should the software cursor be suppressed when Vulkan-layer frames are active?
