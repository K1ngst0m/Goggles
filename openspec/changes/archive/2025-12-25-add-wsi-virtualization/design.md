## Context

The Goggles Vulkan layer intercepts frame presentation to capture and share frames via DMA-BUF. Currently, target applications still create their own windows and display independently. WSI virtualization intercepts all window system integration calls to create a fully virtual display path.

## Goals / Non-Goals

**Goals:**
- Virtual surface creation (no real window)
- Virtual swapchain with DMA-BUF exportable images
- Compatible with existing capture pipeline
- Linux platform support (X11, Wayland, XCB)

**Non-Goals:**
- Input forwarding (separate proposal)
- Windows/macOS support
- VR/XR surface virtualization

## Decisions

**Decision: Use synthetic handles for virtual objects**

Virtual surfaces and swapchains use incrementing uint64 addresses cast to Vulkan handles. This avoids conflicts with real driver handles.

```cpp
uint64_t next_handle_ = 0x1000;
VkSurfaceKHR handle = reinterpret_cast<VkSurfaceKHR>(next_handle_++);
```

**Decision: Reuse existing DMA-BUF export logic**

Virtual swapchain images are created the same way as the current export image in `vk_capture.cpp`. The `VkExternalMemoryImageCreateInfo` chain with DMA-BUF handle type is already proven.

**Decision: Fixed virtual capabilities**

Return sensible defaults for surface queries:
- Image count: 2-3
- Formats: B8G8R8A8_SRGB, B8G8R8A8_UNORM
- Present modes: FIFO, IMMEDIATE
- Extent: From environment variable or default 1920x1080

## Risks / Trade-offs

**Risk:** Applications may query unsupported features
- Mitigation: Return comprehensive capability sets covering common use cases

**Risk:** Validation layers may flag virtual handles
- Mitigation: Document that validation should be disabled in WSI proxy mode

**Trade-off:** Virtual swapchain images require DMA-BUF export capability
- All modern Linux drivers support this; older drivers will fail gracefully

## Architecture

```
vkCreate*SurfaceKHR
    └─ [WSI_PROXY mode] → WsiVirtualizer::create_surface()
                              └─ Returns synthetic VkSurfaceKHR

vkGetPhysicalDeviceSurface*KHR
    └─ [virtual surface] → Return fixed capabilities

vkCreateSwapchainKHR
    └─ [virtual surface] → WsiVirtualizer::create_swapchain()
                              ├─ Create N VkImage with DMA-BUF export
                              ├─ Export each as DMA-BUF fd
                              └─ Return synthetic VkSwapchainKHR

vkAcquireNextImageKHR
    └─ [virtual swapchain] → Return next index (round-robin)

vkQueuePresentKHR
    └─ [virtual swapchain] → Send DMA-BUF fd to Goggles app
```

## Open Questions

None - design is straightforward extension of existing capture mechanism.