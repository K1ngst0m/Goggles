# Change: Add Wayland Native Input Forwarding

## Why

The current input forwarding implementation only supports X11 applications via XTest injection to XWayland. Wayland-native applications connecting to the nested compositor cannot receive input events. Additionally, the XTest approach bypasses wlroots' seat/focus management system and requires maintaining a separate X11 connection.

## What Changes

- **BREAKING**: Replace XTest injection with unified `wlr_seat_*` APIs for both X11 and Wayland apps (removes `display_number()`, adds `x11_display()`/`wayland_display()`)
- Connect XWayland to seat via `wlr_xwayland_set_seat()` so wlr_xwm handles X11 translation
- Track surfaces from both xdg_shell (Wayland) and XWayland clients
- Add virtual keyboard device with xkb keymap for proper key event delivery
- Add pointer capability to seat for mouse event delivery
- Implement thread-safe input event marshaling from main thread to compositor thread
- Rename `XWaylandServer` to `CompositorServer` to reflect its broader role
- **BREAKING**: Remove X11/XTest dependencies from input forwarding (removes libX11/libXtst link targets from build)

## Old Code Cleanup

Since the project is pre-release, we will completely remove the XTest-based implementation rather than maintaining dual paths:

**Files to rename:**
- `src/input/xwayland_server.hpp` -> `compositor_server.hpp`
- `src/input/xwayland_server.cpp` -> `compositor_server.cpp`

**Code to remove from `input_forwarder.cpp`:**
- `#include <X11/Xlib.h>` and `#include <X11/extensions/XTest.h>`
- `Display* x11_display` member and `XOpenDisplay()`/`XCloseDisplay()` calls
- `linux_to_x11_keycode()` function (wlr_xwm handles X11 keycode translation)
- `XTestFakeKeyEvent()`, `XTestFakeButtonEvent()`, `XTestFakeMotionEvent()` calls
- `XFlush()` calls

**Dependencies to remove from `src/input/CMakeLists.txt`:**
- `X11::X11` link target
- `X11::Xtst` link target
- Any `find_package(X11)` if local to this module

**Archive for reference:**
- The old XTest design is preserved in `openspec/changes/archive/2026-01-04-add-input-forwarding-x11/`

**Test app updates (`tests/input/goggles_input_test.cpp`):**
- Currently hardcodes `setenv("SDL_VIDEODRIVER", "x11", 1)`
- Create two separate test binaries for separation of concerns:
  - `goggles_manual_input_x11` - manual X11/XWayland input probe via wlr_seat -> wlr_xwm path
  - `goggles_manual_input_wayland` - manual native Wayland input probe via wlr_seat path
- Each binary sets appropriate `SDL_VIDEODRIVER` (`x11` or `wayland`)
- Also log `WAYLAND_DISPLAY` in addition to `DISPLAY`

## Impact

- New spec: `input-forwarding` capability
- Affected code:
  - `src/input/xwayland_server.*` -> `compositor_server.*` (rename + refactor)
  - `src/input/input_forwarder.hpp` (minimal changes)
  - `src/input/input_forwarder.cpp` (major refactor - remove X11, add wlr_seat)
  - `src/input/CMakeLists.txt` (remove X11/XTest deps)
  - `tests/input/goggles_input_test.cpp` -> split into `_x11` and `_wayland` variants
  - `tests/input/CMakeLists.txt` (build both test binaries)
  - `docs/input_forwarding.md` (update architecture)
- Removed dependencies: `libX11`, `libXtst` (from input forwarding module)
