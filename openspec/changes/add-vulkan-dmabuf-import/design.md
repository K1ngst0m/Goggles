# Design: Vulkan DMA-BUF Import

## Context

The capture layer exports frames as DMA-BUFs with LINEAR tiling. The app currently uses CPU mmap to read this data, which is inefficient. We need zero-copy GPU import.

**Constraints:**
- Must use SDL3 for window/surface creation (already in use)
- Must use vulkan-hpp (C++ bindings), NOT raw Vulkan C API (per project policy D.2)
- Must follow project error handling (`tl::expected`)
- Must follow naming conventions (snake_case, m_ prefix)
- Single-threaded render loop on main thread

## Goals / Non-Goals

**Goals:**
- Import DMA-BUF fd as VkImage without CPU copy
- Render to SDL window via Vulkan swapchain
- Minimal implementation - no shader pipeline yet

**Non-Goals:**
- Multi-pass shader pipeline (future work)
- Multi-GPU support
- HDR output support
- Window resize handling (basic clear, no proper recreation)

## Decisions

### Decision: Use vulkan-hpp with NO_EXCEPTIONS and dynamic dispatch
**Rationale:** Per project policy D.2, application code must use vulkan-hpp. Dynamic dispatch required for extension functions (DMA-BUF import, external memory).

**Configuration (per policy D.2):**
```cpp
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
// Plus: VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE in one cpp file
```

**RAII usage (per policy D.2 guidelines):**
- `vk::Unique*` for: Instance, Device, Surface, Swapchain (long-lived, destroyed at shutdown)
- Plain `vk::` handles for: CommandBuffers (from pool), Fences/Semaphores (reused per-frame), imported images (explicit sync needed)

**Error handling:**
- vulkan-hpp returns `vk::ResultValue<T>` (no exceptions)
- Convert to `tl::expected` at VulkanBackend public API boundaries
- Check `.result` field before using `.value`

### Decision: Use commandBuffer.blitImage for rendering
**Rationale:** Simple, handles format conversion automatically, no shaders needed for this minimal version.

**Alternatives considered:**
- Full render pass with fragment shader - More complex, not needed yet
- commandBuffer.copyImage - Requires exact format match

### Decision: Single VulkanBackend class
**Rationale:** Keep implementation simple. One class manages instance, device, swapchain, and rendering.

**Alternatives considered:**
- Separate classes for each concern - Adds complexity for minimal implementation

### Decision: Re-import DMA-BUF fd each frame (initially)
**Rationale:** Simpler implementation. The layer sends the same fd repeatedly for unchanged textures. We can optimize later by caching based on fd number.

**Future optimization:** Track fd and only reimport when dimensions change.

## Architecture

```
CaptureReceiver (socket, metadata)
        │
        ▼
  VulkanBackend (vulkan-hpp, NO_EXCEPTIONS, static dispatch)
        │
        ├── vk::UniqueInstance (SDL extensions)
        ├── vk::UniqueSurfaceKHR (from SDL_Vulkan_CreateSurface)
        ├── vk::UniqueDevice (external memory extensions)
        ├── vk::UniqueSwapchainKHR
        ├── vk::UniqueCommandPool
        ├── vk::CommandBuffer (plain, from pool, reused)
        ├── vk::Fence, vk::Semaphore (plain, reused per-frame)
        │
        └── Per-frame:
            ├── Import DMA-BUF → vk::Image + vk::DeviceMemory (plain, explicit cleanup)
            ├── commandBuffer.blitImage (imported → swapchain)
            └── queue.presentKHR
```

## File Structure

```
src/render/vulkan/
├── vulkan_backend.hpp    # VulkanBackend class declaration
├── vulkan_backend.cpp    # Implementation
└── vulkan_types.hpp      # RAII wrappers (if needed)

src/app/
├── capture_receiver.hpp  # Modified: expose fd/metadata, no mmap
├── capture_receiver.cpp  # Modified: remove mmap logic
└── main.cpp             # Modified: use VulkanBackend instead of SDL_Renderer
```

## Risks / Trade-offs

**Risk:** Driver compatibility for DMA-BUF import
**Mitigation:** Test on multiple drivers (NVIDIA, AMD, Intel). Fall back to mmap if import fails.

**Risk:** Format mismatch between exported and import capabilities
**Mitigation:** Query supported formats via `vkGetPhysicalDeviceImageFormatProperties2`. The layer already uses common formats (B8G8R8A8, A2B10G10R10).

**Trade-off:** Simplicity vs Performance
**Choice:** Initial implementation prioritizes simplicity. No caching, no async import, no fancy sync.

## Open Questions

1. Should we support fallback to mmap if Vulkan import fails?
   - **Tentative:** No, require Vulkan support for now.

2. Should imported image be cached across frames when fd is unchanged?
   - **Tentative:** Defer optimization, import each frame for simplicity.
