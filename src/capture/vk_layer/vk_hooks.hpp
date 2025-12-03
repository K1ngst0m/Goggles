#pragma once

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

// =============================================================================
// Device Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateDevice(VkPhysicalDevice physicalDevice,
                                         const VkDeviceCreateInfo* pCreateInfo,
                                         const VkAllocationCallbacks* pAllocator,
                                         VkDevice* pDevice);

void VKAPI_CALL Goggles_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);

// =============================================================================
// Swapchain Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateSwapchainKHR(VkDevice device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkSwapchainKHR* pSwapchain);

void VKAPI_CALL Goggles_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                            const VkAllocationCallbacks* pAllocator);

// =============================================================================
// Present Hook
// =============================================================================

VkResult VKAPI_CALL Goggles_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

} // namespace goggles::capture
