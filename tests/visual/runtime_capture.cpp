#include "runtime_capture.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <render/chain/chain_runtime.hpp>
#include <render/chain/vulkan_context.hpp>
#include <stb/stb_image_write.h>
#include <util/diagnostics/diagnostic_policy.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace goggles::test {

namespace {

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_visual_capture_tests";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
            return;
        }

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{instance});

        uint32_t device_count = 0;
        if (vkEnumeratePhysicalDevices(instance, &device_count, nullptr) != VK_SUCCESS ||
            device_count == 0) {
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        if (vkEnumeratePhysicalDevices(instance, &device_count, devices.data()) != VK_SUCCESS) {
            return;
        }

        for (const auto candidate : devices) {
            uint32_t family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            for (uint32_t family = 0; family < family_count; ++family) {
                if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                    physical_device = candidate;
                    queue_family_index = family;
                    break;
                }
            }
            if (physical_device != VK_NULL_HANDLE) {
                break;
            }
        }

        if (physical_device == VK_NULL_HANDLE) {
            return;
        }

        float queue_priority = 1.0F;
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;

        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = queue_family_index;
        queue_info.queueCount = 1U;
        queue_info.pQueuePriorities = &queue_priority;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1U;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.pNext = &features13;
        if (vkCreateDevice(physical_device, &device_info, nullptr, &device) != VK_SUCCESS) {
            return;
        }

        vkGetDeviceQueue(device, queue_family_index, 0U, &queue);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device{device});

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = queue_family_index;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
            return;
        }
    }

    ~VulkanRuntimeFixture() {
        if (device != VK_NULL_HANDLE) {
            if (command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, command_pool, nullptr);
            }
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }

    [[nodiscard]] auto available() const -> bool {
        return device != VK_NULL_HANDLE && queue != VK_NULL_HANDLE &&
               command_pool != VK_NULL_HANDLE;
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queue_family_index = UINT32_MAX;
    VkCommandPool command_pool = VK_NULL_HANDLE;
};

struct ImageGuard {
    ImageGuard() = default;
    ImageGuard(const ImageGuard&) = delete;
    auto operator=(const ImageGuard&) -> ImageGuard& = delete;

    ImageGuard(ImageGuard&& other) noexcept { *this = std::move(other); }

    auto operator=(ImageGuard&& other) noexcept -> ImageGuard& {
        if (this == &other) {
            return *this;
        }

        cleanup();
        device = other.device;
        image = other.image;
        memory = other.memory;
        view = other.view;

        other.device = VK_NULL_HANDLE;
        other.image = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        return *this;
    }

    ~ImageGuard() { cleanup(); }

    void cleanup() {
        if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE && image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

struct BufferGuard {
    BufferGuard() = default;
    BufferGuard(const BufferGuard&) = delete;
    auto operator=(const BufferGuard&) -> BufferGuard& = delete;

    BufferGuard(BufferGuard&& other) noexcept { *this = std::move(other); }

    auto operator=(BufferGuard&& other) noexcept -> BufferGuard& {
        if (this == &other) {
            return *this;
        }

        cleanup();
        device = other.device;
        buffer = other.buffer;
        memory = other.memory;

        other.device = VK_NULL_HANDLE;
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        return *this;
    }

    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    ~BufferGuard() { cleanup(); }

    void cleanup() {
        if (device != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }
};

struct BufferSpec {
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties;
};

struct ImageTransition {
    VkImageLayout old_layout;
    VkImageLayout new_layout;
};

auto find_memory_type(VkPhysicalDevice physical_device, uint32_t type_bits,
                      VkMemoryPropertyFlags properties) -> std::optional<uint32_t> {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        if ((type_bits & (1u << index)) != 0u &&
            (memory_properties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }
    return std::nullopt;
}

auto create_image(VkDevice device, VkPhysicalDevice physical_device, VkExtent2D extent,
                  VkImageUsageFlags usage) -> std::optional<ImageGuard> {
    ImageGuard guard{};
    guard.device = device;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent = {.width = extent.width, .height = extent.height, .depth = 1u};
    image_info.mipLevels = 1u;
    image_info.arrayLayers = 1u;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &image_info, nullptr, &guard.image) != VK_SUCCESS) {
        return std::nullopt;
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, guard.image, &requirements);
    const auto memory_type = find_memory_type(physical_device, requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memory_type.has_value()) {
        return std::nullopt;
    }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = *memory_type;
    if (vkAllocateMemory(device, &alloc_info, nullptr, &guard.memory) != VK_SUCCESS) {
        return std::nullopt;
    }
    if (vkBindImageMemory(device, guard.image, guard.memory, 0u) != VK_SUCCESS) {
        return std::nullopt;
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = guard.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1u;
    view_info.subresourceRange.layerCount = 1u;
    if (vkCreateImageView(device, &view_info, nullptr, &guard.view) != VK_SUCCESS) {
        return std::nullopt;
    }

    return guard;
}

auto create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size,
                   BufferSpec spec) -> std::optional<BufferGuard> {
    BufferGuard guard{};
    guard.device = device;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = spec.usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &guard.buffer) != VK_SUCCESS) {
        return std::nullopt;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, guard.buffer, &requirements);
    const auto memory_type =
        find_memory_type(physical_device, requirements.memoryTypeBits, spec.properties);
    if (!memory_type.has_value()) {
        return std::nullopt;
    }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = *memory_type;
    if (vkAllocateMemory(device, &alloc_info, nullptr, &guard.memory) != VK_SUCCESS) {
        return std::nullopt;
    }
    if (vkBindBufferMemory(device, guard.buffer, guard.memory, 0u) != VK_SUCCESS) {
        return std::nullopt;
    }

    return guard;
}

void transition_image(VkCommandBuffer command_buffer, VkImage image, ImageTransition transition) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = transition.old_layout;
    barrier.newLayout = transition.new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (transition.new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (transition.new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (transition.new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (transition.new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0u, 0u, nullptr, 0u, nullptr, 1u,
                         &barrier);
}

auto allocate_command_buffer(VkDevice device, VkCommandPool command_pool)
    -> std::optional<VkCommandBuffer> {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        return std::nullopt;
    }
    return command_buffer;
}

auto create_fence(VkDevice device) -> std::optional<VkFence> {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        return std::nullopt;
    }
    return fence;
}

auto submit_and_wait(VkDevice device, VkQueue queue, VkCommandBuffer command_buffer, VkFence fence)
    -> bool {
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer;
    if (vkQueueSubmit(queue, 1u, &submit_info, fence) != VK_SUCCESS) {
        return false;
    }
    return vkWaitForFences(device, 1u, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}

auto make_quadrant_pixels(VkExtent2D extent) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(extent.width) * extent.height * 4u,
                                     255u);
    for (uint32_t y = 0; y < extent.height; ++y) {
        for (uint32_t x = 0; x < extent.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * extent.width + x) * 4u;
            const bool right = x >= extent.width / 2u;
            const bool bottom = y >= extent.height / 2u;

            if (!right && !bottom) {
                pixels[offset + 0u] = 255u;
                pixels[offset + 1u] = 0u;
                pixels[offset + 2u] = 0u;
            } else if (right && !bottom) {
                pixels[offset + 0u] = 0u;
                pixels[offset + 1u] = 255u;
                pixels[offset + 2u] = 0u;
            } else if (!right && bottom) {
                pixels[offset + 0u] = 0u;
                pixels[offset + 1u] = 0u;
                pixels[offset + 2u] = 255u;
            } else {
                pixels[offset + 0u] = 255u;
                pixels[offset + 1u] = 255u;
                pixels[offset + 2u] = 255u;
            }
        }
    }
    return pixels;
}

auto upload_quadrant_source(const VulkanRuntimeFixture& fixture, const ImageGuard& source,
                            VkExtent2D extent) -> bool {
    const auto pixels = make_quadrant_pixels(extent);
    const auto buffer_size = static_cast<VkDeviceSize>(pixels.size());
    auto staging = create_buffer(
        fixture.device, fixture.physical_device, buffer_size,
        {.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
         .properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});
    if (!staging.has_value()) {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(fixture.device, staging->memory, 0u, buffer_size, 0u, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, pixels.data(), pixels.size());
    vkUnmapMemory(fixture.device, staging->memory);

    const auto command_buffer = allocate_command_buffer(fixture.device, fixture.command_pool);
    const auto fence = create_fence(fixture.device);
    if (!command_buffer.has_value() || !fence.has_value()) {
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(*command_buffer, &begin_info) != VK_SUCCESS) {
        return false;
    }

    transition_image(*command_buffer, source.image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL});

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1u};
    vkCmdCopyBufferToImage(*command_buffer, staging->buffer, source.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

    transition_image(*command_buffer, source.image,
                     {.old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    if (vkEndCommandBuffer(*command_buffer) != VK_SUCCESS) {
        return false;
    }

    const bool submitted = submit_and_wait(fixture.device, fixture.queue, *command_buffer, *fence);
    vkDestroyFence(fixture.device, *fence, nullptr);
    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &*command_buffer);
    return submitted;
}

auto readback_image_png(const VulkanRuntimeFixture& fixture, VkImage image, VkExtent2D extent,
                        VkImageLayout current_layout, const std::filesystem::path& output_path)
    -> bool {
    const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(extent.width) * extent.height * 4u;
    auto staging = create_buffer(
        fixture.device, fixture.physical_device, buffer_size,
        {.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
         .properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});
    if (!staging.has_value()) {
        return false;
    }

    const auto command_buffer = allocate_command_buffer(fixture.device, fixture.command_pool);
    const auto fence = create_fence(fixture.device);
    if (!command_buffer.has_value() || !fence.has_value()) {
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(*command_buffer, &begin_info) != VK_SUCCESS) {
        return false;
    }

    transition_image(
        *command_buffer, image,
        {.old_layout = current_layout, .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL});

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1u};
    vkCmdCopyImageToBuffer(*command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging->buffer, 1u, &region);

    transition_image(
        *command_buffer, image,
        {.old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .new_layout = current_layout});

    if (vkEndCommandBuffer(*command_buffer) != VK_SUCCESS) {
        return false;
    }

    const bool submitted = submit_and_wait(fixture.device, fixture.queue, *command_buffer, *fence);
    vkDestroyFence(fixture.device, *fence, nullptr);
    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &*command_buffer);
    if (!submitted) {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(fixture.device, staging->memory, 0u, buffer_size, 0u, &mapped) != VK_SUCCESS) {
        return false;
    }
    const int write_result = stbi_write_png(
        output_path.string().c_str(), static_cast<int>(extent.width),
        static_cast<int>(extent.height), 4, mapped, static_cast<int>(extent.width * 4u));
    vkUnmapMemory(fixture.device, staging->memory);
    return write_result != 0;
}

void emit_capture_event(diagnostics::DiagnosticSession& session, uint32_t pass_ordinal,
                        uint32_t frame_index, const std::filesystem::path& image_path) {
    diagnostics::DiagnosticEvent event{};
    event.severity = diagnostics::Severity::info;
    event.original_severity = diagnostics::Severity::info;
    event.category = diagnostics::Category::capture;
    event.localization = {.pass_ordinal = pass_ordinal, .stage = "capture", .resource = "image"};
    event.frame_index = frame_index;
    event.message = "Captured frame evidence";
    event.evidence = diagnostics::CaptureEvidence{
        .pass_ordinal = pass_ordinal,
        .frame_index = frame_index,
        .image_ref = image_path.string(),
    };
    session.emit(std::move(event));
}

auto create_capture_runtime(const VulkanRuntimeFixture& fixture, const RuntimeCapturePlan& plan,
                            const std::filesystem::path& cache_dir)
    -> Result<std::unique_ptr<render::ChainRuntime>> {
    render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    auto runtime_result = render::ChainRuntime::create(
        vk_ctx, vk::Format::eR8G8B8A8Unorm, 2u,
        {.shader_dir = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders",
         .cache_dir = cache_dir},
        {plan.source_extent.width, plan.source_extent.height});
    if (!runtime_result) {
        return make_error<std::unique_ptr<render::ChainRuntime>>(runtime_result.error().code,
                                                                 runtime_result.error().message,
                                                                 runtime_result.error().location);
    }

    return std::move(*runtime_result);
}

void init_capture_result(RuntimeCaptureResult* result, diagnostics::DiagnosticPolicy policy) {
    auto session = diagnostics::DiagnosticSession::create(policy);
    auto harness = std::make_unique<diagnostics::TestHarnessSink>();
    result->sink = harness.get();
    session->register_sink(std::move(harness));
    result->session = std::move(session);
    result->temp_dir = std::make_unique<TempDir>();
}

auto record_runtime_frame(render::ChainRuntime& runtime, const VulkanRuntimeFixture& fixture,
                          const ImageGuard& source, const ImageGuard& target,
                          const RuntimeCapturePlan& plan, uint32_t frame, bool* target_initialized)
    -> Result<void> {
    const auto command_buffer = allocate_command_buffer(fixture.device, fixture.command_pool);
    const auto fence = create_fence(fixture.device);
    if (!command_buffer.has_value() || !fence.has_value()) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate visual capture command resources");
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(*command_buffer, &begin_info) != VK_SUCCESS) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Failed to begin visual capture command buffer");
    }

    if (!*target_initialized) {
        transition_image(*command_buffer, target.image,
                         {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                          .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        *target_initialized = true;
    }

    runtime.record(
        vk::CommandBuffer{*command_buffer}, vk::Image{source.image}, vk::ImageView{source.view},
        {plan.source_extent.width, plan.source_extent.height}, vk::ImageView{target.view},
        {plan.target_extent.width, plan.target_extent.height}, frame % 2u, ScaleMode::fit, 0u);

    if (vkEndCommandBuffer(*command_buffer) != VK_SUCCESS ||
        !submit_and_wait(fixture.device, fixture.queue, *command_buffer, *fence)) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Failed to execute visual capture frame");
    }

    vkDestroyFence(fixture.device, *fence, nullptr);
    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &*command_buffer);
    return {};
}

auto capture_requested_frame(RuntimeCaptureResult& result, render::ChainRuntime& runtime,
                             const VulkanRuntimeFixture& fixture, const RuntimeCapturePlan& plan,
                             uint32_t frame, VkImage target_image) -> Result<void> {
    const auto final_path =
        result.temp_dir->path / (plan.preset_name + "_frame" + std::to_string(frame) + ".png");
    if (!readback_image_png(
            fixture, target_image,
            {.width = plan.target_extent.width, .height = plan.target_extent.height},
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, final_path)) {
        return make_error<void>(ErrorCode::file_write_failed, "Failed to capture final frame PNG");
    }
    result.final_frames.emplace(frame, final_path);
    emit_capture_event(*result.session, diagnostics::LocalizationKey::CHAIN_LEVEL, frame,
                       final_path);

    for (const auto pass_ordinal : plan.intermediate_pass_ordinals) {
        const auto pass_path =
            result.temp_dir->path / (plan.preset_name + "_pass" + std::to_string(pass_ordinal) +
                                     "_frame" + std::to_string(frame) + ".png");
        const auto capture_result = runtime.capture_pass_output(pass_ordinal);
        if (!capture_result) {
            return make_error<void>(capture_result.error().code, capture_result.error().message,
                                    capture_result.error().location);
        }
        const auto write_result =
            stbi_write_png(pass_path.string().c_str(), static_cast<int>(capture_result->width),
                           static_cast<int>(capture_result->height), 4, capture_result->rgba.data(),
                           static_cast<int>(capture_result->width * 4u));
        if (write_result == 0) {
            return make_error<void>(ErrorCode::file_write_failed,
                                    "Failed to write intermediate capture PNG");
        }
        result.intermediate_frames.emplace(pass_frame_key(pass_ordinal, frame), pass_path);
        emit_capture_event(*result.session, pass_ordinal, frame, pass_path);
    }

    return {};
}

} // namespace

TempDir::TempDir() {
    path = std::filesystem::temp_directory_path() /
           ("goggles_visual_capture_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
}

TempDir::~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

auto pass_frame_key(uint32_t pass_ordinal, uint32_t frame_index) -> std::string {
    return std::to_string(pass_ordinal) + ":" + std::to_string(frame_index);
}

auto capture_runtime_outputs(const RuntimeCapturePlan& plan) -> Result<RuntimeCaptureResult> {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        return make_error<RuntimeCaptureResult>(
            ErrorCode::vulkan_init_failed,
            "No Vulkan graphics device available for visual capture");
    }
    if (plan.preset_path.empty() || !std::filesystem::exists(plan.preset_path)) {
        return make_error<RuntimeCaptureResult>(ErrorCode::invalid_data,
                                                "Preset not found: " + plan.preset_path.string());
    }
    if (plan.frame_indices.empty()) {
        return make_error<RuntimeCaptureResult>(ErrorCode::invalid_data,
                                                "At least one frame index is required");
    }

    auto source =
        create_image(fixture.device, fixture.physical_device,
                     {.width = plan.source_extent.width, .height = plan.source_extent.height},
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto target =
        create_image(fixture.device, fixture.physical_device,
                     {.width = plan.target_extent.width, .height = plan.target_extent.height},
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!source.has_value() || !target.has_value()) {
        return make_error<RuntimeCaptureResult>(ErrorCode::vulkan_init_failed,
                                                "Failed to create visual capture images");
    }
    if (!upload_quadrant_source(
            fixture, *source,
            {.width = plan.source_extent.width, .height = plan.source_extent.height})) {
        return make_error<RuntimeCaptureResult>(ErrorCode::vulkan_device_lost,
                                                "Failed to upload visual capture source image");
    }

    const auto cache_dir = std::filesystem::temp_directory_path() /
                           ("goggles_visual_capture_cache_" + plan.preset_name);
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = create_capture_runtime(fixture, plan, cache_dir);
    if (!runtime_result) {
        return make_error<RuntimeCaptureResult>(runtime_result.error().code,
                                                runtime_result.error().message,
                                                runtime_result.error().location);
    }
    auto runtime = std::move(*runtime_result);

    diagnostics::DiagnosticPolicy policy{};
    policy.capture_mode = diagnostics::CaptureMode::investigate;
    runtime->create_diagnostic_session(policy);

    const auto load_result = runtime->load_preset(plan.preset_path);
    if (!load_result) {
        runtime->shutdown();
        return make_error<RuntimeCaptureResult>(
            load_result.error().code, load_result.error().message, load_result.error().location);
    }

    RuntimeCaptureResult result;
    init_capture_result(&result, policy);

    for (const auto& [name, value] : plan.control_overrides) {
        const auto controls = runtime->list_controls();
        const auto control_it =
            std::find_if(controls.begin(), controls.end(),
                         [&name](const auto& control) { return control.name == name; });
        if (control_it == controls.end()) {
            runtime->shutdown();
            return make_error<RuntimeCaptureResult>(ErrorCode::invalid_data,
                                                    "Unknown runtime control: " + name);
        }
        if (!runtime->set_control_value(control_it->control_id, value)) {
            runtime->shutdown();
            return make_error<RuntimeCaptureResult>(ErrorCode::invalid_data,
                                                    "Failed to set runtime control: " + name);
        }
    }

    const auto max_frame = *std::max_element(plan.frame_indices.begin(), plan.frame_indices.end());
    bool target_initialized = false;

    for (uint32_t frame = 0; frame <= max_frame; ++frame) {
        const auto record_result = record_runtime_frame(*runtime, fixture, *source, *target, plan,
                                                        frame, &target_initialized);
        if (!record_result) {
            runtime->shutdown();
            return make_error<RuntimeCaptureResult>(record_result.error().code,
                                                    record_result.error().message,
                                                    record_result.error().location);
        }

        if (std::find(plan.frame_indices.begin(), plan.frame_indices.end(), frame) ==
            plan.frame_indices.end()) {
            continue;
        }

        const auto capture_result =
            capture_requested_frame(result, *runtime, fixture, plan, frame, target->image);
        if (!capture_result) {
            runtime->shutdown();
            return make_error<RuntimeCaptureResult>(capture_result.error().code,
                                                    capture_result.error().message,
                                                    capture_result.error().location);
        }
    }

    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
    return result;
}

} // namespace goggles::test
