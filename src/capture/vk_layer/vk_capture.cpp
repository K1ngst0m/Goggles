#include "vk_capture.hpp"

#include "ipc_socket.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#define LAYER_DEBUG(fmt, ...) fprintf(stderr, "[goggles-layer] " fmt "\n", ##__VA_ARGS__)

namespace goggles::capture {

// =============================================================================
// Global Instance
// =============================================================================

CaptureManager& get_capture_manager() {
    static CaptureManager manager;
    return manager;
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

bool CaptureManager::init_export_image(SwapData* swap, VkDeviceData* dev_data) {
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

    // HOST_VISIBLE is not required for modifier-based tiling
    VkPhysicalDeviceMemoryProperties mem_props;
    inst_data->funcs.GetPhysicalDeviceMemoryProperties(dev_data->physical_device, &mem_props);

    uint32_t mem_type_index = find_export_memory_type(mem_props, mem_reqs.memoryTypeBits);

    if (mem_type_index == UINT32_MAX) {
        LAYER_DEBUG("No suitable memory type found");
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        return false;
    }

    VkExportMemoryAllocateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &export_info;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type_index;

    res = funcs.AllocateMemory(device, &alloc_info, nullptr, &swap->export_mem);
    if (res != VK_SUCCESS) {
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        return false;
    }

    res = funcs.BindImageMemory(device, swap->export_image, swap->export_mem, 0);
    if (res != VK_SUCCESS) {
        funcs.FreeMemory(device, swap->export_mem, nullptr);
        funcs.DestroyImage(device, swap->export_image, nullptr);
        swap->export_image = VK_NULL_HANDLE;
        swap->export_mem = VK_NULL_HANDLE;
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

    swap->export_initialized = true;
    LAYER_DEBUG("Export image initialized: fd=%d, stride=%u, offset=%u, modifier=0x%" PRIx64,
                swap->dmabuf_fd, swap->dmabuf_stride, swap->dmabuf_offset, swap->dmabuf_modifier);
    return true;
}

// =============================================================================
// Frame Resources
// =============================================================================

void CaptureManager::create_frame_resources(SwapData* swap, VkDeviceData* dev_data,
                                            uint32_t count) {
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    swap->frames.resize(count);

    for (auto& frame : swap->frames) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = dev_data->graphics_queue_family;
        funcs.CreateCommandPool(device, &pool_info, nullptr, &frame.cmd_pool);

        VkCommandBufferAllocateInfo cmd_info{};
        cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_info.commandPool = frame.cmd_pool;
        cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_info.commandBufferCount = 1;
        funcs.AllocateCommandBuffers(device, &cmd_info, &frame.cmd_buffer);

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        funcs.CreateFence(device, &fence_info, nullptr, &frame.fence);

        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        funcs.CreateSemaphore(device, &sem_info, nullptr, &frame.semaphore);
    }
}

void CaptureManager::destroy_frame_resources(SwapData* swap, VkDeviceData* dev_data) {
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    for (auto& frame : swap->frames) {
        if (frame.cmd_buffer_busy && frame.fence != VK_NULL_HANDLE) {
            funcs.WaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);
        }
        if (frame.semaphore != VK_NULL_HANDLE) {
            funcs.DestroySemaphore(device, frame.semaphore, nullptr);
        }
        if (frame.fence != VK_NULL_HANDLE) {
            funcs.DestroyFence(device, frame.fence, nullptr);
        }
        if (frame.cmd_pool != VK_NULL_HANDLE) {
            funcs.DestroyCommandPool(device, frame.cmd_pool, nullptr);
        }
    }
    swap->frames.clear();
}

// =============================================================================
// Frame Capture
// =============================================================================

void CaptureManager::on_present(VkQueue queue, const VkPresentInfoKHR* present_info,
                                VkDeviceData* dev_data) {
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
        create_frame_resources(swap, dev_data, static_cast<uint32_t>(swap->swap_images.size()));
    }

    auto& socket = get_layer_socket();
    if (!socket.is_connected()) {
        if (socket.connect()) {
            LAYER_DEBUG("Connected to Goggles app");
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
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    if (swap->frames.empty() || image_index >= swap->swap_images.size()) {
        return;
    }

    uint32_t frame_idx = swap->frame_index;
    swap->frame_index = (frame_idx + 1) % static_cast<uint32_t>(swap->frames.size());
    FrameData& frame = swap->frames[frame_idx];

    if (frame.cmd_buffer_busy) {
        funcs.WaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);
        funcs.ResetFences(device, 1, &frame.fence);
        frame.cmd_buffer_busy = false;
    }

    funcs.ResetCommandPool(device, frame.cmd_pool, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    funcs.BeginCommandBuffer(frame.cmd_buffer, &begin_info);

    VkImage src_image = swap->swap_images[image_index];
    VkImage dst_image = swap->export_image;

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
    funcs.CmdPipelineBarrier(frame.cmd_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
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

    funcs.CmdCopyImage(frame.cmd_buffer, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    src_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    src_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    src_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    src_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // GENERAL layout for external (DMA-BUF) access
    dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst_barrier.dstAccessMask = 0;
    dst_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dst_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barriers[0] = src_barrier;
    barriers[1] = dst_barrier;
    funcs.CmdPipelineBarrier(frame.cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT |
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 2, barriers);

    funcs.EndCommandBuffer(frame.cmd_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.cmd_buffer;

    VkResult res = funcs.QueueSubmit(queue, 1, &submit_info, frame.fence);
    if (res != VK_SUCCESS) {
        LAYER_DEBUG("QueueSubmit failed: %d", res);
        return;
    }

    // Synchronous wait ensures DMA-BUF contains valid data before IPC send
    funcs.WaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);
    funcs.ResetFences(device, 1, &frame.fence);
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

        static uint64_t frame_count = 0;
        bool sent = socket.send_texture(tex_data, swap->dmabuf_fd);
        if (++frame_count % 60 == 1) { // Log every 60 frames
            LAYER_DEBUG("Frame %" PRIu64 ": send=%s, fd=%d, %ux%u, modifier=0x%" PRIx64,
                        frame_count, sent ? "ok" : "FAIL", swap->dmabuf_fd, tex_data.width,
                        tex_data.height, tex_data.modifier);
        }
    }
}

// =============================================================================
// Cleanup
// =============================================================================

void CaptureManager::cleanup_swap_data(SwapData* swap, VkDeviceData* dev_data) {
    auto& funcs = dev_data->funcs;
    VkDevice device = swap->device;

    destroy_frame_resources(swap, dev_data);

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
}

} // namespace goggles::capture
