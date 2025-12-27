# Design: Input Forwarding Architecture

## Overview

Input forwarding enables users to control captured Vulkan applications by pressing keys in the Goggles viewer window. The solution uses a nested XWayland server (via headless wlroots compositor) combined with XTest injection to generate real X11 protocol events that bypass synthetic input filters.

## Architecture Layers

### Layer 1: SDL Event Source (Existing)

```
User presses W key
    ↓
SDL3 generates SDL_EVENT_KEY_DOWN
    ↓
main.cpp event loop receives event
```

**No changes to SDL integration.**

### Layer 2: Input Forwarding Module (New)

```cpp
namespace goggles::input {

class InputForwarder {
public:
    [[nodiscard]] auto init() -> Result<void>;
    [[nodiscard]] auto forward_key(const SDL_KeyboardEvent& event) -> Result<void>;
    [[nodiscard]] auto display_number() const -> int;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
```

**`InputForwarder::Impl`** contains:
- `XWaylandServer` instance (compositor thread, wlroots objects)
- X11 `Display*` connection to nested XWayland
- SDL → Linux keycode → X11 keycode translation map

**Responsibilities**:
- Start headless Wayland compositor on wayland-N socket
- Spawn XWayland process connected to compositor (auto-select :1, :2, ...)
- Open X11 client connection to :N
- Translate SDL scancodes to X11 keycodes
- Call `XTestFakeKeyEvent()` to inject keys
- Send DISPLAY number to layer via IPC

### Layer 3: XWayland Server (New)

```cpp
namespace goggles::input {

class XWaylandServer {
public:
    [[nodiscard]] auto start() -> Result<int>; // Returns DISPLAY number
    void stop();

private:
    struct wlr_backend* m_backend;
    struct wlr_compositor* m_compositor;
    struct wlr_seat* m_seat;
    struct wlr_xwayland* m_xwayland;
    struct wl_display* m_display;
    struct wl_event_loop* m_event_loop;
    std::jthread m_compositor_thread;
};

} // namespace goggles::input
```

**Responsibilities**:
- Create headless wlroots backend (`wlr_headless_backend_create`)
- Create minimal Wayland compositor (no rendering, just protocol infrastructure)
- Start XWayland server via `wlr_xwayland_create()`
- Run Wayland event loop in dedicated thread
- Clean up all wlroots resources on shutdown

**Why headless backend?**
- No GPU/display conflicts with host compositor
- No actual frame composition (XWayland only needs Wayland socket)
- Lightweight: ~1 MB memory, <1% CPU

### Layer 4: IPC Protocol Extension (Modified)

**Existing**: DMA-BUF fd + metadata over Unix socket `\0goggles/vkcapture`

**New message types**:
```cpp
enum class CaptureMessageType : uint32_t {
    client_hello = 1,
    texture_data = 2,
    control = 3,
    semaphore_init = 4,      // Existing
    frame_metadata = 5,      // Existing
    config_request = 6,      // NEW
    input_display_ready = 7, // NEW
};

struct CaptureConfigRequest {
    CaptureMessageType type = CaptureMessageType::config_request;
    uint32_t version = 1;
    uint32_t reserved[2]{};
};
static_assert(sizeof(CaptureConfigRequest) == 16);

struct CaptureInputDisplayReady {
    CaptureMessageType type = CaptureMessageType::input_display_ready;
    int32_t display_number;
    uint32_t reserved[2]{};
};
static_assert(sizeof(CaptureInputDisplayReady) == 16);
```

**Two-Phase Protocol Flow**:

**Phase 1: Config Handshake (Layer Constructor)**
1. Layer constructor connects to socket
2. Sends `config_request` message
3. Goggles responds with `input_display_ready` containing DISPLAY number
4. Layer sets `DISPLAY=:N` environment variable
5. Layer closes config connection

**Phase 2: Frame Capture (on_present)**
6. Layer reconnects on first `vkQueuePresentKHR`
7. Sends `client_hello` message
8. Normal frame capture proceeds

**Why two connections?**
- Layer constructor runs before app main (need DISPLAY early)
- Socket connection for frame capture happens on first present (after app main)
- Separate config connection solves timing issue
- Clean separation of concerns (config vs frame data)

### Layer 5: Vulkan Layer Integration (Modified)

**Current**: Layer loads, intercepts `vkQueuePresentKHR`, sends frames

**New**: Layer performs config handshake in constructor before app initializes

```cpp
__attribute__((constructor(101))) static void
layer_early_init()
{
    LayerSocketClient config_socket;
    if (!config_socket.connect()) {
        return;  // No Goggles instance running, use default DISPLAY
    }

    CaptureConfigRequest request{};
    request.type = CaptureMessageType::config_request;
    request.version = 1;

    if (!config_socket.send(&request, sizeof(request))) {
        return;
    }

    CaptureInputDisplayReady response{};
    if (!config_socket.receive_with_timeout(&response, sizeof(response), 100)) {
        return;  // Timeout, use default DISPLAY
    }

    char display_str[32];
    snprintf(display_str, sizeof(display_str), ":%d", response.display_number);
    setenv("DISPLAY", display_str, 1);

    // Config socket destroyed on scope exit
}
```

**Constructor priority**: `101` (before default `100` for other constructors, ensures DISPLAY set first)

**Connection lifecycle**:
- Config connection: Created and destroyed in constructor
- Frame capture connection: Created later in `on_present()` (existing code unchanged)

## Data Flow: Keyboard Event Path

```
1. User presses W in Goggles window
   ↓
2. SDL3 generates SDL_EVENT_KEY_DOWN (scancode=26)
   ↓
3. main.cpp calls input_forwarder.forward_key(event)
   ↓
4. InputForwarder::forward_key():
   - Translate scancode: SDL 26 → Linux KEY_W (17) → X11 25
   - Call XTestFakeKeyEvent(display_to_nested, 25, True, CurrentTime)
   - Call XFlush(display_to_nested)
   ↓
5. XWayland receives XTest request on Xorg internal API
   ↓
6. XWayland generates X11 KeyPress event on wire protocol
   ↓
7. Captured app's XNextEvent() / xcb_poll_for_event() receives KeyPress
   ↓
8. App processes input (indistinguishable from physical keyboard)
```

**Latency**: ~1-2ms (SDL → XTest → XWayland → App)

## Threading Model

### Main Thread (Existing)
- SDL event loop
- Vulkan rendering
- `InputForwarder::forward_key()` calls (non-blocking X11 send)

### Compositor Thread (New)
- Runs `wl_display_run()` event loop
- Handles XWayland lifecycle events
- Processes Wayland protocol requests (minimal, XWayland only)

**Synchronization**: None required (X11 connection thread-safe for write)

## Error Handling Strategy

### InputForwarder::init()

```cpp
auto InputForwarder::init() -> Result<void> {
    GOGGLES_TRY(m_impl->server.start());  // Returns Result<int> with DISPLAY number

    auto x11_result = open_x11_connection(m_impl->server.display_number());
    if (!x11_result) {
        m_impl->server.stop();  // Cleanup on failure
        return make_error(ErrorCode::input_init_failed,
                         "Failed to connect to nested XWayland");
    }
    m_impl->x11_display = *x11_result;

    GOGGLES_TRY(send_display_to_layer(m_impl->server.display_number()));

    return ok();
}
```

**Error codes**:
- `ErrorCode::input_init_failed` - XWayland start failed or X11 connection failed
- `ErrorCode::input_socket_send_failed` - IPC handshake failed

### XWaylandServer::start()

```cpp
auto XWaylandServer::start() -> Result<int> {
    // Try :1, :2, :3... until successful
    for (int display_num = 1; display_num < 10; ++display_num) {
        auto socket_name = fmt::format("wayland-{}", display_num);
        const char* socket = wl_display_add_socket(m_display, socket_name.c_str());

        if (socket) {
            // Success, start XWayland on :N
            m_xwayland = wlr_xwayland_create(m_display, m_compositor, false);
            if (!m_xwayland) {
                return make_error(ErrorCode::input_init_failed,
                                 "XWayland creation failed");
            }

            return ok(display_num);
        }
    }

    return make_error(ErrorCode::input_init_failed,
                     "No available DISPLAY numbers (1-9 all bound)");
}
```

### Graceful Degradation

If `InputForwarder::init()` fails:
1. Log error with `GOGGLES_LOG_ERROR`
2. Continue app startup without input forwarding
3. User can still view captured frames, just can't control app
4. `forward_key()` becomes no-op (early return if not initialized)

## Resource Management

### RAII Ownership

```cpp
struct InputForwarder::Impl {
    XWaylandServer server;                    // RAII: stops in destructor
    Display* x11_display = nullptr;           // Closed in destructor via XCloseDisplay
    std::unique_ptr<KeyTranslationMap> keymap; // RAII

    ~Impl() {
        if (x11_display) {
            XCloseDisplay(x11_display);
        }
        // server.~XWaylandServer() called automatically, stops compositor
    }
};

struct XWaylandServer {
    struct wl_display* m_display = nullptr;
    struct wlr_backend* m_backend = nullptr;
    // ... other wlroots objects
    std::jthread m_compositor_thread;  // Joins automatically in destructor

    ~XWaylandServer() {
        stop();
    }

    void stop() {
        if (m_display) {
            wl_display_terminate(m_display);  // Stops event loop
        }
        // Thread joins automatically via std::jthread

        // Clean up wlroots in correct order
        if (m_xwayland) wlr_xwayland_destroy(m_xwayland);
        if (m_seat) wlr_seat_destroy(m_seat);
        if (m_compositor) wlr_compositor_destroy(m_compositor);
        if (m_backend) wlr_backend_destroy(m_backend);
        if (m_display) wl_display_destroy(m_display);
    }
};
```

**No raw pointers in public API**. All resources owned via RAII.

## Performance Characteristics

### Memory
- XWaylandServer: ~1 MB (wlroots + XWayland process)
- InputForwarder: ~4 KB (PIMPL + X11 connection)
- Total overhead: ~1 MB

### CPU
- Compositor thread: <0.5% (event loop mostly idle)
- forward_key(): <10 μs per call (XTest + XFlush)

### Latency
- SDL event → XTest → XWayland → App: ~1-2 ms

## Testing Strategy

### Unit Tests (Deferred)
- Keycode translation (SDL → Linux → X11)
- Error handling paths

### Integration Tests (Manual)
1. Start Goggles
2. Start test app with `GOGGLES_CAPTURE=1`
3. Press W/A/S/D in Goggles window
4. Verify test app prints `[Input] KEY DOWN: name='W'`

### Compatibility Testing
- X11 native apps (test_app)
- Wine/DXVK apps (Resident Evil 4)
- Different keyboard layouts (US, DE, etc.)

## Future Enhancements

### Phase 2: Mouse Support
- Add `forward_mouse()` method
- XTestFakeMotionEvent, XTestFakeButtonEvent
- Mouse pointer confinement (optional)

### Phase 3: Wayland Native Apps
- Replace XTest with libei (Wayland input injection)
- Requires different protocol (ei_seat, ei_keyboard)

### Phase 4: Multi-App Focus
- Track multiple captured app windows
- Route input based on focus policy (user-selectable)

## Comparison with Alternatives

| Approach | Pros | Cons | Verdict |
|----------|------|------|---------|
| **XTest → XWayland** (chosen) | Real X11 events, Wine compatible, no special perms | Requires wlroots dep | ✅ Best |
| uinput | Direct kernel injection | Needs root/groups, Wine filters | ❌ Rejected |
| Cage | Full compositor | Complex, unneeded display output | ❌ Rejected |
| SDL forwarding | No deps | No way to inject cross-DISPLAY | ❌ Impossible |

## Dependencies Justification

| Dependency | Why Required | Size Impact |
|------------|--------------|-------------|
| wlroots | Only library providing headless Wayland compositor + XWayland integration | ~500 KB lib |
| wayland-server | Protocol implementation (wlroots dependency) | ~200 KB lib |
| xkbcommon | Keyboard layout handling (wlroots dependency) | ~300 KB lib |
| libX11 | X11 client (connect to XWayland) | Already installed |
| libXtst | XTest extension | ~20 KB lib |

**Total new deps**: wlroots + wayland-server + xkbcommon = ~1 MB

All are system packages, no build-time compilation required.
