# Design: Wayland Native Input Forwarding

## Context

Goggles provides input forwarding to captured applications by running a nested compositor (headless wlroots) with XWayland. The current implementation injects input via XTest to the XWayland server, which works but:

1. Bypasses wlroots' seat/focus management system
2. Requires maintaining a separate X11 client connection
3. Cannot support Wayland-native apps
4. Adds X11/XTest as dependencies

Investigation revealed that `wlr_xwayland_set_seat()` connects XWayland to the compositor's seat, allowing the wlr_xwm (X Window Manager) to automatically translate `wlr_seat_*` input events to X11 events. This enables a **unified input path** for both Wayland and X11 apps.

### Constraints

- **Thread safety**: wlr_seat calls must execute on the compositor thread (running `wl_display_run`)
- **Single-app focus**: MVP targets single captured application (no multi-window focus management)
- **Coordinate mapping**: Not implemented; document as limitation
- **Backward compatibility**: X11 apps must continue working (via wlr_xwm translation)

## Goals / Non-Goals

**Goals:**
- Unified input path using `wlr_seat_*` for both X11 and Wayland apps
- Remove X11/XTest dependencies from input forwarding
- Support single focused application (auto-focus first connected client)
- Simpler codebase with single code path

**Non-Goals:**
- Multi-application focus management
- Coordinate scaling/mapping between viewer and target
- Touch input support
- Clipboard/data device integration

## Architecture

### Previous Data Flow (XTest - Being Removed)

```
SDL Event (main thread)
    |
    v
InputForwarder::forward_key()
    |
    v
sdl_to_linux_keycode() -> linux_to_x11_keycode()
    |
    v
XTestFakeKeyEvent(x11_display, keycode, pressed)
    |
    v
XWayland receives XTest request
    |
    v
X11 app receives KeyPress/KeyRelease
```

**Problems**: Dual dependencies, bypasses seat, doesn't support Wayland apps.

### New Data Flow (Unified wlr_seat)

```
SDL Event (main thread)
    |
    v
InputForwarder::forward_key()
    |
    v
sdl_to_linux_keycode()
    |
    v
Write to eventfd (queue event)
    |
    [Thread boundary - main -> compositor]
    |
    v
Compositor thread reads event
    |
    v
wlr_seat_keyboard_notify_key(seat, time, linux_keycode, state)
    |
    +--[Wayland surface focused]
    |       |
    |       v
    |   wl_keyboard.key sent to Wayland client
    |
    +--[XWayland surface focused]
            |
            v
        wlr_xwm translates to X11 KeyPress
            |
            v
        Xwayland server delivers to X11 app
```

**Benefits**: Single code path, no X11 deps, proper seat/focus management.

### Component Changes

#### CompositorServer (renamed from XWaylandServer)

```cpp
class CompositorServer {
public:
    [[nodiscard]] auto start() -> Result<int>;
    void stop();
    [[nodiscard]] auto display_number() const -> int;
    [[nodiscard]] auto wayland_socket_name() const -> std::string;

    // Input event injection (thread-safe, marshals to compositor thread)
    void inject_key(uint32_t linux_keycode, bool pressed);
    void inject_pointer_motion(double x, double y);
    void inject_pointer_button(uint32_t button, bool pressed);
    void inject_pointer_axis(double value, bool horizontal);

private:
    // Core wlroots objects
    wl_display* m_display;
    wl_event_loop* m_event_loop;
    wlr_backend* m_backend;
    wlr_renderer* m_renderer;
    wlr_allocator* m_allocator;
    wlr_compositor* m_compositor;
    wlr_xdg_shell* m_xdg_shell;
    wlr_seat* m_seat;
    wlr_xwayland* m_xwayland;

    // New: Virtual keyboard
    wlr_keyboard* m_virtual_keyboard;

    // New: Event marshaling
    int m_event_fd;

    // New: Surface tracking
    std::vector<wlr_surface*> m_surfaces;
    wlr_surface* m_focused_surface;

    // Signal handlers
    wl_listener m_new_xdg_toplevel;
    wl_listener m_new_xwayland_surface;

    std::jthread m_compositor_thread;
};
```

#### InputForwarder (Simplified)

```cpp
struct InputForwarder::Impl {
    CompositorServer server;
    // X11 Display* REMOVED - no longer needed
};

auto InputForwarder::forward_key(const SDL_KeyboardEvent& event) -> Result<void> {
    uint32_t linux_keycode = sdl_to_linux_keycode(event.scancode);
    if (linux_keycode == 0) return {};

    // Single path - wlr_seat handles both Wayland and X11
    m_impl->server.inject_key(linux_keycode, event.down);
    return {};
}
```

## Decisions

### Decision 1: Replace XTest with wlr_seat for XWayland

**Choice**: Use `wlr_xwayland_set_seat()` + `wlr_seat_*` APIs for X11 apps

**Rationale**:
- wlr_xwm automatically translates seat events to X11 events
- Eliminates X11/XTest dependencies
- Single code path for all apps
- Proper integration with wlroots seat/focus system

**Alternatives considered**:
- Keep XTest alongside wlr_seat (dual path) - rejected as unnecessary complexity
- Use XTest for X11 and wlr_seat for Wayland only - rejected, more code to maintain

### Decision 2: eventfd for cross-thread marshaling

**Choice**: Use eventfd + wl_event_loop_add_fd

**Rationale**:
- wlr_seat calls must happen on compositor thread
- eventfd is lightweight, single-fd notification
- wl_event_loop integration is idiomatic for wlroots

### Decision 3: Auto-focus first connected surface

**Choice**: Automatically focus the first surface (xdg_toplevel or XWayland) that connects

**Rationale**:
- MVP targets single-app use case
- Avoids complex focus management
- User explicitly launches target with appropriate env vars

## Thread Model

```
Main Thread                    Compositor Thread
-----------                    -----------------
SDL event loop                 wl_display_run() event loop
    |                               |
    v                               |
forward_key()                       |
    |                               |
    v                               |
server.inject_key()                 |
    |                               |
    v                               |
write(eventfd, event)               |
                                    v
                            wl_event_loop wakes
                                    |
                                    v
                            handle_input_event()
                                    |
                                    v
                            wlr_seat_keyboard_notify_key()
                                    |
                     +--------------+---------------+
                     |                              |
                     v                              v
              [Wayland surface]              [XWayland surface]
                     |                              |
                     v                              v
              wl_keyboard.key              wlr_xwm -> X11 KeyPress
                     |                              |
                     v                              v
              Wayland Client                   X11 App
```

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| wlr_xwm translation issues | Tested in many compositors; well-proven path |
| Event marshaling latency | eventfd is low-latency (~microseconds); acceptable |
| Modifier state desync | Track modifier state in CompositorServer |
| Coordinate mismatch | Document as limitation; future coordinate mapping task |

## Migration Plan

1. Add `wlr_xwayland_set_seat()` call to connect XWayland to seat
2. Add virtual keyboard and surface tracking
3. Add eventfd marshaling for thread-safe input injection
4. Implement `inject_*` methods using `wlr_seat_*` APIs
5. Update InputForwarder to use new inject methods
6. Remove X11/XTest code and dependencies
7. Update documentation

**Rollback**: Revert to XTest-based approach if wlr_xwm has issues (unlikely).

## Open Questions

1. ~~Should we keep XTest as fallback?~~ **No** - wlr_xwm is proven, adds complexity
2. How to handle keyboard layout/keymap? **Use xkb_keymap_new_from_names with defaults**
