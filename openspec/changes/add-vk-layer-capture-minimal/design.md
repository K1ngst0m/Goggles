# Design: Minimal Vulkan Layer Capture

## Context
This is the foundational capture mechanism for Goggles. We're implementing the minimum viable Vulkan layer that can intercept frames from a target application (vkcube) and display them in the Goggles window.

**Reference:** Based on `obs-vkcapture` architecture - a production Vulkan capture layer for OBS Studio.

## Goals / Non-Goals
- **Goals:**
  - Validate Vulkan layer hooking works reliably on Linux
  - Prove DMA-BUF export from layer works correctly
  - Display captured frames in Goggles window with visible output
  - Keep implementation minimal while using production-proven patterns

- **Non-Goals:**
  - DRM format modifier support (use LINEAR tiling initially)
  - Multi-pass shader pipeline (Prototype 5)
  - Wayland/X11 compositor capture fallback
  - Hot-reloading or dynamic configuration
  - Format conversion (assume matching formats)
  - App-side zero-copy rendering (temporary mmap approach for layer validation)

## Decisions

### Layer Architecture (from obs-vkcapture)
- **Decision:** Single shared object with dispatch table keyed by loader dispatch pointer
- **Rationale:** Proven pattern from obs-vkcapture; handles multiple instances/devices correctly
- **Pattern:** Use `GET_LDT(x) = *(void**)x` to get loader dispatch table as map key

### Frame Sharing Method: DMA-BUF Export
- **Decision:** Create exportable VkImage with `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`, GPU copy from swapchain
- **Rationale:** Production-proven pattern from obs-vkcapture; enables future zero-copy optimization
- **Flow:**
  1. Create export image with external memory + LINEAR tiling
  2. GPU copy: `vkCmdCopyImage(swapchain_image → export_image)`
  3. Export memory as DMA-BUF fd via `vkGetMemoryFdKHR`
  4. Pass fd to Goggles app via Unix socket
  5. **Temporary:** App mmaps DMA-BUF for CPU access, renders via SDL
  6. **Future:** App imports as Vulkan/EGL texture for zero-copy

### IPC Mechanism: Unix Domain Socket + SCM_RIGHTS
- **Decision:** Abstract Unix socket to pass texture metadata + DMA-BUF file descriptors
- **Rationale:** Standard Linux pattern for passing fds between processes; used by obs-vkcapture
- **Protocol:**
  - Layer → App: `capture_texture_data` struct + DMA-BUF fd via `SCM_RIGHTS`
  - App → Layer: `capture_control_data` struct (start/stop capture)

### Vulkan Extensions Required
- **Instance:** `VK_KHR_EXTERNAL_MEMORY_CAPABILITIES`
- **Device:** `VK_KHR_EXTERNAL_MEMORY_FD`, `VK_EXT_EXTERNAL_MEMORY_DMA_BUF`, `VK_KHR_BIND_MEMORY_2`

### Synchronization (from obs-vkcapture)
- **Decision:** Per-frame resources with fence + semaphore coordination
- **Resources per frame:** command pool, command buffer, fence, semaphore
- **Flow:**
  1. Wait on present semaphores before copy command
  2. Submit copy command with own semaphore
  3. Pass own semaphore to actual present call
  4. Use fence to track command buffer completion before reuse

### Swapchain Hook
- **Decision:** Add `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` to swapchain creation
- **Rationale:** Required to copy from swapchain images; fall back to original flags if fails

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Layer crashes target app | Minimal hooking, fall back on extension failures |
| DMA-BUF import fails | Validate extension support; log detailed errors |
| LINEAR tiling performance | Acceptable for prototype; add modifier support later |
| Multi-GPU mismatch | Check device UUID; warn if different GPUs |
| Socket connection race | Retry with backoff; app polls for layer connection |

## File Structure

```
src/capture/vk_layer/
├── layer_main.cpp       # vkNegotiateLoaderLayerInterfaceVersion, dispatch
├── vk_dispatch.hpp      # Dispatch tables (vk_inst_funcs, vk_device_funcs)
├── vk_hooks.cpp         # Hook implementations (CreateInstance, QueuePresent, etc.)
├── vk_hooks.hpp         # Hook declarations
├── vk_capture.cpp       # Export image creation, GPU copy logic
├── vk_capture.hpp       # Capture state structures
├── ipc_socket.cpp       # Unix socket, fd passing
├── ipc_socket.hpp       # IPC interface

src/capture/
├── capture_protocol.hpp # Shared structs (capture_texture_data, etc.)

config/
└── goggles_layer.json   # Vulkan layer manifest

src/app/
└── main.cpp             # DMA-BUF mmap, SDL rendering (temporary)
```

## Key Data Structures (adapted from obs-vkcapture)

```cpp
struct SwapData {
    VkExtent2D extent;
    VkFormat format;
    VkImage export_image;       // Exportable image for DMA-BUF
    VkDeviceMemory export_mem;  // Backing memory
    VkImage* swap_images;       // Swapchain images
    uint32_t image_count;
    int dmabuf_fd;              // Exported file descriptor
    int dmabuf_stride;
    int dmabuf_offset;
};

struct FrameData {
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;
    VkFence fence;
    VkSemaphore semaphore;
    bool cmd_buffer_busy;
};
```

## Open Questions
- Should Goggles app use Vulkan or EGL for DMA-BUF import? (Vulkan preferred for consistency)
- Environment variable setup? (`VK_LAYER_PATH`, `VK_INSTANCE_LAYERS=VK_LAYER_goggles_capture`)
- What socket path? (Abstract socket `/goggles/vkcapture`)
