# Change: Export Cross-Process Timeline Semaphores

## Why

Currently the Layer uses an internal timeline semaphore for GPU sync, with a Worker thread waiting on CPU-side before sending the DMA-BUF FD. This introduces extra CPU-GPU sync overhead and cannot achieve true back-pressure frame throttling.

By exporting semaphores for Goggles to participate in GPU sync directly:
1. Eliminate CPU wait in Layer Worker thread
2. Enable GPU-to-GPU direct synchronization
3. Implement back-pressure via dual semaphores - Layer waits when Goggles is slow

## What Changes

- Create two exportable timeline semaphores: `frame_ready` (Layer→Goggles) and `frame_consumed` (Goggles→Layer)
- Export FDs using `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT`
- Add new IPC message type to transfer semaphore FDs (once per connection)
- Modify capture_frame() to use dual-semaphore sync logic
- Goggles imports semaphores and uses them in render submission

## Impact

- Affected specs: `vk-layer-capture`
- Affected code:
  - `src/capture/capture_protocol.hpp` - New message types
  - `src/capture/vk_layer/vk_capture.cpp` - Semaphore creation and sync logic
  - `src/capture/vk_layer/ipc_socket.cpp` - FD transfer
  - `src/capture/capture_receiver.cpp` - Receive semaphores
  - `src/render/backend/vulkan_backend.cpp` - Import and use semaphores