#pragma once

#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

namespace goggles::capture {

// =============================================================================
// Instance Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                           const VkAllocationCallbacks* pAllocator,
                                           VkInstance* pInstance);

void VKAPI_CALL Goggles_DestroyInstance(VkInstance instance,
                                        const VkAllocationCallbacks* pAllocator);

VkResult VKAPI_CALL Goggles_EnumeratePhysicalDevices(VkInstance instance,
                                                     uint32_t* pPhysicalDeviceCount,
                                                     VkPhysicalDevice* pPhysicalDevices);

// =============================================================================
// Device Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateDevice(VkPhysicalDevice physicalDevice,
                                         const VkDeviceCreateInfo* pCreateInfo,
                                         const VkAllocationCallbacks* pAllocator,
                                         VkDevice* pDevice);

void VKAPI_CALL Goggles_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);

// =============================================================================
// Surface Hooks (WSI proxy)
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateXlibSurfaceKHR(VkInstance instance,
                                                 const VkXlibSurfaceCreateInfoKHR* pCreateInfo,
                                                 const VkAllocationCallbacks* pAllocator,
                                                 VkSurfaceKHR* pSurface);

VkResult VKAPI_CALL Goggles_CreateXcbSurfaceKHR(VkInstance instance,
                                                const VkXcbSurfaceCreateInfoKHR* pCreateInfo,
                                                const VkAllocationCallbacks* pAllocator,
                                                VkSurfaceKHR* pSurface);

VkResult VKAPI_CALL Goggles_CreateWaylandSurfaceKHR(
    VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

void VKAPI_CALL Goggles_DestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                          const VkAllocationCallbacks* pAllocator);

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pCapabilities);

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                                               VkSurfaceKHR surface,
                                                               uint32_t* pSurfaceFormatCount,
                                                               VkSurfaceFormatKHR* pSurfaceFormats);

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount,
    VkPresentModeKHR* pPresentModes);

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                                               uint32_t queueFamilyIndex,
                                                               VkSurfaceKHR surface,
                                                               VkBool32* pSupported);

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    VkSurfaceCapabilities2KHR* pSurfaceCapabilities);

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats);

// =============================================================================
// Swapchain Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateSwapchainKHR(VkDevice device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkSwapchainKHR* pSwapchain);

void VKAPI_CALL Goggles_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                            const VkAllocationCallbacks* pAllocator);

VkResult VKAPI_CALL Goggles_GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                  uint32_t* pSwapchainImageCount,
                                                  VkImage* pSwapchainImages);

VkResult VKAPI_CALL Goggles_AcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                uint64_t timeout, VkSemaphore semaphore,
                                                VkFence fence, uint32_t* pImageIndex);

// =============================================================================
// Present Hook
// =============================================================================

VkResult VKAPI_CALL Goggles_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

} // namespace goggles::capture
