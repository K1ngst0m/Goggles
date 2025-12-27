# Proposal: Add Input Forwarding

## Problem Statement

Captured Vulkan applications run with redirected display (`DISPLAY=:N`) and receive frames from the Goggles viewer, but users cannot control them because keyboard input goes to the Goggles window instead. Standard input forwarding methods fail because:

1. Captured apps run on a different X11 display server
2. Synthetic input methods (uinput, XTest to host) are filtered by applications (especially Wine/DXVK)
3. No mechanism exists to route input from viewer window to captured application

Users must manually focus the captured application window (which may be invisible) or use workarounds like SSH + xinput, breaking the seamless capture experience.

## Proposed Solution

Introduce an input forwarding module (`src/input/`) that creates a nested XWayland server for captured applications and forwards keyboard events from the Goggles SDL window via XTest injection into the nested server.

### Architecture

```
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
2. **`src/input/input_forwarder.cpp/hpp`**: Public API class (PIMPL pattern), forwards SDL keyboard events via XTest
3. **Extended IPC protocol**: Add `input_display_ready` message to existing Unix socket for DISPLAY handshake

### Integration Points

- **`src/app/main.cpp`**: Instantiate `InputForwarder`, call `init()` before capture receiver starts
- **`src/capture/vk_layer/vk_capture.cpp`**: Layer constructor receives DISPLAY number via IPC, sets `DISPLAY=:N` before app main
- **`src/capture/capture_protocol.hpp`**: Add `CaptureInputDisplayReady` message type

## Benefits

- **Seamless UX**: Users control captured apps by pressing keys in viewer window
- **Wine/DXVK compatible**: XTest → XWayland generates real X11 events, not filtered
- **Zero config**: Automatic DISPLAY selection (:1, :2, ...) and handshake via existing socket
- **Minimal deps**: System packages only (wlroots, wayland-server, xkbcommon, libX11, libXtst)

## Non-Goals

- Mouse input forwarding (defer to future work)
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

## Dependencies

All system packages (no CPM changes required):

| Package | Version | Purpose |
|---------|---------|---------|
| wlroots | 0.19 | Headless Wayland compositor |
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
| Layer timing (IPC before DISPLAY set) | Layer blocks on IPC receive (timeout: 100ms) before app main |

## Success Criteria

- User presses W/A/S/D in Goggles window → captured app receives KeyPress events
- Works with Wine/DXVK applications (tested with RE4)
- No configuration required (automatic DISPLAY handshake)
- Clean shutdown (no segfaults, proper resource cleanup)
- Passes `openspec validate add-input-forwarding --strict`
