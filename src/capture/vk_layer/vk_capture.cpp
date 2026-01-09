#include "vk_capture.hpp"

#include "ipc_socket.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <util/profiling.hpp>

#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

struct Time {
    static constexpr uint64_t one_sec = 1'000'000'000;
    static constexpr uint64_t infinite = UINT64_MAX;
};

// =============================================================================
// Async Worker Configuration
// =============================================================================

static bool should_use_async_capture() {
    static const bool use_async = []() {
        const char* env = std::getenv("GOGGLES_CAPTURE_ASYNC");
        return env == nullptr || std::strcmp(env, "0") != 0;
    }();
    return use_async;
}

// =============================================================================
// Async Worker Thread
// =============================================================================

void CaptureManager::worker_func() {
    GOGGLES_PROFILE_FUNCTION();
    while (!shutdown_.load(std::memory_order_acquire)) {
        std::unique_lock lock(cv_mutex_);
        cv_.wait(lock, [this] {
            return shutdown_.load(std::memory_order_acquire) || !async_queue_.empty();
        });

        while (!async_queue_.empty()) {
            auto opt_item = async_queue_.try_pop();
            if (!opt_item) {
                break;
            }
            AsyncCaptureItem item = *opt_item;

            auto* dev_data = get_object_tracker().get_device(item.device);
            if (!dev_data) {
                close(item.dmabuf_fd);
                continue;
            }

            auto& funcs = dev_data->funcs;

            VkSemaphoreWaitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores = &item.timeline_sem;
            wait_info.pValues = &item.timeline_value;

            VkResult res = funcs.WaitSemaphoresKHR(item.device, &wait_info, Time::one_sec);
            if (res != VK_SUCCESS) {
                close(item.dmabuf_fd);
                continue;
            }

            auto& socket = get_layer_socket();
            if (socket.is_connected()) {
                socket.send_texture_with_fd(item.metadata, item.dmabuf_fd);
            }

            close(item.dmabuf_fd);
        }
    }

    // Drain remaining items on shutdown
    while (!async_queue_.empty()) {
        auto opt_item = async_queue_.try_pop();
        if (!opt_item) {
            break;
        }
        AsyncCaptureItem item = *opt_item;

        auto* dev_data = get_object_tracker().get_device(item.device);
        if (dev_data) {
            auto& funcs = dev_data->funcs;

            VkSemaphoreWaitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores = &item.timeline_sem;
            wait_info.pValues = &item.timeline_value;

            VkResult res = funcs.WaitSemaphoresKHR(item.device, &wait_info, Time::one_sec);
            if (res == VK_SUCCESS) {
                auto& socket = get_layer_socket();
                if (socket.is_connected()) {
                    socket.send_texture_with_fd(item.metadata, item.dmabuf_fd);
                }
            }
        }
        close(item.dmabuf_fd);
    }
}

// =============================================================================
// Global Instance
// =============================================================================

CaptureManager& get_capture_manager() {
    static CaptureManager manager;
    return manager;
}

CaptureManager::CaptureManager() {
    if (should_use_async_capture()) {
        LAYER_DEBUG("Async capture mode enabled");
        shutdown_.store(false, std::memory_order_release);
        worker_thread_ = std::thread(&CaptureManager::worker_func, this);
    } else {
        LAYER_DEBUG("Sync capture mode enabled");
    }
}

CaptureManager::~CaptureManager() {
    shutdown();
}

void CaptureManager::shutdown() {
    bool expected = false;
    if (!shutdown_.compare_exchange_strong(expected, true, std::memory_order_release)) {
        return; // Already shutdown
    }

    cv_.notify_one();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

// =============================================================================
// Swapchain Lifecycle
// =============================================================================

void CaptureManager::on_swapchain_created(VkDevice device, VkSwapchainKHR swapchain,
                                          const VkSwapchainCreateInfoKHR* create_info,
                                          VkDeviceData* dev_data) {

    std::lock_guard lock(mutex_);

    SwapData swap{};
    swap.swapchain = swapchain;
    swap.device = device;
    swap.extent = create_info->imageExtent;
    swap.format = create_info->imageFormat;
    swap.composite_alpha = create_info->compositeAlpha;

    // Only fully support opaque windows; alpha blending may be ignored
    if (create_info->compositeAlpha != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        const char* alpha_name = "UNKNOWN";
        switch (create_info->compositeAlpha) {
        case VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR:
            alpha_name = "PRE_MULTIPLIED";
            break;
        case VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR:
            alpha_name = "POST_MULTIPLIED";
            break;
        case VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR:
            alpha_name = "INHERIT";
            break;
        default:
            break;
        }
        LAYER_DEBUG("WARNING: Swapchain uses compositeAlpha=%s, capture may ignore alpha blending",
                    alpha_name);
    }

    uint32_t image_count = 0;
    if (dev_data->funcs.GetSwapchainImagesKHR) {
        VkResult res =
            dev_data->funcs.GetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
        LAYER_DEBUG("GetSwapchainImagesKHR query: res=%d, count=%u", res, image_count);
        if (res == VK_SUCCESS && image_count > 0) {
            swap.swap_images.resize(image_count);
            dev_data->funcs.GetSwapchainImagesKHR(device, swapchain, &image_count,
                                                  swap.swap_images.data());
        }
    } else {
        LAYER_DEBUG("GetSwapchainImagesKHR function pointer is NULL!");
    }

    LAYER_DEBUG("Swapchain created: %ux%u, format=%d, images=%zu", create_info->imageExtent.width,
                create_info->imageExtent.height, static_cast<int>(create_info->imageFormat),
                swap.swap_images.size());
    swaps_[swapchain] = std::move(swap);
}

void CaptureManager::on_swapchain_destroyed(VkDevice device, VkSwapchainKHR swapchain) {
    std::lock_guard lock(mutex_);

    auto it = swaps_.find(swapchain);
    if (it == swaps_.end()) {
        return;
    }

    auto* dev_data = get_object_tracker().get_device(device);
    if (dev_data) {
        cleanup_swap_data(&it->second, dev_data);
    }

    swaps_.erase(it);
}

SwapData* CaptureManager::get_swap_data(VkSwapchainKHR swapchain) {
    std::lock_guard lock(mutex_);
    auto it = swaps_.find(swapchain);
    return it != swaps_.end() ? &it->second : nullptr;
}

// =============================================================================
// Export Image Initialization
// =============================================================================

// from drm_fourcc.h
constexpr uint64_t DRM_FORMAT_MOD_LINEAR = 0;
constexpr uint64_t DRM_FORMAT_MOD_INVALID = 0xffffffffffffffULL;

struct ModifierInfo {
    uint64_t modifier;
    VkFormatFeatureFlags features;
    uint32_t plane_count;
};

// Query supported DRM modifiers for a format that can be used for export
static std::vector<ModifierInfo> query_export_modifiers(VkPhysicalDevice phys_device,
                                                        VkFormat format, VkInstFuncs* inst_funcs) {
    std::vector<ModifierInfo> result;

    if (!inst_funcs->GetPhysicalDeviceFormatProperties2) {
        return result;
    }

    VkDrmFormatModifierPropertiesListEXT modifier_list{};
    modifier_list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

    VkFormatProperties2 format_props{};
    format_props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    format_props.pNext = &modifier_list;

    inst_funcs->GetPhysicalDeviceFormatProperties2(phys_device, format, &format_props);

    if (modifier_list.drmFormatModifierCount == 0) {
        return result;
    }

    std::vector<VkDrmFormatModifierPropertiesEXT> modifiers(modifier_list.drmFormatModifierCount);
    modifier_list.pDrmFormatModifierProperties = modifiers.data();

    inst_funcs->GetPhysicalDeviceFormatProperties2(phys_device, format, &format_props);

    for (const auto& mod : modifiers) {
        if ((mod.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) &&
            mod.drmFormatModifierPlaneCount == 1) { // Single-plane only for now
            result.push_back({
                mod.drmFormatModifier,
                mod.drmFormatModifierTilingFeatures,
                mod.drmFormatModifierPlaneCount,
            });
        }
    }

    return result;
}

static uint32_t find_export_memory_type(const VkPhysicalDeviceMemoryProperties& mem_props,
                                        uint32_t type_bits) {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            return i;
        }
    }
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_bits & (1u << i)) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool allocate_export_memory(SwapData* swap, VkDeviceData* dev_data,
                                   const VkMemoryRequirements& mem_reqs, uint32_t mem_type_index) {
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    VkExportMemoryAllocateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &export_info;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type_index;

    VkResult res = funcs.AllocateMemory(device, &alloc_info, nullptr, &swap->export_mem);
    if (res != VK_SUCCESS) {
        return false;
    }

    res = funcs.BindImageMemory(device, swap->export_image, swap->export_mem, 0);
    if (res != VK_SUCCESS) {
        funcs.FreeMemory(device, swap->export_mem, nullptr);
        swap->export_mem = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool CaptureManager::init_export_image(SwapData* swap, VkDeviceData* dev_data) {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    auto* inst_data = dev_data->inst_data;
    VkDevice device = swap->device;

    auto modifiers =
        query_export_modifiers(dev_data->physical_device, swap->format, &inst_data->funcs);

    bool use_modifier_tiling = !modifiers.empty();

    std::vector<uint64_t> modifier_list;
    if (use_modifier_tiling) {
        for (const auto& mod : modifiers) {
            modifier_list.push_back(mod.modifier);
        }
        LAYER_DEBUG("Using DRM modifier list with %zu modifiers for format %d",
                    modifier_list.size(), swap->format);
    } else {
        LAYER_DEBUG("No suitable DRM modifiers found, falling back to LINEAR tiling");
    }

    VkExternalMemoryImageCreateInfo ext_mem_info{};
    ext_mem_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    ext_mem_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageDrmFormatModifierListCreateInfoEXT modifier_list_info{};

    if (use_modifier_tiling) {
        modifier_list_info.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
        modifier_list_info.drmFormatModifierCount = static_cast<uint32_t>(modifier_list.size());
        modifier_list_info.pDrmFormatModifiers = modifier_list.data();

        ext_mem_info.pNext = &modifier_list_info;
    }

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = &ext_mem_info;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = swap->format;
    image_info.extent = {swap->extent.width, swap->extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling =
        use_modifier_tiling ? VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT : VK_IMAGE_TILING_LINEAR;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res = funcs.CreateImage(device, &image_info, nullptr, &swap->export_image);
    if (res != VK_SUCCESS) {
        LAYER_DEBUG("CreateImage failed: %d", res);
        return false;
    }

    if (use_modifier_tiling && funcs.GetImageDrmFormatModifierPropertiesEXT) {
        VkImageDrmFormatModifierPropertiesEXT modifier_props{};
        modifier_props.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
        res = funcs.GetImageDrmFormatModifierPropertiesEXT(device, swap->export_image,
                                                           &modifier_props);
        if (res == VK_SUCCESS) {
            swap->dmabuf_modifier = modifier_props.drmFormatModifier;
            LAYER_DEBUG("Driver selected DRM modifier 0x%" PRIx64, swap->dmabuf_modifier);
        } else {
            swap->dmabuf_modifier = DRM_FORMAT_MOD_INVALID;
            LAYER_DEBUG("Failed to query DRM modifier: %d", res);
        }
    } else {
        swap->dmabuf_modifier = DRM_FORMAT_MOD_LINEAR;
    }

    VkMemoryRequirements mem_reqs;
    funcs.GetImageMemoryRequirements(device, swap->export_image, &mem_reqs);
    VkPhysicalDeviceMemoryProperties mem_props;
    inst_data->funcs.GetPhysicalDeviceMemoryProperties(dev_data->physical_device, &mem_props);
    uint32_t mem_type_index = find_export_memory_type(mem_props, mem_reqs.memoryTypeBits);

    if (mem_type_index == UINT32_MAX) {
        LAYER_DEBUG("No suitable memory type found");
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        return false;
    }

    if (!allocate_export_memory(swap, dev_data, mem_reqs, mem_type_index)) {
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryGetFdInfoKHR fd_info{};
    fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fd_info.memory = swap->export_mem;
    fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    res = funcs.GetMemoryFdKHR(device, &fd_info, &swap->dmabuf_fd);
    if (res != VK_SUCCESS || swap->dmabuf_fd < 0) {
        LAYER_DEBUG("GetMemoryFdKHR failed: %d, fd=%d", res, swap->dmabuf_fd);
        funcs.FreeMemory(device, swap->export_mem, nullptr);
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        swap->export_mem = VK_NULL_HANDLE;
        return false;
    }

    VkImageSubresource subres{};
    subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subres.mipLevel = 0;
    subres.arrayLayer = 0;

    VkSubresourceLayout layout;
    funcs.GetImageSubresourceLayout(device, swap->export_image, &subres, &layout);
    swap->dmabuf_stride = static_cast<uint32_t>(layout.rowPitch);
    swap->dmabuf_offset = static_cast<uint32_t>(layout.offset);

    if (!init_device_sync(device, dev_data)) {
        close(swap->dmabuf_fd);
        swap->dmabuf_fd = -1;
        funcs.FreeMemory(device, swap->export_mem, nullptr);
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        swap->export_mem = VK_NULL_HANDLE;
        return false;
    }

    swap->export_initialized = true;
    LAYER_DEBUG("Export image initialized: fd=%d, stride=%u, offset=%u, modifier=0x%" PRIx64,
                swap->dmabuf_fd, swap->dmabuf_stride, swap->dmabuf_offset, swap->dmabuf_modifier);
    return true;
}

DeviceSyncState* CaptureManager::get_device_sync(VkDevice device) {
    auto it = device_sync_.find(device);
    return it != device_sync_.end() ? &it->second : nullptr;
}

bool CaptureManager::init_device_sync(VkDevice device, VkDeviceData* dev_data) {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;

    auto& sync = device_sync_[device];
    if (sync.initialized) {
        return true;
    }

    VkExportSemaphoreCreateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkSemaphoreTypeCreateInfo timeline_info{};
    timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_info.pNext = &export_info;
    timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_info.initialValue = 0;

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sem_info.pNext = &timeline_info;

    VkResult res = funcs.CreateSemaphore(device, &sem_info, nullptr, &sync.frame_ready_sem);
    if (res != VK_SUCCESS) {
        LAYER_DEBUG("frame_ready_sem creation failed: %d", res);
        return false;
    }

    res = funcs.CreateSemaphore(device, &sem_info, nullptr, &sync.frame_consumed_sem);
    if (res != VK_SUCCESS) {
        LAYER_DEBUG("frame_consumed_sem creation failed: %d", res);
        funcs.DestroySemaphore(device, sync.frame_ready_sem, nullptr);
        sync.frame_ready_sem = VK_NULL_HANDLE;
        return false;
    }

    VkSemaphoreGetFdInfoKHR fd_info{};
    fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    fd_info.semaphore = sync.frame_ready_sem;
    res = funcs.GetSemaphoreFdKHR(device, &fd_info, &sync.frame_ready_fd);
    if (res != VK_SUCCESS || sync.frame_ready_fd < 0) {
        LAYER_DEBUG("frame_ready_sem FD export failed: %d", res);
        funcs.DestroySemaphore(device, sync.frame_ready_sem, nullptr);
        funcs.DestroySemaphore(device, sync.frame_consumed_sem, nullptr);
        sync.frame_ready_sem = VK_NULL_HANDLE;
        sync.frame_consumed_sem = VK_NULL_HANDLE;
        return false;
    }

    fd_info.semaphore = sync.frame_consumed_sem;
    res = funcs.GetSemaphoreFdKHR(device, &fd_info, &sync.frame_consumed_fd);
    if (res != VK_SUCCESS || sync.frame_consumed_fd < 0) {
        LAYER_DEBUG("frame_consumed_sem FD export failed: %d", res);
        close(sync.frame_ready_fd);
        sync.frame_ready_fd = -1;
        funcs.DestroySemaphore(device, sync.frame_ready_sem, nullptr);
        funcs.DestroySemaphore(device, sync.frame_consumed_sem, nullptr);
        sync.frame_ready_sem = VK_NULL_HANDLE;
        sync.frame_consumed_sem = VK_NULL_HANDLE;
        return false;
    }

    sync.initialized = true;
    LAYER_DEBUG("Cross-process semaphores created: ready_fd=%d, consumed_fd=%d",
                sync.frame_ready_fd, sync.frame_consumed_fd);
    return true;
}

void CaptureManager::reset_device_sync(VkDevice device, VkDeviceData* dev_data) {
    auto* sync = get_device_sync(device);
    if (!sync) {
        return;
    }

    cleanup_device_sync(device, dev_data);

    // Reset copy_cmds busy state for all swapchains on this device
    for (auto& [_, swap] : swaps_) {
        if (swap.device == device) {
            for (auto& cmd : swap.copy_cmds) {
                cmd.busy = false;
                cmd.timeline_value = 0;
            }
        }
    }

    if (!init_device_sync(device, dev_data)) {
        LAYER_DEBUG("Failed to recreate sync primitives on reconnect");
    }
}

void CaptureManager::cleanup_device_sync(VkDevice device, VkDeviceData* dev_data) {
    auto* sync = get_device_sync(device);
    if (!sync) {
        return;
    }

    auto& funcs = dev_data->funcs;

    if (sync->frame_ready_fd >= 0) {
        close(sync->frame_ready_fd);
        sync->frame_ready_fd = -1;
    }
    if (sync->frame_consumed_fd >= 0) {
        close(sync->frame_consumed_fd);
        sync->frame_consumed_fd = -1;
    }
    if (sync->frame_ready_sem != VK_NULL_HANDLE) {
        funcs.DestroySemaphore(device, sync->frame_ready_sem, nullptr);
        sync->frame_ready_sem = VK_NULL_HANDLE;
    }
    if (sync->frame_consumed_sem != VK_NULL_HANDLE) {
        funcs.DestroySemaphore(device, sync->frame_consumed_sem, nullptr);
        sync->frame_consumed_sem = VK_NULL_HANDLE;
    }

    sync->semaphores_sent = false;
    sync->frame_counter = 0;
    sync->initialized = false;
}

void CaptureManager::on_device_destroyed(VkDevice device, VkDeviceData* dev_data) {
    std::lock_guard lock(mutex_);
    cleanup_device_sync(device, dev_data);
    device_sync_.erase(device);
}

// =============================================================================
// Copy Command Buffers
// =============================================================================

bool CaptureManager::init_copy_cmds(SwapData* swap, VkDeviceData* dev_data) {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    size_t count = swap->swap_images.size();
    swap->copy_cmds.resize(count);

    for (size_t i = 0; i < count; ++i) {
        CopyCmd& cmd = swap->copy_cmds[i];

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = 0;
        pool_info.queueFamilyIndex = dev_data->graphics_queue_family;
        VkResult res = funcs.CreateCommandPool(device, &pool_info, nullptr, &cmd.pool);
        if (res != VK_SUCCESS) {
            LAYER_DEBUG("CreateCommandPool failed for copy cmd %zu: %d", i, res);
            destroy_copy_cmds(swap, dev_data);
            return false;
        }

        VkCommandBufferAllocateInfo cmd_info{};
        cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_info.commandPool = cmd.pool;
        cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_info.commandBufferCount = 1;
        res = funcs.AllocateCommandBuffers(device, &cmd_info, &cmd.cmd);
        if (res != VK_SUCCESS) {
            LAYER_DEBUG("AllocateCommandBuffers failed for copy cmd %zu: %d", i, res);
            destroy_copy_cmds(swap, dev_data);
            return false;
        }

        // Record copy commands for this swapchain image
        VkImage src_image = swap->swap_images[i];
        VkImage dst_image = swap->export_image;

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0; // Reusable
        funcs.BeginCommandBuffer(cmd.cmd, &begin_info);

        VkImageMemoryBarrier src_barrier{};
        src_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        src_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        src_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        src_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        src_barrier.image = src_image;
        src_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        src_barrier.subresourceRange.baseMipLevel = 0;
        src_barrier.subresourceRange.levelCount = 1;
        src_barrier.subresourceRange.baseArrayLayer = 0;
        src_barrier.subresourceRange.layerCount = 1;

        VkImageMemoryBarrier dst_barrier{};
        dst_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        dst_barrier.srcAccessMask = 0;
        dst_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        dst_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dst_barrier.image = dst_image;
        dst_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        dst_barrier.subresourceRange.baseMipLevel = 0;
        dst_barrier.subresourceRange.levelCount = 1;
        dst_barrier.subresourceRange.baseArrayLayer = 0;
        dst_barrier.subresourceRange.layerCount = 1;

        VkImageMemoryBarrier barriers[2] = {src_barrier, dst_barrier};
        funcs.CmdPipelineBarrier(cmd.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2,
                                 barriers);

        VkImageCopy copy_region{};
        copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.srcSubresource.mipLevel = 0;
        copy_region.srcSubresource.baseArrayLayer = 0;
        copy_region.srcSubresource.layerCount = 1;
        copy_region.srcOffset = {0, 0, 0};
        copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.dstSubresource.mipLevel = 0;
        copy_region.dstSubresource.baseArrayLayer = 0;
        copy_region.dstSubresource.layerCount = 1;
        copy_region.dstOffset = {0, 0, 0};
        copy_region.extent = {swap->extent.width, swap->extent.height, 1};

        funcs.CmdCopyImage(cmd.cmd, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        src_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        src_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        src_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_barrier.dstAccessMask = 0;
        dst_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dst_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

        barriers[0] = src_barrier;
        barriers[1] = dst_barrier;
        funcs.CmdPipelineBarrier(cmd.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT |
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0, 0, nullptr, 0, nullptr, 2, barriers);

        funcs.EndCommandBuffer(cmd.cmd);
    }

    LAYER_DEBUG("Initialized %zu copy command buffers", count);
    return true;
}

void CaptureManager::destroy_copy_cmds(SwapData* swap, VkDeviceData* dev_data) {
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;
    auto* sync = get_device_sync(device);

    for (auto& cmd : swap->copy_cmds) {
        if (cmd.busy && sync && sync->frame_ready_sem != VK_NULL_HANDLE) {
            VkSemaphoreWaitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores = &sync->frame_ready_sem;
            wait_info.pValues = &cmd.timeline_value;

            funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
        }
        if (cmd.pool != VK_NULL_HANDLE) {
            funcs.DestroyCommandPool(device, cmd.pool, nullptr);
        }
    }
    swap->copy_cmds.clear();
}

// =============================================================================
// Frame Capture
// =============================================================================

void CaptureManager::on_present(VkQueue queue, const VkPresentInfoKHR* present_info,
                                VkDeviceData* dev_data) {
    GOGGLES_PROFILE_FUNCTION();
    if (present_info->swapchainCount == 0) {
        return;
    }

    VkSwapchainKHR swapchain = present_info->pSwapchains[0];
    uint32_t image_index = present_info->pImageIndices[0];

    std::lock_guard lock(mutex_);

    auto it = swaps_.find(swapchain);
    if (it == swaps_.end()) {
        return;
    }

    SwapData* swap = &it->second;

    auto& socket = get_layer_socket();
    if (!socket.is_connected()) {
        if (socket.connect()) {
            LAYER_DEBUG("Connected to Goggles app");
        } else {
            return;
        }
    }

    if (!swap->export_initialized) {
        LAYER_DEBUG("Initializing export image...");
        if (!init_export_image(swap, dev_data)) {
            LAYER_DEBUG("Export image init FAILED");
            return;
        }
        if (!init_copy_cmds(swap, dev_data)) {
            LAYER_DEBUG("Copy commands init FAILED");
            return;
        }
    }

    CaptureControl ctrl{};
    socket.poll_control(ctrl);

    VkPresentInfoKHR modified_present = *present_info;
    capture_frame(swap, image_index, queue, dev_data, &modified_present);
}

void CaptureManager::capture_frame(SwapData* swap, uint32_t image_index, VkQueue queue,
                                   VkDeviceData* dev_data,
                                   [[maybe_unused]] VkPresentInfoKHR* present_info) {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;
    auto& socket = get_layer_socket();

    if (swap->copy_cmds.empty() || image_index >= swap->copy_cmds.size()) {
        return;
    }

    if (!socket.is_connected()) {
        return;
    }

    auto* sync = get_device_sync(device);
    if (!sync || !sync->initialized) {
        return;
    }

    CopyCmd& cmd = swap->copy_cmds[image_index];
    uint64_t current_frame = sync->frame_counter + 1;

    // Send semaphore FDs on first frame
    if (!sync->semaphores_sent && sync->frame_ready_fd >= 0 && sync->frame_consumed_fd >= 0) {
        if (socket.send_semaphores(sync->frame_ready_fd, sync->frame_consumed_fd)) {
            sync->semaphores_sent = true;
            LAYER_DEBUG("Semaphore FDs sent to Goggles");
        }
    }

    // Back-pressure: wait for Goggles to consume previous frame (N-1)
    if (current_frame > 1) {
        uint64_t wait_value = current_frame - 1;
        VkSemaphoreWaitInfo wait_info{};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &sync->frame_consumed_sem;
        wait_info.pValues = &wait_value;

        constexpr uint64_t timeout_ns = 500'000'000; // 500ms
        VkResult res = funcs.WaitSemaphoresKHR(device, &wait_info, timeout_ns);
        if (res == VK_TIMEOUT) {
            LAYER_DEBUG("Timeout waiting for frame_consumed, resetting sync primitives");
            reset_device_sync(device, dev_data);
            return;
        }
    }

    // Wait if this command buffer is still in flight
    if (cmd.busy) {
        VkSemaphoreWaitInfo wait_info{};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &sync->frame_ready_sem;
        wait_info.pValues = &cmd.timeline_value;

        funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
        cmd.busy = false;
    }

    // Submit copy with signal on frame_ready
    cmd.timeline_value = current_frame;

    VkTimelineSemaphoreSubmitInfo timeline_submit{};
    timeline_submit.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_submit.signalSemaphoreValueCount = 1;
    timeline_submit.pSignalSemaphoreValues = &current_frame;

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = &timeline_submit;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &sync->frame_ready_sem;

    VkResult res = funcs.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        return;
    }

    sync->frame_counter = current_frame;
    cmd.busy = true;

    // Send frame metadata
    CaptureFrameMetadata metadata{};
    metadata.type = CaptureMessageType::frame_metadata;
    metadata.width = swap->extent.width;
    metadata.height = swap->extent.height;
    metadata.format = swap->format;
    metadata.stride = swap->dmabuf_stride;
    metadata.offset = swap->dmabuf_offset;
    metadata.modifier = swap->dmabuf_modifier;
    metadata.frame_number = current_frame;

    // First frame after export init also sends DMA-BUF FD
    if (current_frame == 1 || !swap->dmabuf_sent) {
        socket.send_texture_with_fd(metadata, swap->dmabuf_fd);
        swap->dmabuf_sent = true;
    } else {
        socket.send_frame_metadata(metadata);
    }
}

// =============================================================================
// Cleanup
// =============================================================================

void CaptureManager::cleanup_swap_data(SwapData* swap, VkDeviceData* dev_data) {
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    destroy_copy_cmds(swap, dev_data);

    if (swap->dmabuf_fd >= 0) {
        close(swap->dmabuf_fd);
        swap->dmabuf_fd = -1;
    }

    if (swap->export_mem != VK_NULL_HANDLE) {
        funcs.FreeMemory(device, swap->export_mem, nullptr);
        swap->export_mem = VK_NULL_HANDLE;
    }

    if (swap->export_image != VK_NULL_HANDLE) {
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
    }

    swap->export_initialized = false;
    swap->dmabuf_sent = false;
}

} // namespace goggles::capture
