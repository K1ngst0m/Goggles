# Proposal: Add Input Forwarding

## Why

When Goggles captures frames from a Vulkan application via the layer, users cannot control the captured application by pressing keys in the Goggles viewer window. Input events go to the Goggles window instead of the captured application.

Standard input forwarding approaches fail:

1. **Synthetic input injection** (uinput, XTest to host DISPLAY) is filtered by many applications, especially Wine/DXVK
2. **Focusing the captured app window** breaks the seamless viewing experience and may not work if the app is headless

Users currently have no way to interact with captured applications while viewing through Goggles.

## Proposed Solution

Introduce an input forwarding module (`src/input/`) that creates a nested XWayland server for captured applications and forwards key + mouse events from the Goggles SDL window via XTest injection into the nested server.

### Architecture

```text
┌─────────────────────────────────────────┐
│  Goggles (DISPLAY=:0)                   │
│                                         │
│  ┌────────────┐      ┌──────────────┐  │
│  │ SDL Window │─────▶│ InputForwarder│  │
│  │  (viewer)  │ keys │   (src/input/) │  │
│  └────────────┘      └────────┬──────┘  │
│                               │         │
│                  ┌────────────▼──────┐  │
│                  │ XWaylandServer    │  │
│                  │ (headless wlroots)│  │
│                  └────────────┬──────┘  │
│                               │         │
│                    Creates DISPLAY=:N   │
│                               │         │
│                  ┌────────────▼──────┐  │
│                  │ X11 connection    │  │
│                  │ XTestFakeKeyEvent │  │
│                  └────────────┬──────┘  │
└──────────────────────────────┼─────────┘
                                │ XTest
                                ▼
┌─────────────────────────────────────────┐
│  Captured App (DISPLAY=:N)              │
│  Receives real X11 KeyPress events      │
└─────────────────────────────────────────┘
```

**Key insight**: XTest injection into XWayland generates real X11 protocol events (KeyPress/KeyRelease), indistinguishable from physical keyboard, bypassing synthetic event filters.

### Components

1. **`src/input/xwayland_server.cpp/hpp`**: Manages headless Wayland compositor (wlroots) and spawns XWayland process
2. **`src/input/input_forwarder.cpp/hpp`**: Public API class (PIMPL pattern), forwards SDL key + mouse events via XTest
3. **Configuration/CLI**: Input forwarding is opt-in in the viewer; the target app must be launched inside the nested XWayland session (`DISPLAY=:N`)

### Integration Points

- **`src/app/main.cpp`**: Optionally initialize `InputForwarder` (via `create()`) and forward SDL input events via XTest
- **`docs/input_forwarding.md`**: Document how to launch the target inside the nested XWayland session (`DISPLAY=:N`)

## Benefits

- **Seamless UX**: Users control captured apps by pressing keys in viewer window
- **Wine/DXVK compatible**: XTest → XWayland generates real X11 events, not filtered
- **Opt-in**: Disabled by default; enable via config or `--input-forwarding`
- **Minimal deps**: System packages only (wlroots, wayland-server, xkbcommon, libX11, libXtst)
- **Basic mouse support**: Button/motion/wheel events are forwarded (coordinate mapping is currently 1:1 with the viewer window)

## Non-Goals

- Accurate mouse coordinate mapping / scaling between viewer and captured app
- Pointer confinement / relative mouse mode support
- Wayland native app support (X11-only for now)
- Display/composition (XWayland used only as input server)
- Multiple app focus management

## Alternatives Considered

### uinput Injection
**Rejected**: Requires root/uinput group, filtered by Wine

### Cage Compositor
**Rejected**: Full compositor with display output we don't need, complex integration

### Direct SDL Input Forwarding
**Rejected**: No way to inject into different DISPLAY without XTest

## What Changes

### Configuration / CLI
- New config option: `input.forwarding` (default: false)
- New CLI flag: `--input-forwarding` (overrides config to enable)
- Input forwarding only works for targets launched with `DISPLAY=:N` (nested XWayland)
- Wayland input forwarding is not supported yet

### Public API
- New class `goggles::input::InputForwarder` (PIMPL pattern)
  - `create() -> ResultPtr<InputForwarder>`
  - `forward_key(const SDL_KeyboardEvent&) -> Result<void>`
  - `forward_mouse_button(const SDL_MouseButtonEvent&) -> Result<void>`
  - `forward_mouse_motion(const SDL_MouseMotionEvent&) -> Result<void>`
  - `forward_mouse_wheel(const SDL_MouseWheelEvent&) -> Result<void>`
  - `display_number() -> int`
- New class `goggles::input::XWaylandServer` (internal)
  - `start() -> Result<int>`
  - `stop() -> void`

### Error Codes
- `ErrorCode::input_init_failed` - XWayland/compositor startup failure
- `ErrorCode::input_socket_send_failed` - IPC message send failure

### Files Added
- `src/input/input_forwarder.cpp/hpp`
- `src/input/xwayland_server.cpp/hpp`

### Files Modified
- `src/app/main.cpp` - InputForwarder instantiation
- `src/app/cli.hpp` - `--input-forwarding` flag
- `src/util/config.*` - `input.forwarding` config option
- `docs/input_forwarding.md` - usage instructions

## Dependencies

All system packages (no CPM changes required):

| Package | Version | Purpose |
|---------|---------|---------|
| wlroots | 0.18 | Headless Wayland compositor |
| wayland-server | Latest | Wayland protocol server |
| xkbcommon | Latest | Keyboard keymap handling |
| libX11 | Latest | X11 client (connect to nested XWayland) |
| libXtst | Latest | XTest extension for input injection |

SDL3 already in project.

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| DISPLAY conflict (socket already bound) | Auto-select :1, :2, :3... until successful |
| XWayland startup failure | Propagate error via `Result<void>`, log failure, disable input forwarding |
| Memory leak in compositor thread | RAII wrappers for all wlroots objects, explicit cleanup |
| Target launched outside nested XWayland | Document requirement: target must be launched with `DISPLAY=:N` |

## Success Criteria

- User presses W/A/S/D in Goggles window → captured app receives KeyPress events
- Works with Wine/DXVK applications (tested with RE4)
- Enabled explicitly (config/CLI); default behavior remains unchanged when disabled
- Clean shutdown (no segfaults, proper resource cleanup)
- Passes `openspec validate add-input-forwarding --strict`
