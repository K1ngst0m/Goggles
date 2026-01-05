## 1. File Renames

- [ ] 1.1 Rename `xwayland_server.hpp` to `compositor_server.hpp`
- [ ] 1.2 Rename `xwayland_server.cpp` to `compositor_server.cpp`
- [ ] 1.3 Rename class `XWaylandServer` to `CompositorServer`
- [ ] 1.4 Update `#include` paths in `input_forwarder.cpp`
- [ ] 1.5 Update CMakeLists.txt source file references

## 2. Seat and Input Device Setup

- [ ] 2.1 Add `WL_SEAT_CAPABILITY_POINTER` to seat capabilities (alongside keyboard)
- [ ] 2.2 Create virtual `wlr_keyboard` device via `wlr_headless_add_input_device()`
- [ ] 2.3 Set xkb keymap on virtual keyboard (`xkb_keymap_new_from_names`)
- [ ] 2.4 Attach keyboard to seat via `wlr_seat_set_keyboard()`
- [ ] 2.5 Call `wlr_xwayland_set_seat()` to connect XWayland to the seat

## 3. Surface Tracking

- [ ] 3.1 Add xdg_toplevel tracking via `wlr_xdg_shell.events.new_toplevel` signal
- [ ] 3.2 Add XWayland surface tracking via `wlr_xwayland.events.new_surface` signal
- [ ] 3.3 Implement unified surface list (`std::vector<wlr_surface*>`)
- [ ] 3.4 Add surface destroy listeners for cleanup
- [ ] 3.5 Auto-focus first connected surface (single-app model)

## 4. Thread-Safe Event Marshaling

- [ ] 4.1 Create eventfd for cross-thread event delivery
- [ ] 4.2 Register event source with `wl_event_loop_add_fd()`
- [ ] 4.3 Define input event struct (type, keycode/button, coords, pressed)
- [ ] 4.4 Implement thread-safe event queue (SPSCQueue or simple pipe)
- [ ] 4.5 Implement compositor-thread event dispatch handler

## 5. Unified Input via wlr_seat

- [ ] 5.1 Implement `inject_key()` using `wlr_seat_keyboard_notify_key()`
- [ ] 5.2 Implement modifier tracking with `wlr_seat_keyboard_notify_modifiers()`
- [ ] 5.3 Implement `inject_pointer_motion()` using `wlr_seat_pointer_notify_motion()`
- [ ] 5.4 Implement `inject_pointer_button()` using `wlr_seat_pointer_notify_button()`
- [ ] 5.5 Implement `inject_pointer_axis()` using `wlr_seat_pointer_notify_axis()`
- [ ] 5.6 Add `wlr_seat_pointer_notify_frame()` after each event batch
- [ ] 5.7 Handle keyboard/pointer enter on surface focus change

## 6. Old Code Cleanup (X11/XTest Removal)

- [ ] 6.1 Remove `#include <X11/Xlib.h>` from input_forwarder.cpp
- [ ] 6.2 Remove `#include <X11/extensions/XTest.h>` from input_forwarder.cpp
- [ ] 6.3 Remove `Display* x11_display` member from InputForwarder::Impl
- [ ] 6.4 Remove `XOpenDisplay()` call in `InputForwarder::create()`
- [ ] 6.5 Remove `XCloseDisplay()` call in `InputForwarder::Impl` destructor
- [ ] 6.6 Remove `linux_to_x11_keycode()` function (wlr_xwm handles this)
- [ ] 6.7 Remove `XTestFakeKeyEvent()` calls in `forward_key()`
- [ ] 6.8 Remove `XTestFakeButtonEvent()` calls in `forward_mouse_button()` and `forward_mouse_wheel()`
- [ ] 6.9 Remove `XTestFakeMotionEvent()` calls in `forward_mouse_motion()`
- [ ] 6.10 Remove all `XFlush()` calls
- [ ] 6.11 Remove X11::X11 from CMakeLists.txt target_link_libraries
- [ ] 6.12 Remove X11::Xtst from CMakeLists.txt target_link_libraries
- [ ] 6.13 Remove find_package(X11) if local to input module

## 7. InputForwarder Interface Updates

- [ ] 7.1 Simplify `forward_key()` to call `server.inject_key()`
- [ ] 7.2 Simplify `forward_mouse_button()` to call `server.inject_pointer_button()`
- [ ] 7.3 Simplify `forward_mouse_motion()` to call `server.inject_pointer_motion()`
- [ ] 7.4 Simplify `forward_mouse_wheel()` to call `server.inject_pointer_axis()`
- [ ] 7.5 Add `wayland_socket_name()` method to expose WAYLAND_DISPLAY value

## 8. Documentation

- [ ] 8.1 Update `docs/input_forwarding.md` with unified wlr_seat architecture
- [ ] 8.2 Remove XTest references from documentation
- [ ] 8.3 Document `WAYLAND_DISPLAY` and `DISPLAY` environment variables
- [ ] 8.4 Add architecture diagram showing unified input flow
- [ ] 8.5 Document known limitations (coordinate mapping, single-app focus)

## 9. Test App Updates

- [ ] 9.1 Create `goggles_input_test_x11.cpp` from existing test (set `SDL_VIDEODRIVER=x11`)
- [ ] 9.2 Create `goggles_input_test_wayland.cpp` (set `SDL_VIDEODRIVER=wayland`)
- [ ] 9.3 Update both to log `WAYLAND_DISPLAY` in addition to `DISPLAY`
- [ ] 9.4 Remove hardcoded `setenv("SDL_VIDEODRIVER", "x11", 1)` from original
- [ ] 9.5 Update `tests/input/CMakeLists.txt` to build both test binaries
- [ ] 9.6 Delete original `goggles_input_test.cpp` after split

## 10. Build Verification

- [ ] 10.1 Verify build succeeds without X11/XTest dependencies
- [ ] 10.2 Verify no X11/XTest symbols in final binary (`nm -u` check)

## 11. Manual Testing

- [ ] 11.1 Test `goggles_input_test_x11` receives keyboard events via wlr_seat -> wlr_xwm
- [ ] 11.2 Test `goggles_input_test_x11` receives pointer events via wlr_seat -> wlr_xwm
- [ ] 11.3 Test `goggles_input_test_wayland` receives keyboard events via wlr_seat
- [ ] 11.4 Test `goggles_input_test_wayland` receives pointer events via wlr_seat
