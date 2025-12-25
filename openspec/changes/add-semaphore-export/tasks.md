# Tasks: Export Cross-Process Timeline Semaphores

## 1. Protocol Layer
- [ ] 1.1 Add `semaphore_init` message type to `capture_protocol.hpp`
- [ ] 1.2 Add `frame_metadata` message type with timeline value field
- [ ] 1.3 Add static_assert for struct sizes

## 2. Layer - Vulkan Setup
- [ ] 2.1 Add `GetSemaphoreFdKHR` to `VkDeviceFuncs` in `vk_dispatch.hpp`
- [ ] 2.2 Add `VK_KHR_external_semaphore` and `VK_KHR_external_semaphore_fd` to required extensions
- [ ] 2.3 Add `frame_ready_sem` and `frame_consumed_sem` fields to `SwapData`
- [ ] 2.4 Modify `init_sync_primitives()` to create exportable timeline semaphores
- [ ] 2.5 Export semaphore FDs via `vkGetSemaphoreFdKHR`

## 3. Layer - IPC
- [ ] 3.1 Add `send_semaphores()` to `ipc_socket.cpp` (SCM_RIGHTS with two FDs)
- [ ] 3.2 Add `send_frame_metadata()` for per-frame data without FD
- [ ] 3.3 Send semaphores on first frame after connection

## 4. Layer - Sync Logic
- [ ] 4.1 Modify `capture_frame()` to wait on `frame_consumed` before copy (back-pressure)
- [ ] 4.2 Signal `frame_ready` after copy submission
- [ ] 4.3 Handle timeout in wait (detect disconnect)
- [ ] 4.4 Reset semaphore state on reconnection

## 5. Goggles - Receiver
- [ ] 5.1 Handle `semaphore_init` message in `capture_receiver.cpp`
- [ ] 5.2 Extract two FDs from SCM_RIGHTS ancillary data
- [ ] 5.3 Store FDs and expose via getters
- [ ] 5.4 Handle `frame_metadata` message type

## 6. Goggles - Backend
- [ ] 6.1 Add `import_sync_semaphores()` to `vulkan_backend.cpp`
- [ ] 6.2 Create timeline semaphores and import FDs via `vkImportSemaphoreFdKHR`
- [ ] 6.3 Modify render submission to wait on `frame_ready`
- [ ] 6.4 Signal `frame_consumed` after render

## 7. Integration
- [ ] 7.1 Call `import_sync_semaphores()` when semaphores received
- [ ] 7.2 Handle fallback when import fails
- [ ] 7.3 Test reconnection scenario

## 8. Testing
- [ ] 8.1 Test basic sync flow with vkcube
- [ ] 8.2 Test back-pressure (slow Goggles)
- [ ] 8.3 Test reconnection