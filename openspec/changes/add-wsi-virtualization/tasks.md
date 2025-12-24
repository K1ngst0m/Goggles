## 1. Core Infrastructure

- [ ] 1.1 Create `wsi_virtual.hpp` with VirtualSurface/VirtualSwapchain structs
- [ ] 1.2 Create `wsi_virtual.cpp` with WsiVirtualizer implementation
- [ ] 1.3 Add `should_use_wsi_proxy()` config function

## 2. Dispatch Table Updates

- [ ] 2.1 Add surface function pointers to `VkInstFuncs`
- [ ] 2.2 Add swapchain function pointers to `VkDeviceFuncs`
- [ ] 2.3 Add `wsi_proxy_enabled` flag to `VkInstData`

## 3. Surface Hooks

- [ ] 3.1 Implement `Goggles_CreateXlibSurfaceKHR`
- [ ] 3.2 Implement `Goggles_CreateWaylandSurfaceKHR`
- [ ] 3.3 Implement `Goggles_CreateXcbSurfaceKHR`
- [ ] 3.4 Implement `Goggles_DestroySurfaceKHR`

## 4. Surface Query Hooks

- [ ] 4.1 Implement `Goggles_GetPhysicalDeviceSurfaceCapabilitiesKHR`
- [ ] 4.2 Implement `Goggles_GetPhysicalDeviceSurfaceFormatsKHR`
- [ ] 4.3 Implement `Goggles_GetPhysicalDeviceSurfacePresentModesKHR`
- [ ] 4.4 Implement `Goggles_GetPhysicalDeviceSurfaceSupportKHR`

## 5. Swapchain Hooks

- [ ] 5.1 Modify `Goggles_CreateSwapchainKHR` for virtual surfaces
- [ ] 5.2 Implement `Goggles_GetSwapchainImagesKHR`
- [ ] 5.3 Implement `Goggles_AcquireNextImageKHR`

## 6. Hook Registration

- [ ] 6.1 Register surface hooks in `Goggles_GetInstanceProcAddr`
- [ ] 6.2 Register swapchain hooks in `Goggles_GetDeviceProcAddr`

## 7. Build Configuration

- [ ] 7.1 Add platform defines to CMakeLists.txt
- [ ] 7.2 Add new source files to CMakeLists.txt

## 8. Testing

- [ ] 8.1 Test with vkcube: `GOGGLES_CAPTURE=1 GOGGLES_WSI_PROXY=1 vkcube`
- [ ] 8.2 Verify no window appears for target app
- [ ] 8.3 Verify Goggles displays captured frames