# Tasks: Add Vulkan DMA-BUF Import

## 0. Setup
- [x] 0.1 Create vulkan header with VULKAN_HPP_NO_EXCEPTIONS and dynamic dispatch defines

## 1. Vulkan Backend Setup (vk::Unique* for long-lived resources)
- [x] 1.1 Add vk::UniqueInstance creation with required extensions (external memory capabilities)
- [x] 1.2 Add vk::UniqueSurfaceKHR via SDL_Vulkan_CreateSurface
- [x] 1.3 Add physical device selection with DMA-BUF import support check
- [x] 1.4 Add vk::UniqueDevice creation with required extensions (external_memory_fd, dma_buf, swapchain)
- [x] 1.5 Add vk::UniqueSwapchainKHR creation matching window size

## 2. Swapchain Infrastructure
- [x] 2.1 Add swapchain image views (vk::UniqueImageView)
- [x] 2.2 Add vk::UniqueCommandPool and vk::CommandBuffer (plain, from pool, reused)
- [x] 2.3 Add sync primitives as plain handles (vk::Fence, vk::Semaphore - reused per frame)

## 3. DMA-BUF Import (plain handles - explicit sync required)
- [x] 3.1 Add vk::Image creation (plain) with external memory handle type for DMA-BUF
- [x] 3.2 Add device.getMemoryFdPropertiesKHR to query memory requirements
- [x] 3.3 Add vk::DeviceMemory allocation (plain) with vk::ImportMemoryFdInfoKHR
- [x] 3.4 Add image-memory binding
- [x] 3.5 Add vk::ImageView creation for imported image
- [x] 3.6 Add explicit cleanup with GPU sync before reimport

## 4. Frame Rendering
- [x] 4.1 Add frame rendering loop with device.acquireNextImageKHR
- [x] 4.2 Add command buffer recording with image transitions and blitImage
- [x] 4.3 Add queue.submit and queue.presentKHR

## 5. Integration
- [x] 5.1 Modify CaptureReceiver to expose raw fd/metadata without mmap
- [x] 5.2 Integrate VulkanBackend into main.cpp replacing SDL_Renderer
- [x] 5.3 Remove CPU mmap and format conversion code

## 6. Resource Cleanup
- [x] 6.1 Ensure vk::Unique* members ordered for correct destruction (device before instance)
- [x] 6.2 Add device.waitIdle() before destroying imported image resources
- [ ] 6.3 Handle swapchain recreation on window resize (deferred - basic clear only)
