#include "vk_hooks.hpp"

#include "ipc_socket.hpp"
#include "vk_capture.hpp"
#include "vk_dispatch.hpp"
#include "wsi_virtual.hpp"

#include <cstdio>
#include <cstring>
#include <util/profiling.hpp>
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
    GETADDR(GetPhysicalDeviceFormatProperties2);
    GETADDR(GetPhysicalDeviceImageFormatProperties2);
    GETADDR(DestroySurfaceKHR);
    GETADDR(GetPhysicalDeviceSurfaceCapabilitiesKHR);
    GETADDR(GetPhysicalDeviceSurfaceFormatsKHR);
    GETADDR(GetPhysicalDeviceSurfacePresentModesKHR);
    GETADDR(GetPhysicalDeviceSurfaceSupportKHR);
    GETADDR(GetPhysicalDeviceSurfaceCapabilities2KHR);
    GETADDR(GetPhysicalDeviceSurfaceFormats2KHR);

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
        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
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

    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features{};
    timeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timeline_features.timelineSemaphore = VK_TRUE;
    timeline_features.pNext = const_cast<void*>(modified_info.pNext);
    modified_info.pNext = &timeline_features;

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
    GETADDR(AcquireNextImageKHR);
    GETADDR(QueuePresentKHR);
    GETADDR(AllocateMemory);
    GETADDR(FreeMemory);
    GETADDR(GetImageMemoryRequirements);
    GETADDR(BindImageMemory);
    GETADDR(GetImageSubresourceLayout);
    GETADDR(GetMemoryFdKHR);
    GETADDR(GetImageDrmFormatModifierPropertiesEXT);
    GETADDR(GetSemaphoreFdKHR);
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
    GETADDR(WaitSemaphoresKHR);

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

    get_capture_manager().on_device_destroyed(device, data);
    get_object_tracker().remove_queues_for_device(device);
    get_object_tracker().remove_device(device);

    destroy_func(device, pAllocator);
}

// =============================================================================
// Surface Hooks (WSI proxy)
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateXlibSurfaceKHR(VkInstance instance,
                                                 const VkXlibSurfaceCreateInfoKHR* /*pCreateInfo*/,
                                                 const VkAllocationCallbacks* /*pAllocator*/,
                                                 VkSurfaceKHR* pSurface) {
    auto& virt = WsiVirtualizer::instance();
    if (virt.is_enabled()) {
        return virt.create_surface(instance, pSurface);
    }

    auto* data = get_object_tracker().get_instance(instance);
    if (!data) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VkResult VKAPI_CALL Goggles_CreateXcbSurfaceKHR(VkInstance instance,
                                                const VkXcbSurfaceCreateInfoKHR* /*pCreateInfo*/,
                                                const VkAllocationCallbacks* /*pAllocator*/,
                                                VkSurfaceKHR* pSurface) {
    auto& virt = WsiVirtualizer::instance();
    if (virt.is_enabled()) {
        return virt.create_surface(instance, pSurface);
    }

    auto* data = get_object_tracker().get_instance(instance);
    if (!data) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VkResult VKAPI_CALL Goggles_CreateWaylandSurfaceKHR(
    VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* /*pCreateInfo*/,
    const VkAllocationCallbacks* /*pAllocator*/, VkSurfaceKHR* pSurface) {
    auto& virt = WsiVirtualizer::instance();
    if (virt.is_enabled()) {
        return virt.create_surface(instance, pSurface);
    }

    auto* data = get_object_tracker().get_instance(instance);
    if (!data) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VKAPI_CALL Goggles_DestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                          const VkAllocationCallbacks* pAllocator) {
    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(surface)) {
        virt.destroy_surface(instance, surface);
        return;
    }

    auto* data = get_object_tracker().get_instance(instance);
    if (data && data->funcs.DestroySurfaceKHR) {
        data->funcs.DestroySurfaceKHR(instance, surface, pAllocator);
    }
}

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* pCapabilities) {

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(surface)) {
        return virt.get_surface_capabilities(physicalDevice, surface, pCapabilities);
    }

    auto* data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!data || !data->funcs.GetPhysicalDeviceSurfaceCapabilitiesKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                               pCapabilities);
}

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount,
    VkSurfaceFormatKHR* pSurfaceFormats) {
    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(surface)) {
        return virt.get_surface_formats(physicalDevice, surface, pSurfaceFormatCount,
                                        pSurfaceFormats);
    }

    auto* data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!data || !data->funcs.GetPhysicalDeviceSurfaceFormatsKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                          pSurfaceFormatCount, pSurfaceFormats);
}

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount,
    VkPresentModeKHR* pPresentModes) {

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(surface)) {
        return virt.get_surface_present_modes(physicalDevice, surface, pPresentModeCount,
                                              pPresentModes);
    }

    auto* data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!data || !data->funcs.GetPhysicalDeviceSurfacePresentModesKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                               pPresentModeCount, pPresentModes);
}

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                                               uint32_t queueFamilyIndex,
                                                               VkSurfaceKHR surface,
                                                               VkBool32* pSupported) {
    auto* data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!data) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(surface)) {
        return virt.get_surface_support(physicalDevice, queueFamilyIndex, surface, pSupported,
                                        data);
    }

    if (!data->funcs.GetPhysicalDeviceSurfaceSupportKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface,
                                                          pSupported);
}

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    VkSurfaceCapabilities2KHR* pSurfaceCapabilities) {

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(pSurfaceInfo->surface)) {
        return virt.get_surface_capabilities(physicalDevice, pSurfaceInfo->surface,
                                             &pSurfaceCapabilities->surfaceCapabilities);
    }

    auto* data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!data || !data->funcs.GetPhysicalDeviceSurfaceCapabilities2KHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, pSurfaceInfo,
                                                                pSurfaceCapabilities);
}

VkResult VKAPI_CALL Goggles_GetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats) {

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(pSurfaceInfo->surface)) {
        if (!pSurfaceFormats) {
            return virt.get_surface_formats(physicalDevice, pSurfaceInfo->surface,
                                            pSurfaceFormatCount, nullptr);
        }
        VkSurfaceFormatKHR formats[2];
        uint32_t count = *pSurfaceFormatCount < 2 ? *pSurfaceFormatCount : 2;
        VkResult res =
            virt.get_surface_formats(physicalDevice, pSurfaceInfo->surface, &count, formats);
        for (uint32_t i = 0; i < count; ++i) {
            pSurfaceFormats[i].surfaceFormat = formats[i];
        }
        *pSurfaceFormatCount = count;
        return res;
    }

    auto* data = get_object_tracker().get_instance_by_physical_device(physicalDevice);
    if (!data || !data->funcs.GetPhysicalDeviceSurfaceFormats2KHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, pSurfaceInfo,
                                                           pSurfaceFormatCount, pSurfaceFormats);
}

// =============================================================================
// Swapchain Hooks
// =============================================================================

VkResult VKAPI_CALL Goggles_CreateSwapchainKHR(VkDevice device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkSwapchainKHR* pSwapchain) {
    auto* data = get_object_tracker().get_device(device);
    if (!data) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_surface(pCreateInfo->surface)) {
        return virt.create_swapchain(device, pCreateInfo, pSwapchain, data);
    }

    if (!data->funcs.CreateSwapchainKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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
    if (!data) {
        return;
    }

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_swapchain(swapchain)) {
        virt.destroy_swapchain(device, swapchain, data);
        return;
    }

    if (!data->funcs.DestroySwapchainKHR) {
        return;
    }

    get_capture_manager().on_swapchain_destroyed(device, swapchain);
    data->funcs.DestroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult VKAPI_CALL Goggles_GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                  uint32_t* pSwapchainImageCount,
                                                  VkImage* pSwapchainImages) {
    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_swapchain(swapchain)) {
        return virt.get_swapchain_images(swapchain, pSwapchainImageCount, pSwapchainImages);
    }

    auto* data = get_object_tracker().get_device(device);
    if (!data || !data->funcs.GetSwapchainImagesKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return data->funcs.GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount,
                                             pSwapchainImages);
}

VkResult VKAPI_CALL Goggles_AcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                uint64_t timeout, VkSemaphore semaphore,
                                                VkFence fence, uint32_t* pImageIndex) {
    auto* data = get_object_tracker().get_device(device);
    if (!data) {
        return VK_ERROR_DEVICE_LOST;
    }

    auto& virt = WsiVirtualizer::instance();
    if (virt.is_virtual_swapchain(swapchain)) {
        return virt.acquire_next_image(device, swapchain, timeout, semaphore, fence, pImageIndex,
                                       data);
    }

    if (!data->funcs.AcquireNextImageKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return data->funcs.AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence,
                                           pImageIndex);
}

// =============================================================================
// Present Hook
// =============================================================================

VkResult VKAPI_CALL Goggles_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    GOGGLES_PROFILE_FRAME("Layer");

    static bool first_call = true;
    if (first_call) {
        LAYER_DEBUG("QueuePresentKHR hook called (first frame)");
        first_call = false;
    }

    auto* data = get_object_tracker().get_device_by_queue(queue);
    if (!data) {
        LAYER_DEBUG("QueuePresentKHR: device lookup failed!");
        return VK_ERROR_DEVICE_LOST;
    }

    auto& virt = WsiVirtualizer::instance();

    bool all_virtual = true;
    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
        if (virt.is_virtual_swapchain(pPresentInfo->pSwapchains[i])) {
            uint32_t img_idx = pPresentInfo->pImageIndices[i];
            auto frame = virt.get_frame_data(pPresentInfo->pSwapchains[i], img_idx);
            if (frame.valid) {
                auto& socket = get_layer_socket();
                if (!socket.is_connected()) {
                    socket.connect();
                }
                if (socket.is_connected()) {
                    CaptureTextureData tex{};
                    tex.type = CaptureMessageType::texture_data;
                    tex.width = frame.width;
                    tex.height = frame.height;
                    tex.format = frame.format;
                    tex.stride = frame.stride;
                    tex.offset = 0;
                    tex.modifier = 0;
                    socket.send_texture(tex, frame.dmabuf_fd);
                }
            }
        } else {
            all_virtual = false;
        }
    }

    if (all_virtual) {
        return VK_SUCCESS;
    }

    if (!data->funcs.QueuePresentKHR) {
        return VK_ERROR_DEVICE_LOST;
    }

    VkPresentInfoKHR modified_present = *pPresentInfo;
    get_capture_manager().on_present(queue, &modified_present, data);

    return data->funcs.QueuePresentKHR(queue, &modified_present);
}

} // namespace goggles::capture
