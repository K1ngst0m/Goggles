#include "vk_capture.hpp"

#include "ipc_socket.hpp"

#include <cinttypes>
#include <util/profiling.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

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
                socket.send_texture(item.metadata, item.dmabuf_fd);
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
                    socket.send_texture(item.metadata, item.dmabuf_fd);
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
        return;  // Already shutdown
    }

    cv_.notify_one();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    std::lock_guard lock(mutex_);
    for (auto& [handle, swap] : swaps_) {
        auto* dev_data = get_object_tracker().get_device(swap.device);
        if (dev_data) {
            cleanup_swap_data(&swap, dev_data);
        }
    }
    swaps_.clear();
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

    if (!init_sync_primitives(swap, dev_data)) {
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

bool CaptureManager::init_sync_primitives(SwapData* swap, VkDeviceData* dev_data) {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    VkSemaphoreTypeCreateInfo timeline_info{};
    timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_info.initialValue = 0;

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sem_info.pNext = &timeline_info;

    VkResult res = funcs.CreateSemaphore(device, &sem_info, nullptr, &swap->timeline_sem);
    if (res == VK_SUCCESS) {
        LAYER_DEBUG("Timeline semaphore created");
    } else {
        LAYER_DEBUG("Timeline semaphore creation failed: %d", res);
        swap->timeline_sem = VK_NULL_HANDLE;
    }

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    res = funcs.CreateFence(device, &fence_info, nullptr, &swap->sync_fence);
    if (res != VK_SUCCESS) {
        LAYER_DEBUG("Fence creation failed: %d, capture disabled", res);
        if (swap->timeline_sem != VK_NULL_HANDLE) {
            funcs.DestroySemaphore(device, swap->timeline_sem, nullptr);
            swap->timeline_sem = VK_NULL_HANDLE;
        }
        return false;
    }

    return true;
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
        begin_info.flags = 0;  // Reusable
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

    for (auto& cmd : swap->copy_cmds) {
        if (cmd.busy && swap->timeline_sem != VK_NULL_HANDLE) {
            VkSemaphoreWaitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores = &swap->timeline_sem;
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

    auto& socket = get_layer_socket();
    if (!socket.is_connected()) {
        if (!socket.connect()) {
            return;
        }
        LAYER_DEBUG("Connected to Goggles app");
    }

    CaptureControl ctrl{};
    if (socket.poll_control(ctrl)) {
        LAYER_DEBUG("Control message received: capturing=%d", ctrl.capturing);
    }

    if (!socket.is_capturing()) {
        static bool last_capturing = true;
        if (last_capturing) {
            LAYER_DEBUG("Capture inactive, skipping GPU work");
            last_capturing = false;
        }
        return;
    }
    
    // Reset the static flag when we start capturing
    static bool last_capturing_active = false;
    if (!last_capturing_active) {
        LAYER_DEBUG("Capture active, proceeding with GPU work");
        last_capturing_active = true;
    }

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

    VkPresentInfoKHR modified_present = *present_info;
    capture_frame(swap, image_index, queue, dev_data, &modified_present);
}

void CaptureManager::capture_frame(SwapData* swap, uint32_t image_index, VkQueue queue,
                                   VkDeviceData* dev_data,
                                   [[maybe_unused]] VkPresentInfoKHR* present_info) {
    GOGGLES_PROFILE_FUNCTION();
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    if (swap->copy_cmds.empty() || image_index >= swap->copy_cmds.size()) {
        return;
    }

    CopyCmd& cmd = swap->copy_cmds[image_index];

    // Wait if this command buffer is still in flight
    if (cmd.busy && swap->timeline_sem != VK_NULL_HANDLE) {
        VkSemaphoreWaitInfo wait_info{};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &swap->timeline_sem;
        wait_info.pValues = &cmd.timeline_value;

        funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
        cmd.busy = false;
    }

    // No recording needed - command buffer is pre-recorded

    VkResult res;

    if (swap->timeline_sem != VK_NULL_HANDLE && should_use_async_capture()) {
        uint64_t timeline_value = ++swap->frame_counter;
        cmd.timeline_value = timeline_value;

        VkTimelineSemaphoreSubmitInfo timeline_submit{};
        timeline_submit.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timeline_submit.signalSemaphoreValueCount = 1;
        timeline_submit.pSignalSemaphoreValues = &timeline_value;

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = &timeline_submit;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd.cmd;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &swap->timeline_sem;

        res = funcs.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        if (res != VK_SUCCESS) {
            return;
        }

        int dup_fd = dup(swap->dmabuf_fd);
        if (dup_fd < 0) {
            VkSemaphoreWaitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores = &swap->timeline_sem;
            wait_info.pValues = &timeline_value;

            funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
            return;
        }

        CaptureTextureData tex_data{};
        tex_data.type = CaptureMessageType::texture_data;
        tex_data.width = swap->extent.width;
        tex_data.height = swap->extent.height;
        tex_data.format = swap->format;
        tex_data.stride = swap->dmabuf_stride;
        tex_data.offset = swap->dmabuf_offset;
        tex_data.modifier = swap->dmabuf_modifier;

        AsyncCaptureItem item{};
        item.device = device;
        item.timeline_sem = swap->timeline_sem;
        item.timeline_value = timeline_value;
        item.dmabuf_fd = dup_fd;
        item.metadata = tex_data;

        if (!async_queue_.try_push(item)) {
            close(dup_fd);
            VkSemaphoreWaitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores = &swap->timeline_sem;
            wait_info.pValues = &timeline_value;

            funcs.WaitSemaphoresKHR(device, &wait_info, Time::infinite);
            return;
        }

        cv_.notify_one();
        cmd.busy = true;
    } else {
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd.cmd;

        res = funcs.QueueSubmit(queue, 1, &submit_info, swap->sync_fence);
        if (res != VK_SUCCESS) {
            return;
        }

        funcs.WaitForFences(device, 1, &swap->sync_fence, VK_TRUE, Time::infinite);
        funcs.ResetFences(device, 1, &swap->sync_fence);

        auto& socket = get_layer_socket();
        if (socket.is_connected()) {
            CaptureTextureData tex_data{};
            tex_data.type = CaptureMessageType::texture_data;
            tex_data.width = swap->extent.width;
            tex_data.height = swap->extent.height;
            tex_data.format = swap->format;
            tex_data.stride = swap->dmabuf_stride;
            tex_data.offset = swap->dmabuf_offset;
            tex_data.modifier = swap->dmabuf_modifier;

            socket.send_texture(tex_data, swap->dmabuf_fd);
        }
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

    if (swap->timeline_sem != VK_NULL_HANDLE) {
        funcs.DestroySemaphore(device, swap->timeline_sem, nullptr);
        swap->timeline_sem = VK_NULL_HANDLE;
    }

    if (swap->sync_fence != VK_NULL_HANDLE) {
        funcs.DestroyFence(device, swap->sync_fence, nullptr);
        swap->sync_fence = VK_NULL_HANDLE;
    }

    swap->export_initialized = false;
}

} // namespace goggles::capture
