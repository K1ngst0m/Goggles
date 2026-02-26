# Change: Wayland-Native Frame Delivery

## Why

The current capture architecture uses a bespoke Unix socket IPC channel between the Vulkan layer
and the Goggles process to deliver DMA-BUF frame handles and timeline semaphore FDs. This requires
maintaining a custom wire protocol (`capture_protocol.hpp`), a socket client in the layer
(`LayerSocketClient`), a socket server in the application (`CaptureReceiver`), an async worker
thread in `CaptureManager`, and a `CmdCopyImage` blit from the game swapchain to a separate
export image on every frame.

The same result can be achieved without any of this: the Vulkan layer can present game frames to
Goggles' compositor Wayland socket using standard Vulkan Wayland WSI. The game creates a real
`VkSwapchainKHR` backed by a `wl_surface` on Goggles' `WAYLAND_DISPLAY`. The Wayland driver
(Mesa) handles DMA-BUF export and `wl_surface.commit` internally. Goggles' compositor receives
each frame as a native Wayland surface commit — the same path already used for Qt/GTK windows —
with no separate IPC socket, no copy, and no bespoke protocol.

To close the GPU synchronization gap that exists in the current compositor capture path (where
`wlr_render_pass_submit` is asynchronous and no explicit fence is propagated), the compositor
gains support for the `wp_linux_drm_syncobj_v1` explicit sync protocol. The wlroots backend
attaches acquire/release timeline points to committed buffers, giving the Vulkan backend proper
GPU-side ordering without polling or CPU stalls.

This change depends on `2026-02-26-drop-wsi-proxy-simplify-capture` being completed first.

## What Changes

- **REPLACED:** Vulkan layer IPC mechanism — `CmdCopyImage` + Unix socket + `CaptureReceiver` —
  with standard Vulkan Wayland WSI presenting to Goggles' compositor socket
- **REMOVED:** `LayerSocketClient` (`ipc_socket.hpp/cpp`)
- **REMOVED:** `CaptureReceiver` (`capture_receiver.hpp/cpp`) and its poll loop in `Application`
- **REMOVED:** `capture_protocol.hpp` wire format
- **REMOVED:** `CaptureManager` async worker thread and export image infrastructure
  (`vk_capture.hpp/cpp`)
- **REMOVED:** Remaining Vulkan layer hook infrastructure: `vk_hooks.cpp/hpp`,
  `vk_dispatch.cpp/hpp`, `layer_main.cpp`, `frame_dump.cpp/hpp`, `logging.hpp`,
  `goggles_vklayer` build target and JSON manifest
- **REMOVED:** `handle_sync_semaphores()` and timeline semaphore import path in `Application` and
  `VulkanBackend`
- **ADDED:** `wp_linux_drm_syncobj_v1` explicit sync protocol support in `CompositorServer` —
  acquire timeline point extracted from committed surface state, release point signaled after
  Goggles finishes reading the buffer
- **ADDED:** Direct buffer import path in `VulkanBackend`: committed `wl_buffer` DMA-BUF is
  imported as a `VkImage` without going through `wlr_renderer` re-compositing for the primary
  game surface
- **MODIFIED:** `Application::update_frame_sources()` — single compositor path only; no
  `capture_receiver` priority logic
- **MODIFIED:** `CompositorServer::get_presented_frame()` — propagates explicit sync acquire
  point alongside `ExternalImageFrame` so `VulkanBackend` can wait on the GPU fence before
  sampling
- **MODIFIED:** `util::ExternalImageFrame` — add optional `sync_fd` field for explicit acquire
  fence
- **MODIFIED:** `SessionCaptureMode` — remove `direct_vulkan` variant; compositor is the only
  capture mode

## Impact

- Affected specs: `vk-layer-capture`, `render-pipeline`
- Affected code:
  - `src/capture/vk_layer/` — entire directory removed (all remaining files after proposal 1)
  - `src/capture/capture_receiver.hpp/cpp` — deleted
  - `src/capture/capture_protocol.hpp` — deleted
  - `src/capture/CMakeLists.txt` — remove `goggles_vklayer` target and manifest wiring
  - `src/compositor/compositor_server.hpp/cpp` — add explicit sync protocol support, propagate
    acquire fence in `get_presented_frame()`
  - `src/util/external_image.hpp` — add optional `sync_fd` field
  - `src/render/backend/vulkan_backend.hpp/cpp` — remove timeline semaphore import, add explicit
    fence wait before sampling imported image; add direct `wl_buffer` import path
  - `src/app/application.hpp/cpp` — remove `CaptureReceiver`, `handle_sync_semaphores()`,
    `SessionCaptureMode::direct_vulkan`, `m_initial_resolution_sent` resolution relay
  - `config/goggles.toml` — remove `capture.backend` config key (compositor is unconditional)
- **NEW DEPENDENCY:** `wp_linux_drm_syncobj_v1` Wayland protocol (part of wayland-protocols
  ≥ 1.32; already available in the pixi environment)
- **BREAKING:** `GOGGLES_CAPTURE=1` layer no longer exists; games do not require any special
  environment variable — they connect to `WAYLAND_DISPLAY` / `DISPLAY` as normal
