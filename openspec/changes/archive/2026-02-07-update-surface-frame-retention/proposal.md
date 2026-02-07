# Change: Preserve last presented surface frame on target switch

## Why
Switching compositor targets clears the presented frame and shows black until a new commit arrives.
UI toolkits like Qt often redraw only on demand, so the viewer goes black even though the surface
already has a valid last frame.

## What Changes
- Retain the last presented frame per surface and reuse it on target changes.
- Refresh presentation on manual and auto target switches without waiting for a new commit.
- Clear presentation only when the new target has no retained frame.

## Impact
- Affected specs: surface-frame-presentation (new)
- Affected code: src/compositor/compositor_server.cpp
