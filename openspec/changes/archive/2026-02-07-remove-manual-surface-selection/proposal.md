# Change: Remove manual surface selection mode

## Why
Manual surface selection adds a second input-target path that diverges from the default behavior and
is harder to maintain. We want a single, always-on auto-selection path while still allowing users
to click a surface to focus it immediately.

## What Changes
- Remove manual override state and APIs; selection becomes a focus request only.
- Keep automatic surface ordering active at all times (newest mapped surface can take focus).
- Update surface selection UI to remove manual/auto indicators and the reset action.
- Simplify input-target resolution to rely on focused surfaces only.

## User Behavior (High-Level)
- The surface list is always available for click-to-focus.
- Clicking a surface switches focus and presentation immediately.
- Auto-selection never turns off; the next mapped surface can take focus after a click.
- There is no manual/auto mode or reset action in the UI.

### Example
1) Two apps are running; app A is focused by auto-selection.
2) User clicks app B in the surface list → focus/presentation switches to B.
3) A new app C maps → focus/presentation switches to C automatically.

## Impact
- Affected specs: input-forwarding
- Affected code: src/compositor/compositor_server.cpp, src/compositor/compositor_server.hpp,
  src/ui/imgui_layer.cpp, src/ui/imgui_layer.hpp, src/app/application.cpp
- Notes: Overlaps with pending changes add-surface-selector and update-auto-surface-selection;
  this change supersedes manual override behavior.
