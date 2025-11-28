# Change: Minimal Vulkan Layer Capture for vkcube

## Why
The Goggles project requires a Vulkan layer injection mechanism to capture frames from target applications. We need a minimal working implementation that validates the core architecture by capturing vkcube and displaying it in the Goggles window. This aligns with Prototype 1-3 goals from the design snapshot.

## Reference Implementation
Based on analysis of `obs-vkcapture` - a production-quality Vulkan capture layer used by OBS Studio.

## What Changes
- Implement Vulkan layer shared object (`libgoggles_vklayer.so`) with essential hooks
- Hook `vkCreateInstance`, `vkCreateDevice`, `vkCreateSwapchainKHR`, and `vkQueuePresentKHR`
- Use **DMA-BUF export** for zero-copy GPU-to-GPU frame sharing (not CPU shared memory)
- GPU copy from swapchain image to exportable image via `vkCmdCopyImage`
- Add Unix domain socket IPC to pass DMA-BUF file descriptors to Goggles app
- Modify Goggles app to mmap DMA-BUF and render via SDL (temporary, for layer validation)
- Add layer manifest JSON for Vulkan loader discovery

## Impact
- Affected specs: Creates new `vk-layer-capture` capability
- Affected code:
  - `src/capture/vk_layer/` - New layer implementation
  - `src/capture/ipc/` - Unix socket + DMA-BUF transfer
  - `src/capture/CMakeLists.txt` - Build layer as shared object
  - `src/app/main.cpp` - Add DMA-BUF mmap and SDL rendering (temporary)
  - `cmake/` - Vulkan SDK detection and layer linking
