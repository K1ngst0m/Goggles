# Change: Add Non-Vulkan Surface Presentation

## Why

Non-Vulkan clients (Wayland/XWayland) can connect for input, but their frames never reach the
Goggles viewer. That blocks overlays, launchers, and desktop UIs from being visible, even though
input forwarding works. The viewer needs a compositor-driven presentation path for these clients.

## What Changes

- Render the selected non-Vulkan surface into a headless compositor output and export a DMA-BUF.
- Expose the latest compositor-presented frame via `InputForwarder` without altering input routing.
- When no Vulkan capture frame is available, feed compositor DMA-BUF frames into the existing
  Vulkan viewer render path.
- Keep the Vulkan layer capture pipeline unchanged.
- Fall back to input-only mode if compositor presentation is unavailable.

## Impact

- Affected specs: `compositor-capture` (new)
- Affected code:
  - `src/input/compositor_server.hpp` - SurfaceFrame and presentation state
  - `src/input/compositor_server.cpp` - swapchain render/export of selected surface
  - `src/input/input_forwarder.hpp` - compositor frame access API
  - `src/input/input_forwarder.cpp` - forwarder passthrough
  - `src/app/application.hpp` - surface frame tracking
  - `src/app/application.cpp` - render path selection + DRM/Vulkan format mapping
  - `src/util/drm_fourcc.hpp` - local DRM FourCC constants
  - `src/util/drm_format.hpp` - DRM to Vulkan format mapping

## Design Rationale

- **Reuse existing viewer pipeline:** importing compositor DMA-BUF frames through the Vulkan backend
  avoids a second renderer and keeps shader/UI handling centralized.
- **No wlr_scene:** the compositor is headless; a full scene graph adds complexity without benefit.
- **Selection via surface selector:** manual override already exists and maps cleanly to a single
  presented surface.

## Non-Goals

- Multi-surface composition, stacking, or tiling.
- Replacing or modifying the Vulkan layer capture path.
- New IPC protocols for compositor frames.
