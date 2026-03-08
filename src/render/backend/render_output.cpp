#include "render_output.hpp"

#include "vulkan_error.hpp"

#include <algorithm>
#include <array>
#include <stb_image_write.h>
#include <thread>
#include <util/logging.hpp>

namespace goggles::render::backend_internal {

namespace {

struct ReadbackStagingBuffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    bool is_coherent = false;
};

auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& mem_props, uint32_t type_bits)
    -> uint32_t {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_bits & (1U << i)) {
            return i;
        }
    }
    return UINT32_MAX;
}

void destroy_render_finished_semaphores(vk::Device device, RenderOutput& output) {
    if (!device) {
        output.render_finished_sems.clear();
        return;
    }

    for (auto semaphore : output.render_finished_sems) {
        device.destroySemaphore(semaphore);
    }
    output.render_finished_sems.clear();
}

void destroy_image_views(vk::Device device, std::vector<vk::ImageView>& image_views) {
    if (!device) {
        image_views.clear();
        return;
    }

    for (auto image_view : image_views) {
        device.destroyImageView(image_view);
    }
    image_views.clear();
}

auto create_render_finished_semaphores(vk::Device device, size_t image_count)
    -> Result<std::vector<vk::Semaphore>> {
    std::vector<vk::Semaphore> render_finished_sems;
    render_finished_sems.resize(image_count);

    vk::SemaphoreCreateInfo sem_info{};
    for (auto& semaphore : render_finished_sems) {
        auto [result, new_semaphore] = device.createSemaphore(sem_info);
        if (result != vk::Result::eSuccess) {
            for (auto created_semaphore : render_finished_sems) {
                if (created_semaphore) {
                    device.destroySemaphore(created_semaphore);
                }
            }
            return make_error<std::vector<vk::Semaphore>>(
                ErrorCode::vulkan_init_failed, "Failed to create render finished semaphore");
        }
        semaphore = new_semaphore;
    }

    return render_finished_sems;
}

void destroy_fences(vk::Device device,
                    std::array<vk::Fence, RenderOutput::MAX_FRAMES_IN_FLIGHT>& fences) {
    if (!device) {
        fences = {};
        return;
    }

    for (auto& fence : fences) {
        if (fence) {
            device.destroyFence(fence);
            fence = nullptr;
        }
    }
}

void destroy_semaphores(vk::Device device,
                        std::array<vk::Semaphore, RenderOutput::MAX_FRAMES_IN_FLIGHT>& semaphores) {
    if (!device) {
        semaphores = {};
        return;
    }

    for (auto& semaphore : semaphores) {
        if (semaphore) {
            device.destroySemaphore(semaphore);
            semaphore = nullptr;
        }
    }
}

auto normalize_wait_stage(vk::Semaphore wait_semaphore, vk::PipelineStageFlags wait_stage)
    -> vk::PipelineStageFlags {
    if (!wait_semaphore || wait_stage != vk::PipelineStageFlags{}) {
        return wait_stage;
    }

    return vk::PipelineStageFlagBits::eColorAttachmentOutput;
}

void destroy_offscreen_target(vk::Device device, RenderOutput& output) {
    if (!device) {
        output.offscreen_view = nullptr;
        output.offscreen_image = nullptr;
        output.offscreen_memory = nullptr;
        output.offscreen_extent = vk::Extent2D{};
        return;
    }

    if (output.offscreen_view) {
        device.destroyImageView(output.offscreen_view);
        output.offscreen_view = nullptr;
    }
    if (output.offscreen_image) {
        device.destroyImage(output.offscreen_image);
        output.offscreen_image = nullptr;
    }
    if (output.offscreen_memory) {
        device.freeMemory(output.offscreen_memory);
        output.offscreen_memory = nullptr;
    }
    output.offscreen_extent = vk::Extent2D{};
}

auto create_readback_staging_buffer(vk::Device device, vk::PhysicalDevice physical_device,
                                    vk::DeviceSize size) -> Result<ReadbackStagingBuffer> {
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = vk::BufferUsageFlagBits::eTransferDst;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    auto [buffer_result, buffer] = device.createBuffer(buffer_info);
    if (buffer_result != vk::Result::eSuccess) {
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to create staging buffer: " +
                                                     vk::to_string(buffer_result));
    }

    auto buffer_requirements = device.getBufferMemoryRequirements(buffer);
    auto mem_props = physical_device.getMemoryProperties();
    uint32_t staging_mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((buffer_requirements.memoryTypeBits & (1U << i)) &&
            (mem_props.memoryTypes[i].propertyFlags &
             (vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)) ==
                (vk::MemoryPropertyFlagBits::eHostVisible |
                 vk::MemoryPropertyFlagBits::eHostCoherent)) {
            staging_mem_type = i;
            break;
        }
    }
    if (staging_mem_type == UINT32_MAX) {
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((buffer_requirements.memoryTypeBits & (1U << i)) &&
                (mem_props.memoryTypes[i].propertyFlags &
                 vk::MemoryPropertyFlagBits::eHostVisible)) {
                staging_mem_type = i;
                break;
            }
        }
    }
    if (staging_mem_type == UINT32_MAX) {
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "No host-visible memory type for staging buffer");
    }

    vk::MemoryAllocateInfo staging_alloc{};
    staging_alloc.allocationSize = buffer_requirements.size;
    staging_alloc.memoryTypeIndex = staging_mem_type;
    auto [alloc_result, memory] = device.allocateMemory(staging_alloc);
    if (alloc_result != vk::Result::eSuccess) {
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to allocate staging memory: " +
                                                     vk::to_string(alloc_result));
    }

    auto bind_result = device.bindBufferMemory(buffer, memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        device.freeMemory(memory);
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to bind staging buffer memory: " +
                                                     vk::to_string(bind_result));
    }

    ReadbackStagingBuffer staging{};
    staging.buffer = buffer;
    staging.memory = memory;
    staging.is_coherent = (mem_props.memoryTypes[staging_mem_type].propertyFlags &
                           vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags{};
    return staging;
}

void destroy_readback_staging_buffer(vk::Device device, ReadbackStagingBuffer& staging) {
    if (staging.memory) {
        device.freeMemory(staging.memory);
        staging.memory = nullptr;
    }
    if (staging.buffer) {
        device.destroyBuffer(staging.buffer);
        staging.buffer = nullptr;
    }
}

auto submit_readback_copy(vk::Device device, vk::Queue queue, vk::CommandBuffer cmd,
                          vk::Fence fence, vk::Image source, vk::Buffer dest, uint32_t width,
                          uint32_t height) -> Result<void> {
    auto reset_fence_result = device.resetFences(fence);
    if (reset_fence_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Fence reset failed: " + vk::to_string(reset_fence_result));
    }

    VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

    vk::ImageMemoryBarrier to_transfer{};
    to_transfer.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    to_transfer.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    to_transfer.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = source;
    to_transfer.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    to_transfer.subresourceRange.levelCount = 1;
    to_transfer.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, to_transfer);

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};
    cmd.copyImageToBuffer(source, vk::ImageLayout::eTransferSrcOptimal, dest, region);

    vk::ImageMemoryBarrier to_attachment{};
    to_attachment.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    to_attachment.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    to_attachment.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    to_attachment.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    to_attachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_attachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_attachment.image = source;
    to_attachment.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    to_attachment.subresourceRange.levelCount = 1;
    to_attachment.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                        to_attachment);

    VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");

    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    auto submit_result = queue.submit(submit_info, fence);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_result));
    }

    auto fence_wait = device.waitForFences(fence, VK_TRUE, UINT64_MAX);
    if (fence_wait != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost, "Fence wait failed during readback");
    }
    return {};
}

auto apply_present_wait(VulkanContext& context, RenderOutput& output, uint64_t present_value)
    -> Result<void> {
    if (output.target_fps == 0) {
        return {};
    }

    constexpr uint64_t MAX_TIMEOUT_NS = 1'000'000'000ULL;
    const uint64_t timeout_ns =
        std::min(MAX_TIMEOUT_NS, static_cast<uint64_t>(1'000'000'000ULL / output.target_fps));
    auto wait_result = static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkWaitForPresentKHR(
        context.device, output.swapchain, present_value, timeout_ns));
    if (wait_result == vk::Result::eSuccess || wait_result == vk::Result::eTimeout ||
        wait_result == vk::Result::eSuboptimalKHR) {
        return {};
    }
    if (wait_result == vk::Result::eErrorOutOfDateKHR ||
        wait_result == vk::Result::eErrorSurfaceLostKHR) {
        output.needs_resize = true;
        return {};
    }
    return make_error<void>(ErrorCode::vulkan_device_lost,
                            "vkWaitForPresentKHR failed: " + vk::to_string(wait_result));
}

void throttle_present(RenderOutput& output) {
    if (output.target_fps == 0) {
        return;
    }

    using clock = std::chrono::steady_clock;
    const auto frame_duration = std::chrono::nanoseconds(1'000'000'000ULL / output.target_fps);

    if (output.last_present_time.time_since_epoch().count() == 0) {
        output.last_present_time = clock::now();
        return;
    }

    const auto next_frame = output.last_present_time + frame_duration;
    const auto now = clock::now();
    if (now < next_frame) {
        std::this_thread::sleep_until(next_frame);
        output.last_present_time = next_frame;
    } else {
        output.last_present_time = now;
    }
}

} // namespace

auto RenderOutput::create_swapchain(VulkanContext& context, uint32_t width, uint32_t height,
                                    vk::Format preferred_format) -> Result<void> {
    auto& physical_device = context.physical_device;
    auto& device = context.device;
    auto& surface = context.surface;
    auto& present_wait_supported = context.present_wait_supported;

    auto [cap_result, capabilities] = physical_device.getSurfaceCapabilitiesKHR(surface);
    if (cap_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to query surface capabilities");
    }

    auto [fmt_result, formats] = physical_device.getSurfaceFormatsKHR(surface);
    if (fmt_result != vk::Result::eSuccess || formats.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to query surface formats");
    }

    vk::SurfaceFormatKHR chosen_format = formats[0];
    for (const auto& format : formats) {
        if (format.format == preferred_format &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            chosen_format = format;
            break;
        }
    }

    vk::Extent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width =
            std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info{};
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
    create_info.imageSharingMode = vk::SharingMode::eExclusive;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    auto [pm_result, present_modes] = physical_device.getSurfacePresentModesKHR(surface);
    vk::PresentModeKHR chosen_mode = vk::PresentModeKHR::eFifo;
    bool mailbox_supported = false;
    if (pm_result == vk::Result::eSuccess) {
        for (auto mode : present_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                mailbox_supported = true;
                break;
            }
        }
    }

    if (present_wait_supported) {
        chosen_mode = vk::PresentModeKHR::eFifo;
    } else if (mailbox_supported) {
        chosen_mode = vk::PresentModeKHR::eMailbox;
    }

    create_info.presentMode = chosen_mode;
    create_info.clipped = VK_TRUE;

    auto [swapchain_result, new_swapchain] = device.createSwapchainKHR(create_info);
    if (swapchain_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create swapchain: " + vk::to_string(swapchain_result));
    }

    std::vector<vk::ImageView> new_swapchain_image_views;
    std::vector<vk::Semaphore> new_render_finished_sems;
    const auto cleanup_new_swapchain = [&]() {
        destroy_image_views(device, new_swapchain_image_views);
        for (auto semaphore : new_render_finished_sems) {
            if (semaphore) {
                device.destroySemaphore(semaphore);
            }
        }
        new_render_finished_sems.clear();
        if (new_swapchain) {
            device.destroySwapchainKHR(new_swapchain);
            new_swapchain = nullptr;
        }
    };

    auto [image_result, images] = device.getSwapchainImagesKHR(new_swapchain);
    if (image_result != vk::Result::eSuccess) {
        cleanup_new_swapchain();
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to get swapchain images");
    }

    new_swapchain_image_views.reserve(images.size());
    for (auto image : images) {
        vk::ImageViewCreateInfo view_info{};
        view_info.image = image;
        view_info.viewType = vk::ImageViewType::e2D;
        view_info.format = chosen_format.format;
        view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        auto [view_result, view] = device.createImageView(view_info);
        if (view_result != vk::Result::eSuccess) {
            cleanup_new_swapchain();
            return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create image view");
        }
        new_swapchain_image_views.push_back(view);
    }

    auto render_finished_result = create_render_finished_semaphores(device, images.size());
    if (!render_finished_result) {
        cleanup_new_swapchain();
        return make_error<void>(render_finished_result.error().code,
                                render_finished_result.error().message,
                                render_finished_result.error().location);
    }
    new_render_finished_sems = std::move(render_finished_result.value());

    cleanup_swapchain(context);

    swapchain = new_swapchain;
    swapchain_images = std::move(images);
    swapchain_image_views = std::move(new_swapchain_image_views);
    render_finished_sems = std::move(new_render_finished_sems);
    swapchain_format = chosen_format.format;
    swapchain_extent = extent;
    headless = false;

    present_id = 0;
    last_present_time = std::chrono::steady_clock::time_point{};

    GOGGLES_LOG_DEBUG("Swapchain created: {}x{}, {} images", extent.width, extent.height,
                      swapchain_images.size());
    return {};
}

void RenderOutput::cleanup_swapchain(VulkanContext& context) {
    auto& device = context.device;

    destroy_render_finished_semaphores(device, *this);

    if (device) {
        for (auto view : swapchain_image_views) {
            device.destroyImageView(view);
        }
        if (swapchain) {
            device.destroySwapchainKHR(swapchain);
        }
    }

    swapchain_image_views.clear();
    swapchain_images.clear();
    swapchain = nullptr;
}

auto RenderOutput::create_command_resources(VulkanContext& context) -> Result<void> {
    auto& device = context.device;
    auto& graphics_queue_family = context.graphics_queue_family;

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = graphics_queue_family;

    auto [pool_result, pool] = device.createCommandPool(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create command pool");
    }

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    auto [alloc_result, buffers] = device.allocateCommandBuffers(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        device.destroyCommandPool(pool);
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate command buffers");
    }

    command_pool = pool;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames[i].command_buffer = buffers[i];
    }

    GOGGLES_LOG_DEBUG("Command pool and {} buffers created", MAX_FRAMES_IN_FLIGHT);
    return {};
}

auto RenderOutput::create_sync_objects(VulkanContext& context) -> Result<void> {
    auto& device = context.device;

    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
    vk::SemaphoreCreateInfo sem_info{};

    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> new_fences{};
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> new_image_available_sems{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        {
            auto [result, fence] = device.createFence(fence_info);
            if (result != vk::Result::eSuccess) {
                destroy_semaphores(device, new_image_available_sems);
                destroy_fences(device, new_fences);
                return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create fence");
            }
            new_fences[i] = fence;
        }
        {
            auto [result, semaphore] = device.createSemaphore(sem_info);
            if (result != vk::Result::eSuccess) {
                destroy_semaphores(device, new_image_available_sems);
                destroy_fences(device, new_fences);
                return make_error<void>(ErrorCode::vulkan_init_failed,
                                        "Failed to create semaphore");
            }
            new_image_available_sems[i] = semaphore;
        }
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames[i].in_flight_fence = new_fences[i];
        frames[i].image_available_sem = new_image_available_sems[i];
    }

    GOGGLES_LOG_DEBUG("Sync objects created");
    return {};
}

auto RenderOutput::create_sync_objects_headless(VulkanContext& context) -> Result<void> {
    auto& device = context.device;

    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> new_fences{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto [result, fence] = device.createFence(fence_info);
        if (result != vk::Result::eSuccess) {
            destroy_fences(device, new_fences);
            return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create fence");
        }
        new_fences[i] = fence;
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames[i].in_flight_fence = new_fences[i];
    }

    headless = true;
    current_frame = 0;
    GOGGLES_LOG_DEBUG("Headless sync objects created");
    return {};
}

auto RenderOutput::create_offscreen_image(VulkanContext& context, vk::Extent2D source_resolution)
    -> Result<void> {
    auto& device = context.device;
    auto& physical_device = context.physical_device;

    uint32_t width = source_resolution.width;
    uint32_t height = source_resolution.height;
    if (width == 0 || height == 0) {
        width = 1920;
        height = 1080;
    }

    vk::Image new_offscreen_image;
    vk::DeviceMemory new_offscreen_memory;
    vk::ImageView new_offscreen_view;

    const auto cleanup_new_offscreen_target = [&]() {
        if (new_offscreen_view) {
            device.destroyImageView(new_offscreen_view);
            new_offscreen_view = nullptr;
        }
        if (new_offscreen_image) {
            device.destroyImage(new_offscreen_image);
            new_offscreen_image = nullptr;
        }
        if (new_offscreen_memory) {
            device.freeMemory(new_offscreen_memory);
            new_offscreen_memory = nullptr;
        }
    };

    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = vk::Format::eR8G8B8A8Unorm;
    image_info.extent = vk::Extent3D{width, height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    auto [image_result, image] = device.createImage(image_info);
    if (image_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create offscreen image: " + vk::to_string(image_result));
    }
    new_offscreen_image = image;

    auto mem_requirements = device.getImageMemoryRequirements(new_offscreen_image);
    auto mem_props = physical_device.getMemoryProperties();
    const uint32_t mem_type = find_memory_type(mem_props, mem_requirements.memoryTypeBits);
    if (mem_type == UINT32_MAX) {
        cleanup_new_offscreen_target();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable memory type for offscreen image");
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = mem_type;

    auto [memory_result, memory] = device.allocateMemory(alloc_info);
    if (memory_result != vk::Result::eSuccess) {
        cleanup_new_offscreen_target();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate offscreen memory: " +
                                    vk::to_string(memory_result));
    }
    new_offscreen_memory = memory;

    auto bind_result = device.bindImageMemory(new_offscreen_image, new_offscreen_memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        cleanup_new_offscreen_target();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to bind offscreen image memory: " +
                                    vk::to_string(bind_result));
    }

    vk::ImageViewCreateInfo view_info{};
    view_info.image = new_offscreen_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk::Format::eR8G8B8A8Unorm;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = device.createImageView(view_info);
    if (view_result != vk::Result::eSuccess) {
        cleanup_new_offscreen_target();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create offscreen image view: " +
                                    vk::to_string(view_result));
    }
    new_offscreen_view = view;

    destroy_offscreen_target(device, *this);

    offscreen_image = new_offscreen_image;
    offscreen_memory = new_offscreen_memory;
    offscreen_view = new_offscreen_view;
    offscreen_extent = vk::Extent2D{width, height};
    swapchain_format = vk::Format::eR8G8B8A8Unorm;
    swapchain_extent = offscreen_extent;
    headless = true;

    GOGGLES_LOG_DEBUG("Offscreen image created: {}x{} R8G8B8A8Unorm", width, height);
    return {};
}

void RenderOutput::destroy(VulkanContext& context) {
    auto& device = context.device;

    destroy_offscreen_target(device, *this);

    if (device) {
        for (auto& frame : frames) {
            device.destroyFence(frame.in_flight_fence);
            if (frame.image_available_sem) {
                device.destroySemaphore(frame.image_available_sem);
            }
        }
    }
    frames = {};

    cleanup_swapchain(context);

    if (device && command_pool) {
        device.destroyCommandPool(command_pool);
    }
    command_pool = nullptr;

    swapchain_format = vk::Format::eUndefined;
    swapchain_extent = vk::Extent2D{};
    current_frame = 0;
    headless = false;
    needs_resize = false;
    target_fps = 0;
    present_id = 0;
    last_present_time = std::chrono::steady_clock::time_point{};
}

void RenderOutput::wait_all_frames(VulkanContext& context) {
    auto& device = context.device;
    if (!device) {
        return;
    }

    std::vector<vk::Fence> fences;
    fences.reserve(MAX_FRAMES_IN_FLIGHT);
    for (const auto& frame : frames) {
        if (frame.in_flight_fence) {
            fences.push_back(frame.in_flight_fence);
        }
    }

    if (fences.empty()) {
        return;
    }

    auto result = device.waitForFences(fences, VK_TRUE, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        GOGGLES_LOG_WARN("wait_all_frames failed: {}", vk::to_string(result));
    }
}

auto RenderOutput::acquire_next_image(VulkanContext& context) -> Result<uint32_t> {
    auto& device = context.device;
    auto& frame = frames[current_frame];

    auto wait_result = device.waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost, "Fence wait failed");
    }

    uint32_t image_index = 0;
    auto result = static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, frame.image_available_sem, nullptr, &image_index));

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        needs_resize = true;
        if (result == vk::Result::eErrorOutOfDateKHR) {
            return make_error<uint32_t>(ErrorCode::vulkan_init_failed, "Swapchain out of date");
        }
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost,
                                    "Failed to acquire swapchain image: " + vk::to_string(result));
    }

    auto reset_result = device.resetFences(frame.in_flight_fence);
    if (reset_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost,
                                    "Fence reset failed: " + vk::to_string(reset_result));
    }

    return image_index;
}

auto RenderOutput::prepare_headless_frame(VulkanContext& context) -> Result<vk::CommandBuffer> {
    auto& device = context.device;
    auto& frame = frames[0];

    current_frame = 0;

    auto wait_result = device.waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<vk::CommandBuffer>(ErrorCode::vulkan_device_lost, "Fence wait failed");
    }

    auto reset_result = device.resetFences(frame.in_flight_fence);
    if (reset_result != vk::Result::eSuccess) {
        return make_error<vk::CommandBuffer>(ErrorCode::vulkan_device_lost,
                                             "Fence reset failed: " + vk::to_string(reset_result));
    }

    return frame.command_buffer;
}

auto RenderOutput::submit_and_present(VulkanContext& context, uint32_t image_index,
                                      vk::Semaphore acquire_wait_semaphore,
                                      vk::PipelineStageFlags acquire_wait_stage) -> Result<void> {
    auto& graphics_queue = context.graphics_queue;
    auto& present_wait_supported = context.present_wait_supported;
    const vk::PipelineStageFlags normalized_acquire_wait_stage =
        normalize_wait_stage(acquire_wait_semaphore, acquire_wait_stage);

    auto& frame = frames[current_frame];
    vk::Semaphore render_finished_sem = render_finished_sems[image_index];

    std::array<vk::Semaphore, 2> wait_semaphores{};
    std::array<vk::PipelineStageFlags, 2> wait_stages{};
    uint32_t wait_count = 1;
    wait_semaphores[0] = frame.image_available_sem;
    wait_stages[0] = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    if (acquire_wait_semaphore) {
        wait_semaphores[wait_count] = acquire_wait_semaphore;
        wait_stages[wait_count] = normalized_acquire_wait_stage;
        ++wait_count;
    }

    vk::SubmitInfo submit_info{};
    submit_info.waitSemaphoreCount = wait_count;
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_sem;

    auto submit_result = graphics_queue.submit(submit_info, frame.in_flight_fence);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_result));
    }

    vk::PresentInfoKHR present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_sem;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;

    vk::PresentIdKHR present_info_id{};
    uint64_t present_value = 0;
    if (present_wait_supported) {
        present_value = ++present_id;
        present_info_id.swapchainCount = 1;
        present_info_id.pPresentIds = &present_value;
        present_info.pNext = &present_info_id;
    }

    auto present_result = graphics_queue.presentKHR(present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        needs_resize = true;
    } else if (present_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Present failed: " + vk::to_string(present_result));
    }

    if (target_fps == 0) {
    } else if (present_wait_supported && present_value > 0) {
        auto wait_result = apply_present_wait(context, *this, present_value);
        if (!wait_result) {
            return wait_result;
        }
    } else {
        throttle_present(*this);
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return {};
}

auto RenderOutput::submit_headless(VulkanContext& context, vk::Semaphore acquire_wait_semaphore,
                                   vk::PipelineStageFlags acquire_wait_stage) -> Result<void> {
    auto& graphics_queue = context.graphics_queue;
    const vk::PipelineStageFlags normalized_acquire_wait_stage =
        normalize_wait_stage(acquire_wait_semaphore, acquire_wait_stage);
    auto& frame = frames[0];
    vk::SubmitInfo submit_info{};
    if (acquire_wait_semaphore) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &acquire_wait_semaphore;
        submit_info.pWaitDstStageMask = &normalized_acquire_wait_stage;
    }
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;

    auto submit_result = graphics_queue.submit(submit_info, frame.in_flight_fence);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_result));
    }

    return {};
}

auto RenderOutput::readback_to_png(VulkanContext& context, const std::filesystem::path& output)
    -> Result<void> {
    auto& device = context.device;
    auto& physical_device = context.physical_device;
    auto& graphics_queue = context.graphics_queue;

    if (!headless || !offscreen_image) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "readback_to_png requires headless mode");
    }

    auto& frame = frames[0];
    auto wait_result = device.waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost, "Fence wait failed before readback");
    }

    const uint32_t width = offscreen_extent.width;
    const uint32_t height = offscreen_extent.height;
    const vk::DeviceSize buffer_size = static_cast<vk::DeviceSize>(width) * height * 4;

    auto staging_result = create_readback_staging_buffer(device, physical_device, buffer_size);
    if (!staging_result) {
        return make_error<void>(staging_result.error().code, staging_result.error().message,
                                staging_result.error().location);
    }
    auto staging = staging_result.value();

    auto copy_result =
        submit_readback_copy(device, graphics_queue, frame.command_buffer, frame.in_flight_fence,
                             offscreen_image, staging.buffer, width, height);
    if (!copy_result) {
        destroy_readback_staging_buffer(device, staging);
        return make_error<void>(copy_result.error().code, copy_result.error().message,
                                copy_result.error().location);
    }

    auto [map_result, data] = device.mapMemory(staging.memory, 0, buffer_size);
    if (map_result != vk::Result::eSuccess) {
        destroy_readback_staging_buffer(device, staging);
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Failed to map staging memory: " + vk::to_string(map_result));
    }

    if (!staging.is_coherent) {
        vk::MappedMemoryRange range{};
        range.memory = staging.memory;
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;
        auto invalidate_result = device.invalidateMappedMemoryRanges(range);
        if (invalidate_result != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("invalidateMappedMemoryRanges failed: {}",
                             vk::to_string(invalidate_result));
        }
    }

    const int png_result =
        stbi_write_png(output.c_str(), static_cast<int>(width), static_cast<int>(height), 4, data,
                       static_cast<int>(width * 4));

    device.unmapMemory(staging.memory);
    destroy_readback_staging_buffer(device, staging);

    if (png_result == 0) {
        return make_error<void>(ErrorCode::file_write_failed,
                                "stbi_write_png failed for: " + output.string());
    }

    GOGGLES_LOG_INFO("PNG written: {} ({}x{})", output.string(), width, height);
    return {};
}

} // namespace goggles::render::backend_internal
