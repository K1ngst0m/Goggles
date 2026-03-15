#include "device.hpp"

#include "instance.hpp"
#include "support/logging.hpp"

#include <new>

#define GOGGLES_LOG_TAG "render.runtime"

namespace goggles::filter_chain::runtime {

auto Device::create(Instance* instance, const goggles_fc_vk_device_create_info_t* create_info,
                    goggles_fc_device_t** out_device) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        instance != nullptr ? instance->log_router() : nullptr);

    if (create_info == nullptr || out_device == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    auto* device = new (std::nothrow) Device();
    if (device == nullptr) {
        *out_device = nullptr;
        return GOGGLES_FC_STATUS_OUT_OF_MEMORY;
    }

    device->m_instance = instance;
    device->m_physical_device = create_info->physical_device;
    device->m_device = create_info->device;
    device->m_graphics_queue = create_info->graphics_queue;
    device->m_graphics_queue_family_index = create_info->graphics_queue_family_index;

    if (create_info->cache_dir.data != nullptr && create_info->cache_dir.size > 0) {
        device->m_cache_dir.assign(create_info->cache_dir.data, create_info->cache_dir.size);
    }

    // Create owned setup/upload command pool from the borrowed VkDevice.
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags =
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = create_info->graphics_queue_family_index;

    VkResult vk_result = vkCreateCommandPool(create_info->device, &pool_info, nullptr,
                                             &device->m_setup_command_pool);
    if (vk_result != VK_SUCCESS) {
        GOGGLES_LOG_ERROR("Failed to create device setup command pool: VkResult={}",
                          static_cast<int>(vk_result));
        delete device;
        *out_device = nullptr;
        return GOGGLES_FC_STATUS_VULKAN_ERROR;
    }

    // Create owned setup/upload fence (starts signaled so first wait succeeds).
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_result = vkCreateFence(create_info->device, &fence_info, nullptr, &device->m_setup_fence);
    if (vk_result != VK_SUCCESS) {
        GOGGLES_LOG_ERROR("Failed to create device setup fence: VkResult={}",
                          static_cast<int>(vk_result));
        vkDestroyCommandPool(create_info->device, device->m_setup_command_pool, nullptr);
        device->m_setup_command_pool = VK_NULL_HANDLE;
        delete device;
        *out_device = nullptr;
        return GOGGLES_FC_STATUS_VULKAN_ERROR;
    }

    // Create owned pipeline cache for device-scoped pipeline compilation.
    // A failed pipeline cache is non-fatal — pipelines still compile without caching.
    VkPipelineCacheCreateInfo cache_info{};
    cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vk_result =
        vkCreatePipelineCache(create_info->device, &cache_info, nullptr, &device->m_pipeline_cache);
    if (vk_result != VK_SUCCESS) {
        GOGGLES_LOG_WARN("Failed to create device pipeline cache: VkResult={} (non-fatal)",
                         static_cast<int>(vk_result));
        device->m_pipeline_cache = VK_NULL_HANDLE;
    }

    *out_device = device->as_handle();
    return GOGGLES_FC_STATUS_OK;
}

Device::~Device() {
    // Teardown ordering: destroy owned Vulkan resources before releasing the device.
    // Borrowed Vulkan handles (VkDevice, VkQueue, VkPhysicalDevice) are NOT destroyed;
    // the host owns those and must keep them valid until after device destroy returns.
    if (m_device != VK_NULL_HANDLE) {
        if (m_pipeline_cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
            m_pipeline_cache = VK_NULL_HANDLE;
        }
        if (m_setup_fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_setup_fence, nullptr);
            m_setup_fence = VK_NULL_HANDLE;
        }
        if (m_setup_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_setup_command_pool, nullptr);
            m_setup_command_pool = VK_NULL_HANDLE;
        }
    }
    m_magic = 0;
}

auto Device::as_handle() -> goggles_fc_device_t* {
    return reinterpret_cast<goggles_fc_device_t*>(this);
}

auto Device::from_handle(goggles_fc_device_t* handle) -> Device* {
    return reinterpret_cast<Device*>(handle);
}

auto Device::from_handle(const goggles_fc_device_t* handle) -> const Device* {
    return reinterpret_cast<const Device*>(handle);
}

} // namespace goggles::filter_chain::runtime
