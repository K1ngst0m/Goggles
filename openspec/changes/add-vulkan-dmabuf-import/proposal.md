# Change: Add Vulkan DMA-BUF Import for Zero-Copy Frame Display

## Why

The current app-side implementation uses CPU mmap to read DMA-BUF data and performs CPU-side pixel format conversion before uploading to SDL texture. This is inefficient and introduces unnecessary latency. By importing the DMA-BUF directly into Vulkan as a VkImage, we achieve true zero-copy frame display on the GPU.

## What Changes

- **MODIFIED**: Replace temporary mmap-based frame display with Vulkan DMA-BUF import
- Add minimal Vulkan initialization (instance, device, swapchain) using SDL3 surface
- Import received DMA-BUF fd as VkImage using `VK_EXT_external_memory_dma_buf`
- Render imported image to swapchain using simple fullscreen blit
- Remove CPU mmap and pixel format conversion code from CaptureReceiver

## Impact

- Affected specs: `vk-layer-capture` (modifies temporary frame display requirement)
- Affected code:
  - `src/app/main.cpp` - Replace SDL_Renderer with Vulkan rendering
  - `src/app/capture_receiver.cpp/hpp` - Remove mmap, add Vulkan image import
  - `src/render/vulkan/` - Add minimal Vulkan backend implementation

## Scope (Minimal Implementation)

This is a **minimal viable implementation** focused on:
1. Vulkan instance/device/swapchain setup via SDL3 surface
2. DMA-BUF fd import as VkImage using external memory extensions
3. Simple fullscreen blit from imported image to swapchain
4. No shader pipeline, no multi-pass, no fancy synchronization (single queue)

## Technical Approach

### API Requirements (per policy D.2)
- **Must use vulkan-hpp** (C++ bindings) with `VULKAN_HPP_NO_EXCEPTIONS` and dynamic dispatch
- Dynamic dispatch required for extension functions (DMA-BUF, external memory)
- Use `vk::Unique*` handles for long-lived resources (Instance, Device, Surface, Swapchain)
- Use plain `vk::` handles for per-frame and GPU-async resources (CommandBuffers, Fences, imported images)
- Convert `vk::ResultValue<T>` to `nonstd::expected` at public API boundaries

### Required Vulkan Extensions
- Instance: `VK_KHR_external_memory_capabilities`, `VK_KHR_get_physical_device_properties2`
- Device: `VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`, `VK_KHR_swapchain`

### Import Flow
1. Receive DMA-BUF fd and metadata (width, height, format, stride) via socket
2. Create `vk::Image` (plain handle) with matching dimensions and format, using `vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT`
3. Import fd via `device.getMemoryFdPropertiesKHR` and `device.allocateMemory` with `vk::ImportMemoryFdInfoKHR`
4. Bind memory to image
5. On reimport: wait for GPU idle, explicitly destroy old image/memory, then create new

### Render Flow  
1. Acquire swapchain image via `device.acquireNextImageKHR`
2. Transition imported image to `vk::ImageLayout::eTransferSrcOptimal`
3. Transition swapchain image to `vk::ImageLayout::eTransferDstOptimal`
4. `commandBuffer.blitImage` from imported to swapchain (handles format conversion)
5. Transition swapchain image to `vk::ImageLayout::ePresentSrcKHR`
6. Queue submit and present
