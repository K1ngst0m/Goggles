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
- **XWayland lifecycle**: XWayland surface destroy signals fire unexpectedly during normal X11 operation; must NOT be used for focus cleanup

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

### Surface Lifecycle

#### Native Wayland (xdg_toplevel)

```
new_toplevel signal
    |
    v
handle_new_xdg_toplevel()
    |
    v
Register commit listener on surface
    |
    v
[commit signal] -> handle_xdg_surface_commit()
    |
    v
Check toplevel->base->initialized
    |
    v
Register map listener
    |
    v
[map signal] -> handle_xdg_surface_map()
    |
    v
Add to m_surfaces, register destroy listener
    |
    v
Focus if no current focus OR current focus is stale XWayland
    |
    v
[destroy signal] -> handle_xdg_surface_destroy()
    |
    v
Remove from m_surfaces, clear focus if was focused
```

#### XWayland (X11 apps)

```
new_surface signal
    |
    v
handle_new_xwayland_surface()
    |
    v
Register associate listener (waits for xsurface->surface to be valid)
    |
    v
[associate signal] -> handle_xwayland_surface_associate()
    |
    v
Register map listener
    |
    v
[map signal] -> handle_xwayland_surface_map()
    |
    v
Focus if no surface currently focused
```

**Note**: XWayland surfaces are NOT tracked in m_surfaces and NO destroy listener is registered. XWayland destroy signals fire at unpredictable times (e.g., during window operations), breaking X11 input forwarding.

### Focus Management

Focus uses **mutual exclusion** between native Wayland and XWayland:

- `m_focused_surface`: Always points to the currently focused wlr_surface (or nullptr)
- `m_focused_xsurface`: Points to the XWayland surface if focus is XWayland (or nullptr)

**Key invariants**:
1. If native Wayland has focus: `m_focused_surface != nullptr`, `m_focused_xsurface == nullptr`
2. If XWayland has focus: `m_focused_surface == xsurface->surface`, `m_focused_xsurface != nullptr`
3. No focus: both are nullptr

**Stale pointer handling**: When switching from XWayland to native Wayland:
1. `m_focused_xsurface` may be dangling (X11 app exited, wlroots freed the memory)
2. `focus_surface()` clears `m_focused_xsurface = nullptr` BEFORE any wlroots API calls
3. This prevents crashes when wlroots internally tries to send leave events

### Component Changes

#### CompositorServer (renamed from XWaylandServer)

```cpp
namespace goggles::input {

class CompositorServer {
public:
    ~CompositorServer();

    CompositorServer(const CompositorServer&) = delete;
    CompositorServer& operator=(const CompositorServer&) = delete;
    CompositorServer(CompositorServer&&) = delete;
    CompositorServer& operator=(CompositorServer&&) = delete;

    [[nodiscard]] static auto create() -> ResultPtr<CompositorServer>;

    [[nodiscard]] auto x11_display() const -> std::string;      // ":1"
    [[nodiscard]] auto wayland_display() const -> std::string;  // "wayland-1"

    void inject_key(uint32_t linux_keycode, bool pressed);
    void inject_pointer_motion(double sx, double sy);
    void inject_pointer_button(uint32_t button, bool pressed);
    void inject_pointer_axis(double value, bool horizontal);

private:
    CompositorServer() = default;

    wl_display* m_display = nullptr;
    wl_event_loop* m_event_loop = nullptr;
    wlr_backend* m_backend = nullptr;
    wlr_renderer* m_renderer = nullptr;
    wlr_allocator* m_allocator = nullptr;
    wlr_compositor* m_compositor = nullptr;
    wlr_xdg_shell* m_xdg_shell = nullptr;
    wlr_seat* m_seat = nullptr;
    wlr_xwayland* m_xwayland = nullptr;
    wlr_keyboard* m_virtual_keyboard = nullptr;

    util::UniqueFd m_event_fd;
    std::vector<wlr_surface*> m_surfaces;
    wlr_surface* m_focused_surface = nullptr;
    int m_display_number = -1;

    wl_listener m_new_xdg_toplevel{};
    wl_listener m_new_xwayland_surface{};

    std::jthread m_compositor_thread;
};

} // namespace goggles::input
```

#### InputForwarder (Simplified)

```cpp
namespace goggles::input {

struct InputForwarder::Impl {
    std::unique_ptr<CompositorServer> server;
};

auto InputForwarder::create() -> ResultPtr<InputForwarder> {
    auto forwarder = std::unique_ptr<InputForwarder>(new InputForwarder());
    forwarder->m_impl->server = GOGGLES_TRY(CompositorServer::create());
    return make_result_ptr(std::move(forwarder));
}

auto InputForwarder::forward_key(const SDL_KeyboardEvent& event) -> Result<void> {
    auto linux_keycode = sdl_to_linux_keycode(event.scancode);
    if (linux_keycode == 0) return {};

    m_impl->server->inject_key(linux_keycode, event.down);
    return {};
}

} // namespace goggles::input
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

### Decision 2: SPSCQueue + eventfd for cross-thread marshaling

**Choice**: Use `goggles::util::SPSCQueue<InputEvent>` + eventfd for wl_event_loop wakeup

**Rationale**:
- Project policy requires `SPSCQueue` for inter-thread communication
- Lock-free, wait-free guarantees (per project threading policy)
- eventfd solely for waking wl_event_loop (single byte notification)
- No mutex contention, predictable latency

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
| XWayland destroy signal issues | Do NOT use destroy listeners for XWayland; use mutual exclusion focus model |
| Stale XWayland pointers | Clear m_focused_xsurface before wlroots API calls in focus_surface() |

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
