# Change: Add Surface Selector for Multi-Surface Input Routing

## Why

When running inside Steam, multiple surfaces may connect to the compositor (game window, Steam overlay, notifications, MangoHud). The current single-surface focus model cannot distinguish between them, causing input to go to the wrong surface or overlay failures. Auto-detection via X11 atoms is complex and error-prone. A hybrid approach with manual override via ImGui provides visibility and control.

## What Changes

- Add `SurfaceInfo` struct to expose surface metadata (id, title, class, dimensions, type)
- Track all connected surfaces with unique IDs in CompositorServer
- Add `get_surfaces()` API to enumerate connected surfaces
- Add `set_input_target(id)` and `clear_input_override()` for manual selection
- Add new ImGui window (F4) displaying surface list with click-to-select
- Show current input target with "(auto)" or "(manual)" indicator

## Impact

- Affected specs: `input-forwarding`
- Affected code:
  - `src/input/compositor_server.hpp` - SurfaceInfo struct, surface enumeration API
  - `src/input/compositor_server.cpp` - surface tracking, manual override logic
  - `src/input/input_forwarder.hpp` - expose surface list and selection
  - `src/input/input_forwarder.cpp` - forward to CompositorServer
  - `src/ui/imgui_layer.hpp` - SurfaceSelectorState, draw method, F4 toggle
  - `src/ui/imgui_layer.cpp` - surface selector window implementation
  - `src/app/application.cpp` - poll surfaces, update UI state, F4 handler

## Design Rationale

**Why NOT wlr_scene:** Gamescope does not use wlr_scene. The scene-graph API is designed for compositors that render surfaces to a display. Goggles is headless (no rendering output), so wlr_scene provides no benefit and adds unnecessary complexity.

**Why NOT full auto-detection:** Detecting overlay windows via X11 atoms (`STEAM_OVERLAY`, `GAMESCOPE_EXTERNAL_OVERLAY`) requires:
- Polling X11 properties on XWayland surfaces
- Handling property change events for dynamic atom updates
- Race conditions between window creation and atom assignment
- Edge cases (Wine helper windows, third-party overlays without atoms)

**Why hybrid approach:**
- Simple default: first surface receives input (predictable)
- frame_done already sent to all surfaces on commit (no change needed)
- Manual override via UI for edge cases
- Visible surface list aids debugging
- Can add auto-detection later if needed

**X11 metadata retrieval:** Surface title (`WM_NAME`) and class (`WM_CLASS`) are queried from XWayland surfaces during association. This uses the existing X11 connection from wlr_xwayland, adding no new dependencies.

## Non-Goals

- Automatic overlay detection via X11 atoms (deferred, may add later)
- Multiple simultaneous input targets
- Surface z-ordering or stacking (not needed for headless compositor)
