## 1. File Renames

- [x] 1.1 Rename `xwayland_server.hpp` to `compositor_server.hpp`
- [x] 1.2 Rename `xwayland_server.cpp` to `compositor_server.cpp`
- [x] 1.3 Rename class `XWaylandServer` to `CompositorServer`
- [x] 1.4 Update `#include` paths in `input_forwarder.cpp`
- [x] 1.5 Update CMakeLists.txt source file references

## 2. Seat and Input Device Setup

- [x] 2.1 Add `WL_SEAT_CAPABILITY_POINTER` to seat capabilities (alongside keyboard)
- [x] 2.2 Create virtual `wlr_keyboard` via `wlr_keyboard_init()` from keyboard interface
- [x] 2.3 Set xkb keymap on virtual keyboard (`xkb_keymap_new_from_names`)
- [x] 2.4 Attach keyboard to seat via `wlr_seat_set_keyboard()`
- [x] 2.5 Call `wlr_xwayland_set_seat()` to connect XWayland to the seat

## 3. Surface Tracking

- [x] 3.1 Add xdg_toplevel tracking via `wlr_xdg_shell.events.new_toplevel` signal
- [x] 3.2 Add XWayland surface tracking via `wlr_xwayland.events.new_surface` signal
- [x] 3.3 Implement unified surface list (`std::vector<wlr_surface*>`)
- [x] 3.4 Add surface destroy listeners for cleanup (Wayland only - see note)
- [x] 3.5 Auto-focus first connected surface (single-app model)

**Note on 3.4**: Destroy listeners are only wired for native Wayland surfaces (xdg_toplevel). XWayland surface destroy listeners are NOT used because they fire unexpectedly during normal X11 operation, breaking input forwarding. Instead, XWayland focus cleanup uses mutual exclusion: when a new surface gains focus, any stale XWayland pointers are cleared.

## 4. Thread-Safe Event Marshaling

- [x] 4.1 Define `InputEvent` struct (type enum, keycode/button, coords, pressed)
- [x] 4.2 Create `SPSCQueue<InputEvent>` for lock-free event passing
- [x] 4.3 Create eventfd for wl_event_loop wakeup notification
- [x] 4.4 Register eventfd with `wl_event_loop_add_fd()`
- [x] 4.5 Implement compositor-thread dispatch: drain queue, call wlr_seat_* APIs

## 5. Unified Input via wlr_seat

- [x] 5.1 Implement `inject_key()` using `wlr_seat_keyboard_notify_key()`
- [x] 5.2 Implement `inject_pointer_motion()` using `wlr_seat_pointer_notify_motion()`
- [x] 5.3 Implement `inject_pointer_button()` using `wlr_seat_pointer_notify_button()`
- [x] 5.4 Implement `inject_pointer_axis()` using `wlr_seat_pointer_notify_axis()`
- [x] 5.5 Add `wlr_seat_pointer_notify_frame()` after each event batch
- [x] 5.6 Handle keyboard/pointer enter on surface focus change

## 6. Old Code Cleanup (X11/XTest Removal)

- [x] 6.1 Remove `#include <X11/Xlib.h>` from input_forwarder.cpp
- [x] 6.2 Remove `#include <X11/extensions/XTest.h>` from input_forwarder.cpp
- [x] 6.3 Remove `Display* x11_display` member from InputForwarder::Impl
- [x] 6.4 Remove `XOpenDisplay()` call in `InputForwarder::create()`
- [x] 6.5 Remove `XCloseDisplay()` call in `InputForwarder::Impl` destructor
- [x] 6.6 Remove `linux_to_x11_keycode()` function (wlr_xwm handles this)
- [x] 6.7 Remove `XTestFakeKeyEvent()` calls in `forward_key()`
- [x] 6.8 Remove `XTestFakeButtonEvent()` calls in `forward_mouse_button()` and `forward_mouse_wheel()`
- [x] 6.9 Remove `XTestFakeMotionEvent()` calls in `forward_mouse_motion()`
- [x] 6.10 Remove all `XFlush()` calls
- [x] 6.11 Remove X11::X11 from CMakeLists.txt target_link_libraries
- [x] 6.12 Remove X11::Xtst from CMakeLists.txt target_link_libraries
- [x] 6.13 Remove find_package(X11) if local to input module

## 7. InputForwarder Interface Updates

- [x] 7.1 Simplify `forward_key()` to call `server.inject_key()`
- [x] 7.2 Simplify `forward_mouse_button()` to call `server.inject_pointer_button()`
- [x] 7.3 Simplify `forward_mouse_motion()` to call `server.inject_pointer_motion()`
- [x] 7.4 Simplify `forward_mouse_wheel()` to call `server.inject_pointer_axis()`
- [x] 7.5 Add `x11_display()` and `wayland_display()` methods to expose display names

## 8. Documentation

- [x] 8.1 Update `docs/input_forwarding.md` with unified wlr_seat architecture
- [x] 8.2 Remove XTest references from documentation
- [x] 8.3 Document `WAYLAND_DISPLAY` and `DISPLAY` environment variables
- [x] 8.4 Add architecture diagram showing unified input flow
- [x] 8.5 Document known limitations (coordinate mapping, single-app focus)

## 9. Test App Updates

- [x] 9.1 Create `goggles_manual_input_x11` (set `SDL_VIDEODRIVER=x11`)
- [x] 9.2 Create `goggles_manual_input_wayland` (set `SDL_VIDEODRIVER=wayland`)
- [x] 9.3 Update both to log `WAYLAND_DISPLAY` in addition to `DISPLAY`
- [x] 9.4 Remove hardcoded `setenv("SDL_VIDEODRIVER", "x11", 1)` from original
- [x] 9.5 Update `tests/CMakeLists.txt` to build both test binaries
- [x] 9.6 Delete original `goggles_input_test.cpp` after split

## 10. Build Verification

- [x] 10.1 Verify build succeeds without X11/XTest dependencies
- [x] 10.2 Verify no X11/XTest symbols in final binary (`nm -u` check)

## 11. Manual Testing

- [x] 11.1 Test `goggles_manual_input_x11` receives keyboard events via wlr_seat -> wlr_xwm
- [x] 11.2 Test `goggles_manual_input_x11` receives pointer events via wlr_seat -> wlr_xwm
- [x] 11.3 Test `goggles_manual_input_wayland` receives keyboard events via wlr_seat
- [x] 11.4 Test `goggles_manual_input_wayland` receives pointer events via wlr_seat
- [x] 11.5 Test X11→Wayland focus transition (close X11 app, open Wayland app)
- [x] 11.6 Test Wayland→X11 focus transition (close Wayland app, open X11 app)
