# Change: Add compositor popup support

## Why
Menu dropdowns and other transient popups are rendered as separate surfaces that the compositor
currently ignores, so they never appear in the presented frame.

## What Changes
- Track and configure Wayland `xdg_popup` surfaces.
- Present popup surfaces composited above their parent surface.
- Resolve pointer targets via wlroots hit-testing with popup-local coordinates.
- Expand cursor bounds to include mapped popups to avoid clamped pointer drift.
- Treat XWayland override-redirect windows as popups for rendering and input routing.

## Impact
- Affected specs: input-forwarding
- Affected code: src/compositor/compositor_server.cpp, compositor presentation path
