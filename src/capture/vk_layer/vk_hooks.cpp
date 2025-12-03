#include "vk_hooks.hpp"

#include "vk_capture.hpp"
#include "vk_dispatch.hpp"

#include <cstdio>
#include <cstring>
#include <vector>
#include <vulkan/vk_layer.h>

#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

static inline bool is_instance_link_info(VkLayerInstanceCreateInfo* info) {
    return info->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
           info->function == VK_LAYER_LINK_INFO;
}

static inline bool is_device_link_info(VkLayerDeviceCreateInfo* info) {
    return info->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
           info->function == VK_LAYER_LINK_INFO;
}

// =============================================================================
// Instance Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                           const VkAllocationCallbacks* pAllocator,
                                           VkInstance* pInstance) {

    auto* link_info =
        reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(pCreateInfo->pNext));
    while (link_info && !is_instance_link_info(link_info)) {
        link_info =
            reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(link_info->pNext));
    }

    if (!link_info) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;

    std::vector<const char*> extensions(pCreateInfo->ppEnabledExtensionNames,
                                        pCreateInfo->ppEnabledExtensionNames +
                                            pCreateInfo->enabledExtensionCount);

    bool has_ext_mem_caps = false;
    for (const auto* ext : extensions) {
        if (strcmp(ext, VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME) == 0) {
            has_ext_mem_caps = true;
            break;
        }
    }
    if (!has_ext_mem_caps) {
        extensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    }

    VkInstanceCreateInfo modified_info = *pCreateInfo;
    modified_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    modified_info.ppEnabledExtensionNames = extensions.data();

    auto create_func = reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr, "vkCreateInstance"));
    VkResult result = create_func(&modified_info, pAllocator, pInstance);

    if (result != VK_SUCCESS) {
        result = create_func(pCreateInfo, pAllocator, pInstance);
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    VkInstData inst_data{};
    inst_data.instance = *pInstance;
    inst_data.valid = true;

    auto& funcs = inst_data.funcs;
#define GETADDR(name) funcs.name = reinterpret_cast<PFN_vk##name>(gipa(*pInstance, "vk" #name))

    GETADDR(GetInstanceProcAddr);
    GETADDR(DestroyInstance);
    GETADDR(EnumeratePhysicalDevices);
    GETADDR(GetPhysicalDeviceProperties);
    GETADDR(GetPhysicalDeviceMemoryProperties);
    GETADDR(GetPhysicalDeviceQueueFamilyProperties);
    GETADDR(EnumerateDeviceExtensionProperties);

#undef GETADDR

    uint32_t phys_count = 0;
    funcs.EnumeratePhysicalDevices(*pInstance, &phys_count, nullptr);
    if (phys_count > 0) {
        std::vector<VkPhysicalDevice> phys_devices(phys_count);
        funcs.EnumeratePhysicalDevices(*pInstance, &phys_count, phys_devices.data());
        for (auto phys : phys_devices) {
            get_object_tracker().add_physical_device(phys, *pInstance);
        }
    }

    get_object_tracker().add_instance(*pInstance, inst_data);

    return VK_SUCCESS;
}

void VKAPI_CALL Goggles_DestroyInstance(VkInstance instance,
                                        const VkAllocationCallbacks* pAllocator) {

    auto* data = get_object_tracker().get_instance(instance);
    if (!data) {
        return;
    }

    PFN_vkDestroyInstance destroy_func = data->funcs.DestroyInstance;
    get_object_tracker().remove_instance(instance);
    destroy_func(instance, pAllocator);
}

// =============================================================================
// Device Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateDevice(VkPhysicalDevice physicalDevice,
                                         const VkDeviceCreateInfo* pCreateInfo,
                                         const VkAllocationCallbacks* pAllocator,
                                         VkDevice* pDevice) {

    auto* inst_data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!inst_data) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto* link_info =
        reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(pCreateInfo->pNext));
    while (link_info && !is_device_link_info(link_info)) {
        link_info = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(link_info->pNext));
    }

    if (!link_info) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;

    std::vector<const char*> extensions(pCreateInfo->ppEnabledExtensionNames,
                                        pCreateInfo->ppEnabledExtensionNames +
                                            pCreateInfo->enabledExtensionCount);

    const char* required_exts[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    };

    for (const auto* req_ext : required_exts) {
        bool found = false;
        for (const auto* ext : extensions) {
            if (strcmp(ext, req_ext) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            extensions.push_back(req_ext);
        }
    }

    VkDeviceCreateInfo modified_info = *pCreateInfo;
    modified_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    modified_info.ppEnabledExtensionNames = extensions.data();

    auto create_func =
        reinterpret_cast<PFN_vkCreateDevice>(gipa(inst_data->instance, "vkCreateDevice"));
    VkResult result = create_func(physicalDevice, &modified_info, pAllocator, pDevice);

    if (result != VK_SUCCESS) {
        result = create_func(physicalDevice, pCreateInfo, pAllocator, pDevice);
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    VkDeviceData dev_data{};
    dev_data.device = *pDevice;
    dev_data.physical_device = physicalDevice;
    dev_data.inst_data = inst_data;
    dev_data.valid = true;

    auto& funcs = dev_data.funcs;
#define GETADDR(name) funcs.name = reinterpret_cast<PFN_vk##name>(gdpa(*pDevice, "vk" #name))

    GETADDR(GetDeviceProcAddr);
    GETADDR(DestroyDevice);
    GETADDR(CreateSwapchainKHR);
    GETADDR(DestroySwapchainKHR);
    GETADDR(GetSwapchainImagesKHR);
    GETADDR(QueuePresentKHR);
    GETADDR(AllocateMemory);
    GETADDR(FreeMemory);
    GETADDR(GetImageMemoryRequirements);
    GETADDR(BindImageMemory);
    GETADDR(GetImageSubresourceLayout);
    GETADDR(GetMemoryFdKHR);
    GETADDR(CreateImage);
    GETADDR(DestroyImage);
    GETADDR(CreateCommandPool);
    GETADDR(DestroyCommandPool);
    GETADDR(ResetCommandPool);
    GETADDR(AllocateCommandBuffers);
    GETADDR(BeginCommandBuffer);
    GETADDR(EndCommandBuffer);
    GETADDR(CmdCopyImage);
    GETADDR(CmdBlitImage);
    GETADDR(CmdPipelineBarrier);
    GETADDR(GetDeviceQueue);
    GETADDR(QueueSubmit);
    GETADDR(CreateFence);
    GETADDR(DestroyFence);
    GETADDR(WaitForFences);
    GETADDR(ResetFences);
    GETADDR(CreateSemaphore);
    GETADDR(DestroySemaphore);

#undef GETADDR

    uint32_t queue_family_count = 0;
    inst_data->funcs.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count,
                                                            nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    inst_data->funcs.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count,
                                                            queue_families.data());

    for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i) {
        const auto& queue_info = pCreateInfo->pQueueCreateInfos[i];
        uint32_t family = queue_info.queueFamilyIndex;

        for (uint32_t j = 0; j < queue_info.queueCount; ++j) {
            VkQueue queue;
            funcs.GetDeviceQueue(*pDevice, family, j, &queue);
            get_object_tracker().add_queue(queue, *pDevice);

            if ((queue_families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                dev_data.graphics_queue == VK_NULL_HANDLE) {
                dev_data.graphics_queue = queue;
                dev_data.graphics_queue_family = family;
            }
        }
    }

    get_object_tracker().add_device(*pDevice, dev_data);

    return VK_SUCCESS;
}

void VKAPI_CALL Goggles_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {

    auto* data = get_object_tracker().get_device(device);
    if (!data) {
        return;
    }

    PFN_vkDestroyDevice destroy_func = data->funcs.DestroyDevice;

    get_object_tracker().remove_queues_for_device(device);
    get_object_tracker().remove_device(device);

    destroy_func(device, pAllocator);
}

// =============================================================================
// Swapchain Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateSwapchainKHR(VkDevice device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkSwapchainKHR* pSwapchain) {

    auto* data = get_object_tracker().get_device(device);
    if (!data || !data->funcs.CreateSwapchainKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // TRANSFER_SRC required for capturing swapchain images
    VkSwapchainCreateInfoKHR modified_info = *pCreateInfo;
    modified_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkResult result =
        data->funcs.CreateSwapchainKHR(device, &modified_info, pAllocator, pSwapchain);

    if (result != VK_SUCCESS) {
        result = data->funcs.CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    }

    get_capture_manager().on_swapchain_created(device, *pSwapchain, pCreateInfo, data);

    return result;
}

void VKAPI_CALL Goggles_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                            const VkAllocationCallbacks* pAllocator) {

    auto* data = get_object_tracker().get_device(device);
    if (!data || !data->funcs.DestroySwapchainKHR) {
        return;
    }

    get_capture_manager().on_swapchain_destroyed(device, swapchain);

    data->funcs.DestroySwapchainKHR(device, swapchain, pAllocator);
}

// =============================================================================
// Present Hook
// =============================================================================

VkResult VKAPI_CALL Goggles_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {

    static bool first_call = true;
    if (first_call) {
        LAYER_DEBUG("QueuePresentKHR hook called (first frame)");
        first_call = false;
    }

    auto* data = get_object_tracker().get_device_by_queue(queue);
    if (!data || !data->funcs.QueuePresentKHR) {
        LAYER_DEBUG("QueuePresentKHR: device lookup failed!");
        return VK_ERROR_DEVICE_LOST;
    }

    VkPresentInfoKHR modified_present = *pPresentInfo;
    get_capture_manager().on_present(queue, &modified_present, data);

    return data->funcs.QueuePresentKHR(queue, &modified_present);
}

} // namespace goggles::capture
