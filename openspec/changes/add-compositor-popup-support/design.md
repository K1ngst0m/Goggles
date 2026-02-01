## Context
The compositor currently tracks only xdg_toplevel surfaces and ignores xdg_popup and XWayland
override-redirect surfaces. Menus and dropdowns are created as separate transient surfaces, so
they never receive configure events, input focus, or presentation.

## Goals / Non-Goals
- Goals:
  - Support xdg_popup lifecycle (configure, map, destroy) and render popups above their parent.
  - Preserve unified input path while honoring popup grabs.
  - Present XWayland override-redirect menus/tooltips as transient popups.
- Non-Goals:
  - Full desktop-style window management (multi-app, tiling, workspace logic).
  - General-purpose damage tracking or scene graph beyond what popups require.

## Decisions
- Decision: Track popups in a dedicated list keyed by parent surface and maintain explicit
  stacking order (creation order). This keeps the existing single-app focus model while enabling
  popup layering.
- Decision: Composite parent + popup surfaces into the presented frame using wlroots surface
  traversal utilities rather than introducing a full scene graph.
- Decision: Use wlroots hit-testing (`wlr_xdg_surface_surface_at`) to resolve pointer targets and
  popup-local coordinates, falling back to the topmost popup during active grabs.
- Decision: Use `wlr_xdg_popup_get_position` to compute popup offsets so geometry-relative positions
  match input coordinates.
- Decision: Clamp cursor movement to the union of root + popup bounds (Wayland and XWayland) instead
  of the root surface alone to prevent cursor drift near popups.
- Decision: Treat XWayland override-redirect windows as popup surfaces and render them above the
  currently focused XWayland surface when no explicit parent is available.

## Risks / Trade-offs
- Rendering multiple surfaces per frame adds CPU/GPU work; constrain to the focused surface tree
  to avoid unnecessary composition.
- Pointer hit-testing and bounds aggregation add per-event work; keep the logic linear and avoid
  extra allocations in hot paths.
- Popup grab handling must not regress the existing input forwarding model; limit changes to
  routing decisions and avoid extra threads or blocking calls.

## Migration Plan
- Implement new popup tracking alongside existing toplevel tracking.
- Integrate composition path for popups in presentation output.
- Update pointer routing to use hit-testing and popup-local offsets.
- Extend cursor bounds to include mapped popups.
- Validate with a native Wayland app (xdg_popup) and an XWayland app (override-redirect menu).

## Open Questions
- Should popup surfaces be exposed in the surface selector UI or hidden behind their parent?
