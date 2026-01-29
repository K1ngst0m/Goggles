# Change: Auto-select newest surface in automatic mode

## Why
Opening a new surface currently leaves focus on the previous surface unless it is the first client.
This feels wrong for multi-window workflows; users expect the newest window to become active when
automatic surface selection is enabled.

## What Changes
- Update auto-selection to focus the most recently mapped surface when no manual override is set.
- Apply the same auto-focus rule to Wayland and XWayland surfaces.
- Preserve manual override behavior so new surfaces do not steal focus while manual selection is
  active.

## Impact
- Affected specs: input-forwarding
- Affected code: src/compositor/compositor_server.cpp
