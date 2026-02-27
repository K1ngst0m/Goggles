# Change: Drop WSI Proxy and Simplify Capture Path

## Why

The WSI proxy mode (`GOGGLES_WSI_PROXY=1`) intercepts platform surface creation and replaces
real swapchains with virtual ones. This breaks compatibility with Vulkan overlay layers (e.g.
Steam overlay, MangoHud) that rely on a real swapchain being visible to downstream layers, and
adds significant complexity: two parallel code paths through every hook function, ~1,252 lines of
virtual surface/swapchain bookkeeping, and duplicated DMA-BUF export infrastructure.

The normal capture path (add `TRANSFER_SRC_BIT`, `CmdCopyImage` to export image, timeline
semaphore sync) covers all Linux gaming use cases reliably without these drawbacks.

Additionally, the compositor `present_swapchain` is hardcoded to `XRGB8888 + DRM_FORMAT_MOD_LINEAR`,
which discards sRGB format information from game output. CRT shaders that expect sRGB-tagged input
receive incorrect gamma through the compositor capture path.

## What Changes

- **REMOVED:** WSI proxy mode — `WsiVirtualizer`, virtual surface/swapchain tracking, all
  `if (virt.is_enabled())` branches in hook functions (~1,252 LOC)
- **REMOVED:** `GOGGLES_WSI_PROXY` environment variable and its handling in `CaptureManager`
- **REMOVED:** `enqueue_virtual_frame()`, `VirtualFrameInfo`, `try_dump_present_image()`,
  virtual frame counter from `CaptureManager`
- **REMOVED:** Resolution relay from `CaptureReceiver` control messages to `WsiVirtualizer`
- **REMOVED:** `SurfaceCapturePath::vulkan` enum value — never set anywhere in the codebase
- **SIMPLIFIED:** `vk_hooks.cpp` collapses to a single real-swapchain capture path with no
  virtual dispatch branches
- **FIXED:** Compositor `present_swapchain` format negotiated from the DRM allocator's supported
  format/modifier set instead of being hardcoded to `XRGB8888 + LINEAR`
- **DOCUMENTED:** `downsample_pass` pre-chain stage is the canonical resolution control for CRT
  shader workflows; `request_surface_resize()` remains best-effort UI convenience only

## Impact

- Affected specs: `vk-layer-capture`
- Affected code:
  - `src/capture/vk_layer/wsi_virtual.hpp` — deleted
  - `src/capture/vk_layer/wsi_virtual.cpp` — deleted
  - `src/capture/vk_layer/vk_hooks.cpp` — remove all virtual branches, simplify all hooks
  - `src/capture/vk_layer/vk_capture.hpp` — remove `VirtualFrameInfo`, virtual frame counter,
    `enqueue_virtual_frame`, `try_dump_present_image`
  - `src/capture/vk_layer/vk_capture.cpp` — remove virtual frame path in `CaptureManager`
  - `src/compositor/compositor_server.hpp` — remove `SurfaceCapturePath::vulkan`
  - `src/compositor/compositor_server.cpp` — fix `setup_output()` format negotiation
  - `src/app/application.cpp` — remove `SurfaceCapturePath::vulkan` reference in
    `sync_surface_filters()`
  - `config/goggles.toml` — remove any WSI proxy config keys if present
- No new dependencies
- **BREAKING:** `GOGGLES_WSI_PROXY=1` launch scripts stop working silently (env var ignored or
  logged as unknown)
