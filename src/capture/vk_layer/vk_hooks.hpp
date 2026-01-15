#pragma once

#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

namespace goggles::capture {

// =============================================================================
// Instance Hooks
// =============================================================================

/// @brief Layer entry point for `vkCreateInstance`.
VkResult VKAPI_CALL Goggles_CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                           const VkAllocationCallbacks* pAllocator,
                                           VkInstance* pInstance);

/// @brief Layer entry point for `vkDestroyInstance`.
void VKAPI_CALL Goggles_DestroyInstance(VkInstance instance,
                                        const VkAllocationCallbacks* pAllocator);

/// @brief Layer entry point for `vkEnumeratePhysicalDevices`.
VkResult VKAPI_CALL Goggles_EnumeratePhysicalDevices(VkInstance instance,
                                                     uint32_t* pPhysicalDeviceCount,
                                                     VkPhysicalDevice* pPhysicalDevices);

// =============================================================================
// Device Hooks
// =============================================================================

/// @brief Layer entry point for `vkCreateDevice`.
VkResult VKAPI_CALL Goggles_CreateDevice(VkPhysicalDevice physicalDevice,
                                         const VkDeviceCreateInfo* pCreateInfo,
                                         const VkAllocationCallbacks* pAllocator,
                                         VkDevice* pDevice);

/// @brief Layer entry point for `vkDestroyDevice`.
void VKAPI_CALL Goggles_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);

// =============================================================================
// Surface Hooks (WSI proxy)
// =============================================================================

/// @brief Layer entry point for `vkCreateXlibSurfaceKHR`.
VkResult VKAPI_CALL Goggles_CreateXlibSurfaceKHR(VkInstance instance,
                                                 const VkXlibSurfaceCreateInfoKHR* pCreateInfo,
                                                 const VkAllocationCallbacks* pAllocator,
                                                 VkSurfaceKHR* pSurface);

/// @brief Layer entry point for `vkCreateXcbSurfaceKHR`.
VkResult VKAPI_CALL Goggles_CreateXcbSurfaceKHR(VkInstance instance,
                                                const VkXcbSurfaceCreateInfoKHR* pCreateInfo,
                                                const VkAllocationCallbacks* pAllocator,
                                                VkSurfaceKHR* pSurface);

/// @brief Layer entry point for `vkCreateWaylandSurfaceKHR`.
VkResult VKAPI_CALL Goggles_CreateWaylandSurfaceKHR(
    VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

/// @brief Layer entry point for `vkDestroySurfaceKHR`.
void VKAPI_CALL Goggles_DestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                          const VkAllocationCallbacks* pAllocator);

/// @brief Layer entry point for `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`.
VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pCapabilities);

/// @brief Layer entry point for `vkGetPhysicalDeviceSurfaceFormatsKHR`.
VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                                               VkSurfaceKHR surface,
                                                               uint32_t* pSurfaceFormatCount,
                                                               VkSurfaceFormatKHR* pSurfaceFormats);

/// @brief Layer entry point for `vkGetPhysicalDeviceSurfacePresentModesKHR`.
VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount,
    VkPresentModeKHR* pPresentModes);

/// @brief Layer entry point for `vkGetPhysicalDeviceSurfaceSupportKHR`.
VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                                               uint32_t queueFamilyIndex,
                                                               VkSurfaceKHR surface,
                                                               VkBool32* pSupported);

/// @brief Layer entry point for `vkGetPhysicalDeviceSurfaceCapabilities2KHR`.
VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    VkSurfaceCapabilities2KHR* pSurfaceCapabilities);

/// @brief Layer entry point for `vkGetPhysicalDeviceSurfaceFormats2KHR`.
VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats);

// =============================================================================
// Swapchain Hooks
// =============================================================================

/// @brief Layer entry point for `vkCreateSwapchainKHR`.
VkResult VKAPI_CALL Goggles_CreateSwapchainKHR(VkDevice device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkSwapchainKHR* pSwapchain);

/// @brief Layer entry point for `vkDestroySwapchainKHR`.
void VKAPI_CALL Goggles_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                            const VkAllocationCallbacks* pAllocator);

/// @brief Layer entry point for `vkGetSwapchainImagesKHR`.
VkResult VKAPI_CALL Goggles_GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                  uint32_t* pSwapchainImageCount,
                                                  VkImage* pSwapchainImages);

/// @brief Layer entry point for `vkAcquireNextImageKHR`.
VkResult VKAPI_CALL Goggles_AcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                uint64_t timeout, VkSemaphore semaphore,
                                                VkFence fence, uint32_t* pImageIndex);

// =============================================================================
// Present Hook
// =============================================================================

/// @brief Layer entry point for `vkQueuePresentKHR` (no logging; hot path).
VkResult VKAPI_CALL Goggles_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

} // namespace goggles::capture
