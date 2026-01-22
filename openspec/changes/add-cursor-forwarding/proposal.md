# Change: Add Cursor Forwarding

## Why

Games require relative pointer motion (mouselook), pointer constraints (cursor lock/confine), and extended button support to function correctly. The current implementation only forwards absolute coordinates and a subset of mouse buttons, making FPS games and other mouse-captured applications unusable.

## What Changes

- Add `zwp_relative_pointer_v1` protocol support for raw mouse deltas
- Add `zwp_pointer_constraints_v1` protocol support for lock/confine
- Extend mouse button mapping to support all Linux input buttons
- Forward SDL's relative motion (`xrel`, `yrel`) alongside absolute position

## Impact

- Affected specs: `input-forwarding`
- Affected code:
  - `src/input/compositor_server.hpp` - new wlroots protocol managers, extended InputEvent enum
  - `src/input/compositor_server.cpp` - relative pointer, pointer constraints, constraint handling
  - `src/input/input_forwarder.hpp` - new `forward_mouse_motion_relative()` method
  - `src/input/input_forwarder.cpp` - use SDL xrel/yrel, extend button mapping

## Design Rationale

Following gamescope's pattern: motion events send BOTH relative (via `wlr_relative_pointer_manager_v1`) AND absolute (via `wlr_seat_pointer_notify_motion`). This matches Wayland protocol semantics where clients may listen to either or both.

Pointer constraints are handled by:
1. Creating `wlr_pointer_constraints_v1` on the display
2. Listening for `new_constraint` signals
3. Activating constraints on the focused surface
4. Applying lock/confine logic during motion processing
