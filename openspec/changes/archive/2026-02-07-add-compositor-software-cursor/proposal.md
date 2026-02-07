# Change: Add Compositor Software Cursor

## Why

Viewer/window scaling makes absolute cursor mapping unreliable. The host pointer position can
never precisely match surface-local coordinates, and clamping the host pointer prevents the
internal cursor from reaching parts of the surface. We need a compositor-local cursor rendered into
presented frames and driven by relative motion only, with the host cursor hidden in internal mode
and input forwarding suspended while the UI overlay is active.

## What Changes

- Track a compositor-local cursor position in surface coordinates and render it into presented
  frames from `CompositorServer`.
- Use raw relative motion deltas to update the compositor cursor; ignore absolute window
  coordinates entirely.
- Respect pointer constraints by freezing or clamping the cursor and honoring cursor hints when
  provided by clients, mirroring pointer lock to the host window.
- Render the cursor using the XCursor assets in `assets/cursor` (Breeze Light, 64px), honoring
  hotspot data.
- Hide the host window cursor and disable pointer forwarding when the global UI is visible; show
  the host cursor in UI mode and resume forwarding when the UI is hidden.
- Remove dead compositor code paths (unused absolute coordinates and related state).

## Impact

- Affected specs: `input-forwarding`
- Affected code:
  - `src/compositor/compositor_server.hpp`
  - `src/compositor/compositor_server.cpp`
  - `src/app/application.cpp`
  - `src/app/application.hpp`
