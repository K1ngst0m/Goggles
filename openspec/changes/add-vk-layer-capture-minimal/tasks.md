# Implementation Tasks

## 1. Vulkan Layer Infrastructure
- [x] 1.1 Create `layer_main.cpp` with `vkNegotiateLoaderLayerInterfaceVersion` entry point
- [x] 1.2 Implement `vkGetInstanceProcAddr` and `vkGetDeviceProcAddr` dispatch
- [x] 1.3 Create `vk_dispatch.hpp` with `vk_inst_funcs` and `vk_device_funcs` tables
- [x] 1.4 Implement object tracking maps using loader dispatch table pointer as key
- [x] 1.5 Create `config/goggles_layer.json` manifest file
- [x] 1.6 Update `src/capture/CMakeLists.txt` to build `libgoggles_vklayer.so`

## 2. Core Vulkan Hooks
- [x] 2.1 Hook `vkCreateInstance` - add required extensions, store instance data
- [x] 2.2 Hook `vkDestroyInstance` - cleanup instance data
- [x] 2.3 Hook `vkCreateDevice` - add required extensions, enumerate queues, store device data
- [x] 2.4 Hook `vkDestroyDevice` - cleanup device data and resources
- [x] 2.5 Hook `vkCreateSwapchainKHR` - add `TRANSFER_SRC_BIT`, capture extent/format/images
- [x] 2.6 Hook `vkDestroySwapchainKHR` - cleanup swapchain data
- [x] 2.7 Hook `vkQueuePresentKHR` - trigger capture flow

## 3. DMA-BUF Export Image
- [x] 3.1 Create `vk_capture.cpp/hpp` with export image creation logic
- [x] 3.2 Create VkImage with `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`
- [x] 3.3 Allocate exportable VkDeviceMemory
- [x] 3.4 Export memory as DMA-BUF fd via `vkGetMemoryFdKHR`
- [x] 3.5 Get image stride/offset via `vkGetImageSubresourceLayout`

## 4. GPU Frame Copy
- [x] 4.1 Create per-frame resources (command pool, command buffer, fence, semaphore)
- [x] 4.2 Implement `vk_capture()` function called from QueuePresentKHR
- [x] 4.3 Record `vkCmdCopyImage` from swapchain image to export image
- [x] 4.4 Handle image memory barriers for layout transitions
- [x] 4.5 Submit with semaphore wait/signal for synchronization with present

## 5. Unix Socket IPC
- [x] 5.1 Create `capture_protocol.hpp` with shared data structures
- [x] 5.2 Create `ipc_socket.cpp/hpp` for Unix domain socket handling
- [x] 5.3 Implement socket server in Goggles app (listens for layer connection)
- [x] 5.4 Implement socket client in layer (connects on first capture)
- [x] 5.5 Send DMA-BUF fd + metadata via `SCM_RIGHTS`
- [x] 5.6 Receive capture control messages (start/stop)

## 6. Goggles App Frame Display (Temporary)
- [x] 6.1 Add socket server to accept layer connection
- [x] 6.2 Receive DMA-BUF fd and metadata via `SCM_RIGHTS`
- [x] 6.3 mmap DMA-BUF fd for CPU access (with `DMA_BUF_IOCTL_SYNC`)
- [x] 6.4 Create SDL_Texture matching frame dimensions
- [x] 6.5 Use `SDL_UpdateTexture()` + `SDL_RenderCopy()` to display frame

## 7. Testing & Validation
- [x] 7.1 Create test script: `scripts/test_vkcube_capture.sh`
- [ ] 7.2 Verify layer loads without crashing vkcube
- [ ] 7.3 Verify DMA-BUF fd is received by Goggles app
- [ ] 7.4 Verify captured frames appear in Goggles window
- [x] 7.5 Document usage in `docs/vk_layer_usage.md`
