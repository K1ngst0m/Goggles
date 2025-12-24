# Change: Add WSI Virtualization Layer

## Why

Goggles currently captures frames from target applications but the target app still creates and displays its own window. Users want the ability to have Goggles' SDL window act as a proxy, completely replacing the target application's display. This enables use cases like:
- Running games without their own window (headless capture)
- Remote game streaming where only the viewer displays
- CRT shader processing without duplicate windows

## What Changes

- **ADDED:** WSI virtualization mode activated via `GOGGLES_WSI_PROXY=1` environment variable
- **ADDED:** Virtual surface creation replacing platform-specific surfaces (X11/Wayland/XCB)
- **ADDED:** Virtual swapchain with DMA-BUF exportable images
- **ADDED:** Surface capability/format/present mode query virtualization
- **MODIFIED:** Existing `vkQueuePresentKHR` hook to work with virtual swapchains

## Impact

- Affected specs: `vk-layer-capture`
- Affected code: `src/capture/vk_layer/` (new files + hook modifications)
- No external dependencies added
- Linux only (X11 + Wayland + XCB surfaces)