#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace goggles::capture {

// First pointer in dispatchable Vulkan objects is the loader dispatch table
#define GET_LDT(x) (*(void**)(x))

struct VkInstFuncs {
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
};

struct VkDeviceFuncs {
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkDestroyDevice DestroyDevice;

    // Swapchain
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;

    // Memory
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;

    // External memory (DMA-BUF)
    PFN_vkGetMemoryFdKHR GetMemoryFdKHR;

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
};

struct VkInstData {
    VkInstance instance;
    VkInstFuncs funcs;
    bool valid;
};

struct VkDeviceData {
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkDeviceFuncs funcs;
    VkInstData* inst_data;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    bool valid;
};

class ObjectTracker {
public:
    void add_instance(VkInstance instance, VkInstData data);
    VkInstData* get_instance(VkInstance instance);
    VkInstData* get_instance_by_physical_device(VkPhysicalDevice device);
    void remove_instance(VkInstance instance);

    void add_device(VkDevice device, VkDeviceData data);
    VkDeviceData* get_device(VkDevice device);
    VkDeviceData* get_device_by_queue(VkQueue queue);
    void remove_device(VkDevice device);

    void add_queue(VkQueue queue, VkDevice device);
    void remove_queues_for_device(VkDevice device);

    void add_physical_device(VkPhysicalDevice phys, VkInstance inst);

private:
    std::mutex mutex_;
    std::unordered_map<void*, VkInstData> instances_;
    std::unordered_map<void*, VkDeviceData> devices_;
    std::unordered_map<void*, VkDevice> queue_to_device_;
    std::unordered_map<void*, VkInstance> phys_to_instance_;
};

ObjectTracker& get_object_tracker();

} // namespace goggles::capture
