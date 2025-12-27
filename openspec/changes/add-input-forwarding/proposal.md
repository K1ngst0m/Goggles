# Proposal: Add Input Forwarding

## Problem Statement

When Goggles captures frames from a Vulkan application via the layer, users cannot control the captured application by pressing keys in the Goggles viewer window. Input events go to the Goggles window instead of the captured application.

Standard input forwarding approaches fail:

1. **Synthetic input injection** (uinput, XTest to host DISPLAY) is filtered by many applications, especially Wine/DXVK
2. **Focusing the captured app window** breaks the seamless viewing experience and may not work if the app is headless

Users currently have no way to interact with captured applications while viewing through Goggles.

## Proposed Solution

Introduce an input forwarding module (`src/input/`) that creates a nested XWayland server for captured applications and forwards key + mouse events from the Goggles SDL window via XTest injection into the nested server.

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
2. **`src/input/input_forwarder.cpp/hpp`**: Public API class (PIMPL pattern), forwards SDL key + mouse events via XTest
3. **Extended IPC protocol**: Add a small config handshake (`config_request` → `input_display_ready`) on the existing Unix socket for DISPLAY configuration
4. **`src/capture/capture_receiver.cpp`**: Responds to layer config requests with the DISPLAY selected by `InputForwarder`

### Integration Points

- **`src/app/main.cpp`**: Instantiate `InputForwarder`, call `init()`, and pass `display_number()` to `CaptureReceiver` for the layer handshake
- **`src/capture/vk_layer/vk_capture.cpp`**: Layer constructor performs a config handshake and sets `DISPLAY=:N` before app main
- **`src/capture/capture_protocol.hpp`**: Add `CaptureConfigRequest` + `CaptureInputDisplayReady` message types

## Benefits

- **Seamless UX**: Users control captured apps by pressing keys in viewer window
- **Wine/DXVK compatible**: XTest → XWayland generates real X11 events, not filtered
- **Zero config**: Automatic DISPLAY selection (:1, :2, ...) and handshake via existing socket
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
| Layer timing (config requested before DISPLAY is ready) | Best-effort handshake with timeout; start Goggles (and `InputForwarder`) before launching the captured app |

## Success Criteria

- User presses W/A/S/D in Goggles window → captured app receives KeyPress events
- Works with Wine/DXVK applications (tested with RE4)
- No configuration required (automatic DISPLAY handshake)
- Clean shutdown (no segfaults, proper resource cleanup)
- Passes `openspec validate add-input-forwarding --strict`
