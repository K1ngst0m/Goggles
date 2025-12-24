## 1. Core Infrastructure

- [x] 1.1 Create `wsi_virtual.hpp` with VirtualSurface/VirtualSwapchain structs
- [x] 1.2 Create `wsi_virtual.cpp` with WsiVirtualizer implementation
- [x] 1.3 Add `should_use_wsi_proxy()` config function

## 2. Dispatch Table Updates

- [x] 2.1 Add surface function pointers to `VkInstFuncs`
- [x] 2.2 Add swapchain function pointers to `VkDeviceFuncs`

## 3. Surface Hooks

- [x] 3.1 Implement `Goggles_CreateXlibSurfaceKHR`
- [x] 3.2 Implement `Goggles_CreateWaylandSurfaceKHR`
- [x] 3.3 Implement `Goggles_CreateXcbSurfaceKHR`
- [x] 3.4 Implement `Goggles_DestroySurfaceKHR`

## 4. Surface Query Hooks

- [x] 4.1 Implement `Goggles_GetPhysicalDeviceSurfaceCapabilitiesKHR`
- [x] 4.2 Implement `Goggles_GetPhysicalDeviceSurfaceFormatsKHR`
- [x] 4.3 Implement `Goggles_GetPhysicalDeviceSurfacePresentModesKHR`
- [x] 4.4 Implement `Goggles_GetPhysicalDeviceSurfaceSupportKHR`

## 5. Swapchain Hooks

- [x] 5.1 Modify `Goggles_CreateSwapchainKHR` for virtual surfaces
- [x] 5.2 Implement `Goggles_GetSwapchainImagesKHR`
- [x] 5.3 Implement `Goggles_AcquireNextImageKHR`

## 6. Hook Registration

- [x] 6.1 Register surface hooks in `Goggles_GetInstanceProcAddr`
- [x] 6.2 Register swapchain hooks in `Goggles_GetDeviceProcAddr`

## 7. Build Configuration

- [x] 7.1 Add platform defines to headers
- [x] 7.2 Add new source files to CMakeLists.txt

## 8. Testing

- [x] 8.1 Test with vkcube: `GOGGLES_CAPTURE=1 GOGGLES_WSI_PROXY=1 vkcube`
- [x] 8.2 Verify no window appears for target app

## 9. Frame Rate Limiting

- [ ] 9.1 Add `GOGGLES_FPS_LIMIT` config (default 60)
- [ ] 9.2 Implement rate limiting in `acquire_next_image`