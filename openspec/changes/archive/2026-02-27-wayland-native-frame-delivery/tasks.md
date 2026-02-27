## 1. Remove Vulkan Layer Build Target

- [x] 1.1 Remove `goggles_vklayer` CMake target from `src/capture/CMakeLists.txt`
- [x] 1.2 Remove layer manifest template `config/goggles_layer.json.in` and its configure step
- [x] 1.3 Remove `pixi run dev` manifest install step for the layer JSON
- [x] 1.4 Delete remaining `src/capture/vk_layer/` source files: `layer_main.cpp`,
      `vk_hooks.cpp/hpp`, `vk_dispatch.cpp/hpp`, `vk_capture.cpp/hpp`, `ipc_socket.cpp/hpp`,
      `frame_dump.cpp/hpp`, `logging.hpp`

## 2. Remove IPC Protocol and Receiver

- [x] 2.1 Delete `src/capture/capture_protocol.hpp`
- [x] 2.2 Delete `src/capture/capture_receiver.hpp` and `capture_receiver.cpp`
- [x] 2.3 Remove `CaptureReceiver` from `src/capture/CMakeLists.txt` source list
- [x] 2.4 Remove `goggles_capture` library target if it becomes empty

## 3. Add Explicit Sync Protocol to `CompositorServer`

- [x] 3.1 Add `wp_linux_drm_syncobj_v1` protocol XML to the wayland-protocols source set in
      `CMakeLists.txt` (if not already present via wlroots)
- [x] 3.2 In `CompositorServer::Impl`, bind `wp_linux_drm_syncobj_manager_v1` global when the
      wlroots backend reports DRM syncobj support
- [x] 3.3 Implement `wp_linux_drm_syncobj_surface_v1` resource per surface: store acquire and
      release timeline point (syncobj FD + point value) in surface state
- [x] 3.4 In `render_surface_to_frame()`, extract the acquire timeline point from committed
      surface state if present
- [x] 3.5 Propagate the acquire timeline point (syncobj FD + point) through
      `util::ExternalImageFrame::sync_fd` to the application

## 4. Extend `util::ExternalImageFrame`

- [x] 4.1 Add `std::optional<util::UniqueFd> sync_fd` field to `ExternalImageFrame`
      (`src/util/external_image.hpp`)
- [x] 4.2 Add `uint64_t sync_point = 0` field alongside `sync_fd` for the timeline point value
- [x] 4.3 Update `CompositorServer::get_presented_frame()` to dup and forward the acquire
      timeline FD when present

## 5. Add Explicit Fence Wait in `VulkanBackend`

- [x] 5.1 Remove `import_sync_semaphores()`, `cleanup_sync_semaphores()`, and all
      `m_frame_ready_sem` / `m_frame_consumed_sem` members and logic
- [x] 5.2 In the `render()` path, when `source_frame->sync_fd` is present: import as
      `VkSemaphore` via `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT` (binary semaphore) and
      add as a wait semaphore in the queue submit before sampling the captured image
- [x] 5.3 When `source_frame->sync_fd` is absent (frames from compositor path without explicit
      sync): proceed without additional wait (implicit DMA-BUF sync as current behavior)
- [x] 5.4 Signal a release fence after submit and write back the release timeline point to the
      compositor surface via `wp_linux_drm_syncobj_surface_v1.set_release_point`

## 6. Remove `CaptureReceiver` and `SessionCaptureMode` from `Application`

- [x] 6.1 Remove `m_capture_receiver` member, `init_capture_receiver()`, and
      `handle_sync_semaphores()` from `Application`
- [x] 6.2 Remove `SessionCaptureMode` enum and `m_session_capture_mode`; remove
      `is_direct_vulkan_session()` helper
- [x] 6.3 Simplify `update_frame_sources()`: remove `poll_frame()`, resolution request sending,
      semaphore handling; call only `compositor_server->get_presented_frame()`
- [x] 6.4 Simplify `render_frame()`: remove `capture_receiver` priority branch; single source
      from `m_surface_frame`
- [x] 6.5 Remove `m_initial_resolution_sent` flag and its resolution relay logic
- [x] 6.6 Update `sync_surface_filters()`: remove `is_direct_vulkan_session()` branch from
      `default_filter_enabled`

## 7. Remove `capture.backend` Config Key

- [x] 7.1 Remove `capture.backend` field from `util::Config` / `config.hpp`
- [x] 7.2 Remove parsing in `util::load_config()`
- [x] 7.3 Remove `capture.backend` from `config/goggles.toml` and any CLI arg mapping

## 8. Build and Quality Gates

- [x] 8.1 `pixi run build -p debug` — zero compile errors
- [x] 8.2 `pixi run build -p quality` — zero clang-tidy warnings
- [x] 8.3 `pixi run test -p test` — all tests pass
- [x] 8.4 Confirm no remaining references to `CaptureReceiver`, `capture_protocol`,
      `LayerSocketClient`, `CaptureManager`, `SessionCaptureMode::direct_vulkan`,
      `frame_ready_sem`, `frame_consumed_sem` in `src/`
- [x] 8.5 Runtime smoke test: Proton Vulkan game launches inside Goggles compositor, frame
      appears in filter chain with no tearing artefacts; Qt/GTK launcher window captured
      alongside game window
