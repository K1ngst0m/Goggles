# Change: Fix WSI Proxy DMA-BUF Metadata (stride/offset/modifier)

## Why
WSI proxy virtual swapchain frames need to carry correct DMA-BUF layout metadata (DRM format modifier + offset) so the viewer can import and sample them reliably.

## What Changes
- Update WSI proxy virtual swapchain image allocation to prefer `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` when supported, and record the selected DRM modifier per image.
- Extend WSI proxy present-to-viewer IPC to send `stride`, `offset`, and `modifier` for each virtual swapchain frame.
- Extend viewer-side `CaptureFrame` to store the DMA-BUF `offset` and plumb it into Vulkan import.

## Impact
- Affected specs: `vk-layer-capture`, `render-pipeline`
- Affected code:
  - `src/capture/vk_layer/wsi_virtual.*`, `src/capture/vk_layer/vk_hooks.cpp`
  - `src/capture/capture_receiver.*`
  - `src/render/backend/vulkan_backend.cpp`
