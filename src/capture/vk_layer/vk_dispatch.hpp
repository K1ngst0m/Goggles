#pragma once

#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace goggles::capture {

/// @brief Extracts the loader dispatch table pointer from a dispatchable Vulkan handle.
// First pointer in dispatchable Vulkan objects is the loader dispatch table
#define GET_LDT(x) (*(void**)(x))

/// @brief Instance-level Vulkan entry points used by the layer.
struct VkInstFuncs {
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2;
    PFN_vkGetPhysicalDeviceFormatProperties2 GetPhysicalDeviceFormatProperties2;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 GetPhysicalDeviceImageFormatProperties2;

    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR GetPhysicalDeviceSurfaceCapabilities2KHR;
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR GetPhysicalDeviceSurfaceFormats2KHR;
};

/// @brief Device-level Vulkan entry points used by the layer.
struct VkDeviceFuncs {
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkDestroyDevice DestroyDevice;

    // Swapchain
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;

    // Memory
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;

    // External memory (DMA-BUF)
    PFN_vkGetMemoryFdKHR GetMemoryFdKHR;
    PFN_vkGetImageDrmFormatModifierPropertiesEXT GetImageDrmFormatModifierPropertiesEXT;

    // External semaphore
    PFN_vkGetSemaphoreFdKHR GetSemaphoreFdKHR;

    // Images
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;

    // Commands
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdBlitImage CmdBlitImage;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;

    // Queues
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkQueueSubmit QueueSubmit;

    // Synchronization
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkResetFences ResetFences;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkWaitSemaphoresKHR WaitSemaphoresKHR;
};

/// @brief Tracked instance state and dispatch table.
struct VkInstData {
    VkInstance instance;
    VkInstFuncs funcs;
    bool valid;
};

/// @brief Tracked device state and dispatch table.
struct VkDeviceData {
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkDeviceFuncs funcs;
    VkInstData* inst_data;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    bool valid;
};

/// @brief Tracks instances/devices/queues to look up dispatch tables during hooking.
class ObjectTracker {
public:
    /// @brief Adds instance tracking data.
    void add_instance(VkInstance instance, VkInstData data);
    /// @brief Returns instance tracking data, or null.
    VkInstData* get_instance(VkInstance instance);
    /// @brief Returns instance tracking data for a physical device, or null.
    VkInstData* get_instance_by_physical_device(VkPhysicalDevice device);
    /// @brief Removes instance tracking data.
    void remove_instance(VkInstance instance);

    /// @brief Adds device tracking data.
    void add_device(VkDevice device, VkDeviceData data);
    /// @brief Returns device tracking data, or null.
    VkDeviceData* get_device(VkDevice device);
    /// @brief Returns device tracking data for a queue, or null.
    VkDeviceData* get_device_by_queue(VkQueue queue);
    /// @brief Removes device tracking data.
    void remove_device(VkDevice device);

    /// @brief Records a queue-to-device association.
    void add_queue(VkQueue queue, VkDevice device);
    /// @brief Removes all queues associated with a device.
    void remove_queues_for_device(VkDevice device);

    /// @brief Records a physical-device-to-instance association.
    void add_physical_device(VkPhysicalDevice phys, VkInstance inst);

private:
    std::mutex mutex_;
    std::unordered_map<void*, VkInstData> instances_;
    std::unordered_map<void*, VkDeviceData> devices_;
    std::unordered_map<void*, VkDevice> queue_to_device_;
    std::unordered_map<void*, VkInstance> phys_to_instance_;
};

/// @brief Returns the process-wide object tracker instance.
ObjectTracker& get_object_tracker();

} // namespace goggles::capture
