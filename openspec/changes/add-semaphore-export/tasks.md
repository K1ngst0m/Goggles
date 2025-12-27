# Tasks: Export Cross-Process Timeline Semaphores

## 1. Protocol Layer
- [x] 1.1 Add `semaphore_init` message type to `capture_protocol.hpp`
- [x] 1.2 Add `frame_metadata` message type with timeline value field
- [x] 1.3 Add static_assert for struct sizes

## 2. Layer - Vulkan Setup
- [x] 2.1 Add `GetSemaphoreFdKHR` to `VkDeviceFuncs` in `vk_dispatch.hpp`
- [x] 2.2 Add `VK_KHR_external_semaphore` and `VK_KHR_external_semaphore_fd` to required extensions
- [x] 2.3 Add `frame_ready_sem` and `frame_consumed_sem` fields to `SwapData`
- [x] 2.4 Modify `init_sync_primitives()` to create exportable timeline semaphores
- [x] 2.5 Export semaphore FDs via `vkGetSemaphoreFdKHR`

## 3. Layer - IPC
- [x] 3.1 Add `send_semaphores()` to `ipc_socket.cpp` (SCM_RIGHTS with two FDs)
- [x] 3.2 Add `send_frame_metadata()` for per-frame data without FD
- [x] 3.3 Send semaphores on first frame after connection

## 4. Layer - Sync Logic
- [x] 4.1 Modify `capture_frame()` to wait on `frame_consumed` before copy (back-pressure)
- [x] 4.2 Signal `frame_ready` after copy submission
- [x] 4.3 Handle timeout in wait (detect disconnect)
- [x] 4.4 Reset semaphore state on reconnection

## 5. Goggles - Receiver
- [x] 5.1 Handle `semaphore_init` message in `capture_receiver.cpp`
- [x] 5.2 Extract two FDs from SCM_RIGHTS ancillary data
- [x] 5.3 Store FDs and expose via getters
- [x] 5.4 Handle `frame_metadata` message type

## 6. Goggles - Backend
- [x] 6.1 Add `import_sync_semaphores()` to `vulkan_backend.cpp`
- [x] 6.2 Create timeline semaphores and import FDs via `vkImportSemaphoreFdKHR`
- [x] 6.3 Modify render submission to wait on `frame_ready`
- [x] 6.4 Signal `frame_consumed` after render

## 7. Integration
- [x] 7.1 Call `import_sync_semaphores()` when semaphores received
- [x] 7.2 Handle fallback when import fails
- [x] 7.3 Test reconnection scenario

## 8. Testing
- [x] 8.1 Test basic sync flow with vkcube
- [ ] 8.2 Test back-pressure (slow Goggles)
- [x] 8.3 Test reconnection

## 9. Robustness (Added)
- [x] 9.1 Use raw C API for `vkGetMemoryFdPropertiesKHR` to handle stale DMA-BUF fds
- [x] 9.2 Use raw C API for `vkImportSemaphoreFdKHR` to handle stale semaphore fds
- [x] 9.3 Use raw C API for `vkAcquireNextImageKHR` to handle device errors gracefully
- [x] 9.4 Reset handles to null after destroy in `cleanup_imported_image()`
- [x] 9.5 Track `m_last_signaled_frame` to prevent signal value collision