# Input Forwarding

## Purpose

Explains how Goggles forwards keyboard input from the viewer window to captured Vulkan applications without requiring window focus switching.

## Problem

When capturing frames from a Vulkan application via the layer, the app typically runs headless (no visible window). Users want to control the captured app by pressing keys in the Goggles viewer window, but standard input methods don't work because:

1. The captured app is on a different display server
2. X11/Wayland don't forward events between unrelated windows
3. Synthetic events (XTest, uinput) are filtered by many applications (especially Wine)

## Solution Architecture

### Component Overview

```
┌────────────────────────────────────────────────────────────┐
│  Goggles Application (DISPLAY=:0, viewer window)           │
│                                                             │
│  ┌──────────────┐        ┌─────────────────────┐           │
│  │ SDL Window   │───────▶│ XWayland Host       │           │
│  │ (key events) │        │ (headless wlroots)  │           │
│  └──────────────┘        └──────────┬──────────┘           │
│                                     │                       │
│                          Creates XWayland on DISPLAY=:1    │
│                                     │                       │
│                          ┌──────────▼──────────┐            │
│                          │  X11 Connection     │            │
│                          │  to :1              │            │
│                          │  (XTestFakeKeyEvent)│            │
│                          └──────────┬──────────┘            │
└─────────────────────────────────────┼────────────────────────┘
                                      │
                    XTest injection   │
                                      ▼
┌────────────────────────────────────────────────────────────┐
│  Captured Application (DISPLAY=:1)                         │
│                                                             │
│  ┌────────────────┐                                         │
│  │ Vulkan Layer   │  (sets DISPLAY=:1 before app main)     │
│  └────────────────┘                                         │
│                                                             │
│  App receives genuine X11 KeyPress/KeyRelease events       │
│  (indistinguishable from physical keyboard)                │
└────────────────────────────────────────────────────────────┘
```

### Why XWayland?

XWayland is not used for display/composition. It's used purely as an **X11 protocol server** that captured applications can connect to. This solves the synthetic event filtering problem:

- **XTest injection into XWayland generates real X11 protocol events**
- Apps receive KeyPress/KeyRelease via normal X11 wire protocol
- No XCB_SEND_EVENT flag that apps check to filter synthetic events
- Works with Wine, games, and applications that reject synthetic input

### Why Headless wlroots?

The nested Wayland compositor uses `wlr_headless_backend_create()` which:

- Creates a Wayland socket (wayland-1) without requiring GPU/display
- Doesn't conflict with the host compositor (Wayland/X11)
- Provides the minimal infrastructure XWayland needs to start
- No actual composition or rendering happens

## Data Flow

### Initialization

1. **Goggles starts** → `goggles::input::start_xwayland_host()`
2. **Create headless Wayland compositor** on wayland-1 socket
3. **Start XWayland** on DISPLAY=:1 (connects to wayland-1)
4. **Open X11 connection** from Goggles to :1 for XTest

### Runtime

1. **User presses W key** in Goggles SDL window
2. **SDL generates SDL_EVENT_KEY_DOWN** with scancode
3. **InputForwarder receives event** via SDL event loop
4. **Translate scancode**: SDL → Linux keycode → X11 keycode (+8)
5. **XTestFakeKeyEvent(display_to_1, x11_keycode, True)**
6. **XWayland generates KeyPress event** on :1 wire protocol
7. **Captured app receives event** via XNextEvent/XCB poll
8. **App processes input** as if from physical keyboard

### Shutdown

1. **Goggles exits** → close X11 connection to :1
2. **Terminate XWayland** process
3. **Destroy Wayland compositor** and socket
4. **Cleanup thread resources**

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| XTest injection | Generates real X11 protocol events, not filtered by apps |
| XWayland without composition | Only need X11 server, not display output |
| Headless wlroots backend | No GPU/display conflict with host compositor |
| Layer sets DISPLAY=:1 | Captured app connects to nested XWayland automatically |
| SDL event forwarding | Integrates with existing Goggles SDL window |

## Limitations

### Current Implementation

- **Basic mouse support**: Mouse button, motion, and wheel events are forwarded but coordinate mapping is 1:1 (no scaling between viewer and target window)
- **Limited scancode map**: Only common keys (A-Z, 0-9, ESC, Enter, Space, arrows, modifiers) mapped
- **No modifier state tracking**: Shift/Ctrl/Alt forwarded as key events but state not synchronized
- **Single app**: Only one captured app receives input

### Future Enhancements

1. **Full keyboard map**: Complete SDL → X11 scancode translation for all keys
2. **Modifier state synchronization**: Track and synchronize Shift/Ctrl/Alt/Super state between viewer and target
3. **Mouse coordinate mapping**: Scale/transform mouse coordinates to match target window dimensions
4. **Pointer confinement**: Optional relative mouse mode and pointer grabbing support
5. **Multi-app focus**: Input routing to specific captured window
6. **Wayland native**: Support Wayland apps via libei (Wayland input injection)

## Dependencies

All dependencies are system packages (not managed by CPM):

| Library | Version | Purpose |
|---------|---------|---------|
| wlroots | 0.18 | Wayland compositor library (headless backend) |
| wayland-server | Latest | Wayland protocol server implementation |
| xkbcommon | Latest | Keyboard keymap handling for Wayland |
| libX11 | Latest | X11 client library (connect to :1) |
| libXtst | Latest | XTest extension (fake input) |

SDL3 is already in the project via CPM.

## Integration with Capture Layer

The Vulkan layer (`src/capture/vk_layer/vk_capture.cpp`) has a constructor that runs before the application's `main()`:

```cpp
__attribute__((constructor)) static void
layer_early_init()
{
    const char* old_display = getenv("DISPLAY");
    setenv("DISPLAY", ":1", 1);
    // App now connects to XWayland on :1 instead of host X server on :0
}
```

This ensures captured applications automatically connect to the nested XWayland without requiring environment variable passing or launch wrapper scripts.

## Comparison with Alternative Approaches

### Rejected: Direct uinput Injection

```
User input → Goggles → uinput → /dev/input/eventX → App
```

**Problem**: Requires root or uinput group membership. Filtered by Wine (checks event source).

### Rejected: Cage Compositor

```
User input → Goggles → Cage IPC → Cage compositor → App
```

**Problem**: Cage is a full compositor with display output we don't need. Complex integration. Abandoned in favor of minimal XWayland-only approach.

### Chosen: XTest to XWayland

```
User input → Goggles → XTest → XWayland :1 → App
```

**Advantages**: No special permissions, works with Wine, minimal dependencies, no display output.

## Testing

### Manual Test

```bash
# Terminal 1: Start Goggles
./build/debug/bin/goggles

# Terminal 2: Start test app
GOGGLES_CAPTURE=1 GOGGLES_WSI_PROXY=1 ./test_app

# Focus Goggles window, press W/A/S/D keys
# test_app terminal should show: [Input] KEY DOWN: scancode=26 name='W'
```

### Verify XWayland Running

```bash
# Should show XWayland process on :1
ps aux | grep Xwayland

# Should show test_app connected to :1
DISPLAY=:1 xdotool search --class test_app
```

## See Also

- [Architecture](architecture.md) - System overview
- [DMA-BUF Sharing](dmabuf_sharing.md) - How frames are captured
- [Project Policies](project_policies.md) - Coding standards
