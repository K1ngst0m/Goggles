## 1. Delete WSI Proxy Files

- [x] 1.1 Delete `src/capture/vk_layer/wsi_virtual.hpp`
- [x] 1.2 Delete `src/capture/vk_layer/wsi_virtual.cpp`
- [x] 1.3 Remove `wsi_virtual` from `src/capture/vk_layer/CMakeLists.txt` source list

## 2. Simplify `vk_hooks.cpp`

- [x] 2.1 Remove `#include "wsi_virtual.hpp"` and the `virt` accessor call
- [x] 2.2 `CreateXlibSurfaceKHR` — remove virtual branch, keep only real surface passthrough
- [x] 2.3 `CreateXcbSurfaceKHR` — remove virtual branch
- [x] 2.4 `CreateWaylandSurfaceKHR` — remove virtual branch
- [x] 2.5 `DestroySurfaceKHR` — remove virtual surface check, keep only real destroy
- [x] 2.6 `GetPhysicalDeviceSurfaceCapabilitiesKHR` — remove virtual branch
- [x] 2.7 `GetPhysicalDeviceSurfaceFormatsKHR` — remove virtual branch
- [x] 2.8 `GetPhysicalDeviceSurfacePresentModesKHR` — remove virtual branch
- [x] 2.9 `GetPhysicalDeviceSurfaceSupportKHR` — remove virtual branch
- [x] 2.10 `GetPhysicalDeviceSurfaceCapabilities2KHR` — remove virtual branch
- [x] 2.11 `GetPhysicalDeviceSurfaceFormats2KHR` — remove virtual branch
- [x] 2.12 `CreateSwapchainKHR` — remove virtual swapchain branch; keep only real path
- [x] 2.13 `DestroySwapchainKHR` — remove virtual swapchain check
- [x] 2.14 `GetSwapchainImagesKHR` — remove virtual image list branch
- [x] 2.15 `AcquireNextImageKHR` — remove virtual acquire path (FPS limiter, semaphore wait)
- [x] 2.16 `AcquireNextImage2KHR` — remove virtual acquire path
- [x] 2.17 `QueuePresentKHR` — remove virtual-only branch; keep only real present + `on_present()`
- [x] 2.18 `WaitForPresentKHR` — remove virtual early-return branch

## 3. Remove Virtual Frame Infrastructure from `CaptureManager`

- [x] 3.1 Remove `VirtualFrameInfo` struct from `vk_capture.hpp`
- [x] 3.2 Remove `virtual_frame_counter_` member and `get_virtual_frame_counter()` accessor
- [x] 3.3 Remove `enqueue_virtual_frame()` declaration and implementation
- [x] 3.4 Remove `try_dump_present_image()` declaration and implementation
- [x] 3.5 Remove resolution relay to `WsiVirtualizer` in `on_present()` / control message handling
- [x] 3.6 Verify async worker thread `worker_func()` handles only timeline-semaphore frames (virtual path removed)

## 4. Fix `SurfaceCapturePath` Enum

- [x] 4.1 Remove `SurfaceCapturePath::vulkan` from enum in `compositor_server.hpp`
- [x] 4.2 Update `sync_surface_filters()` in `application.cpp`: remove `SurfaceCapturePath::vulkan` branch from `default_filter_enabled` expression

## 5. Fix Compositor `present_swapchain` Format Negotiation

- [x] 5.1 In `CompositorServer::Impl::setup_output()`, replace hardcoded `DRM_FORMAT_XRGB8888 + DRM_FORMAT_MOD_LINEAR` with a query to `wlr_output_get_primary_formats()` against allocator buffer caps
- [x] 5.2 Select preferred format/modifier from the reported set (prefer XRGB8888 or ARGB8888 with tiled modifier if available, fall back to LINEAR)
- [x] 5.3 Keep `present_modifiers` storage as a `std::vector<uint64_t>` to hold the negotiated modifier list
- [x] 5.4 Recreate `present_swapchain` on output reconfigure using the negotiated format

## 6. Config and Environment Cleanup

- [x] 6.1 Remove `GOGGLES_WSI_PROXY` environment variable check from `CaptureManager` / `should_use_wsi_proxy()`
- [x] 6.2 Remove `GOGGLES_WIDTH` / `GOGGLES_HEIGHT` / `GOGGLES_FPS_LIMIT` env var handling that was exclusive to WSI proxy
- [x] 6.3 Log a warning if `GOGGLES_WSI_PROXY=1` is detected at startup, stating the mode has been removed

## 7. Build and Quality Gates

- [x] 7.1 `pixi run build -p debug` — confirm zero compile errors
- [x] 7.2 `pixi run build -p quality` — confirm zero clang-tidy warnings
- [x] 7.3 `pixi run test -p test` — all existing tests pass
- [x] 7.4 Confirm no remaining references to `WsiVirtualizer`, `wsi_virtual`, `enqueue_virtual_frame`, `GOGGLES_WSI_PROXY` in `src/`
