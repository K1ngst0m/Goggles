#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <util/diagnostics/gpu_timestamp_pool.hpp>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace {

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_gpu_timestamp_pool_tests";
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

        uint32_t device_count = 0u;
        if (vkEnumeratePhysicalDevices(instance, &device_count, nullptr) != VK_SUCCESS ||
            device_count == 0u) {
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        if (vkEnumeratePhysicalDevices(instance, &device_count, devices.data()) != VK_SUCCESS) {
            return;
        }

        for (const auto candidate : devices) {
            uint32_t family_count = 0u;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            for (uint32_t family = 0u; family < family_count; ++family) {
                if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
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

        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = queue_family_index;
        queue_info.queueCount = 1u;
        queue_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.pNext = &features13;
        if (vkCreateDevice(physical_device, &device_info, nullptr, &device) != VK_SUCCESS) {
            return;
        }

        vkGetDeviceQueue(device, queue_family_index, 0u, &queue);
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
        return device != VK_NULL_HANDLE && command_pool != VK_NULL_HANDLE;
    }

    [[nodiscard]] auto timestamps_supported() const -> bool {
        return physical_device != VK_NULL_HANDLE &&
               goggles::diagnostics::GpuTimestampPool::supports_timestamps(
                   vk::PhysicalDevice{physical_device}, queue_family_index);
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

        if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
        if (device != VK_NULL_HANDLE && image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
        if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }

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

    ~ImageGuard() {
        if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
        if (device != VK_NULL_HANDLE && image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
        if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
    }

    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
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

auto create_image(VkDevice device, VkPhysicalDevice physical_device, VkExtent2D extent)
    -> std::optional<ImageGuard> {
    ImageGuard guard{};
    guard.device = device;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_info.extent = {.width = extent.width, .height = extent.height, .depth = 1u};
    image_info.mipLevels = 1u;
    image_info.arrayLayers = 1u;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    view_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1u;
    view_info.subresourceRange.layerCount = 1u;
    if (vkCreateImageView(device, &view_info, nullptr, &guard.view) != VK_SUCCESS) {
        return std::nullopt;
    }

    return guard;
}

auto submit_and_wait(VkDevice device, VkQueue queue, VkCommandBuffer command_buffer) -> bool {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        return false;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer;
    const bool ok = vkQueueSubmit(queue, 1u, &submit_info, fence) == VK_SUCCESS &&
                    vkWaitForFences(device, 1u, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    vkDestroyFence(device, fence, nullptr);
    return ok;
}

struct ImageTransition {
    VkImageLayout old_layout;
    VkImageLayout new_layout;
};

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

    const VkPipelineStageFlags dst_stage =
        transition.new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stage, 0u, 0u,
                         nullptr, 0u, nullptr, 1u, &barrier);
}

auto allocate_command_buffer(const VulkanRuntimeFixture& fixture) -> VkCommandBuffer {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return command_buffer;
}

} // namespace

TEST_CASE("GpuTimestampPool records available timestamp regions",
          "[render][diagnostics][profiling]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping GPU timestamp pool test because no Vulkan graphics device is available");
    }
    if (!fixture.timestamps_supported()) {
        SKIP("Skipping GPU timestamp pool availability test because timestamps are unavailable");
    }

    auto pool_result = goggles::diagnostics::GpuTimestampPool::create(
        vk::Device{fixture.device}, vk::PhysicalDevice{fixture.physical_device},
        goggles::diagnostics::GpuTimestampPoolCreateInfo{.graphics_queue_family_index =
                                                             fixture.queue_family_index,
                                                         .max_passes = 2u,
                                                         .frames_in_flight = 1u});
    REQUIRE(pool_result);

    auto pool = std::move(*pool_result);
    REQUIRE(pool->is_available());

    VkCommandBuffer command_buffer = allocate_command_buffer(fixture);
    REQUIRE(command_buffer != VK_NULL_HANDLE);

    const VkExtent2D extent{.width = 1u, .height = 1u};
    auto source_image = create_image(fixture.device, fixture.physical_device, extent);
    auto target_image = create_image(fixture.device, fixture.physical_device, extent);
    REQUIRE(source_image.has_value());
    REQUIRE(target_image.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);

    pool->reset_frame(vk::CommandBuffer{command_buffer}, 0u);

    pool->write_prechain_timestamp(vk::CommandBuffer{command_buffer}, 0u, true);
    transition_image(command_buffer, source_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    pool->write_prechain_timestamp(vk::CommandBuffer{command_buffer}, 0u, false);

    pool->write_pass_timestamp(vk::CommandBuffer{command_buffer}, 0u, 1u, true);
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    pool->write_pass_timestamp(vk::CommandBuffer{command_buffer}, 0u, 1u, false);

    pool->write_pass_timestamp(vk::CommandBuffer{command_buffer}, 0u, 0u, true);
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    pool->write_pass_timestamp(vk::CommandBuffer{command_buffer}, 0u, 0u, false);

    pool->write_final_composition_timestamp(vk::CommandBuffer{command_buffer}, 0u, true);
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    pool->write_final_composition_timestamp(vk::CommandBuffer{command_buffer}, 0u, false);

    REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
    REQUIRE(submit_and_wait(fixture.device, fixture.queue, command_buffer));

    auto samples_result = pool->read_results(0u);
    REQUIRE(samples_result);

    const auto& samples = *samples_result;
    REQUIRE(samples.size() == 4u);

    CHECK(samples[0].region == goggles::diagnostics::GpuTimestampRegion::pass);
    CHECK(samples[0].pass_ordinal == 0u);
    CHECK(samples[1].region == goggles::diagnostics::GpuTimestampRegion::pass);
    CHECK(samples[1].pass_ordinal == 1u);
    CHECK(samples[2].region == goggles::diagnostics::GpuTimestampRegion::prechain);
    CHECK(samples[3].region == goggles::diagnostics::GpuTimestampRegion::final_composition);
    CHECK(std::all_of(samples.begin(), samples.end(),
                      [](const auto& sample) { return sample.duration_us >= 0.0; }));

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
}

TEST_CASE("GpuTimestampPool degrades cleanly when timestamps are unavailable",
          "[render][diagnostics][profiling]") {
    auto pool = goggles::diagnostics::GpuTimestampPool::create_unavailable();
    CHECK_FALSE(pool->is_available());

    pool->reset_frame(vk::CommandBuffer{}, 0u);
    pool->write_pass_timestamp(vk::CommandBuffer{}, 0u, 0u, true);
    pool->write_pass_timestamp(vk::CommandBuffer{}, 0u, 0u, false);
    pool->write_prechain_timestamp(vk::CommandBuffer{}, 0u, true);
    pool->write_prechain_timestamp(vk::CommandBuffer{}, 0u, false);
    pool->write_final_composition_timestamp(vk::CommandBuffer{}, 0u, true);
    pool->write_final_composition_timestamp(vk::CommandBuffer{}, 0u, false);

    auto samples_result = pool->read_results(0u);
    REQUIRE(samples_result);
    CHECK(samples_result->empty());
}

TEST_CASE("GpuTimestampPool readback stays non-blocking while the next frame records",
          "[render][diagnostics][profiling]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping GPU timestamp pool async test because no Vulkan graphics device is "
             "available");
    }
    if (!fixture.timestamps_supported()) {
        SKIP("Skipping GPU timestamp pool async test because timestamps are unavailable");
    }

    auto pool_result = goggles::diagnostics::GpuTimestampPool::create(
        vk::Device{fixture.device}, vk::PhysicalDevice{fixture.physical_device},
        goggles::diagnostics::GpuTimestampPoolCreateInfo{.graphics_queue_family_index =
                                                             fixture.queue_family_index,
                                                         .max_passes = 1u,
                                                         .frames_in_flight = 2u});
    REQUIRE(pool_result);

    auto pool = std::move(*pool_result);
    REQUIRE(pool->is_available());

    VkCommandBuffer first_frame = allocate_command_buffer(fixture);
    VkCommandBuffer second_frame = allocate_command_buffer(fixture);
    REQUIRE(first_frame != VK_NULL_HANDLE);
    REQUIRE(second_frame != VK_NULL_HANDLE);

    const VkExtent2D extent{.width = 1u, .height = 1u};
    auto first_source = create_image(fixture.device, fixture.physical_device, extent);
    auto first_target = create_image(fixture.device, fixture.physical_device, extent);
    auto second_source = create_image(fixture.device, fixture.physical_device, extent);
    auto second_target = create_image(fixture.device, fixture.physical_device, extent);
    REQUIRE(first_source.has_value());
    REQUIRE(first_target.has_value());
    REQUIRE(second_source.has_value());
    REQUIRE(second_target.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    REQUIRE(vkBeginCommandBuffer(first_frame, &begin_info) == VK_SUCCESS);
    pool->reset_frame(vk::CommandBuffer{first_frame}, 0u);
    pool->write_prechain_timestamp(vk::CommandBuffer{first_frame}, 0u, true);
    transition_image(first_frame, first_source->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    pool->write_prechain_timestamp(vk::CommandBuffer{first_frame}, 0u, false);
    pool->write_pass_timestamp(vk::CommandBuffer{first_frame}, 0u, 0u, true);
    transition_image(first_frame, first_target->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    pool->write_pass_timestamp(vk::CommandBuffer{first_frame}, 0u, 0u, false);
    pool->write_final_composition_timestamp(vk::CommandBuffer{first_frame}, 0u, true);
    transition_image(first_frame, first_target->image,
                     {.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    pool->write_final_composition_timestamp(vk::CommandBuffer{first_frame}, 0u, false);
    REQUIRE(vkEndCommandBuffer(first_frame) == VK_SUCCESS);

    auto pending_results = pool->read_results(0u);
    REQUIRE(pending_results);
    CHECK(pending_results->empty());

    REQUIRE(vkBeginCommandBuffer(second_frame, &begin_info) == VK_SUCCESS);
    pool->reset_frame(vk::CommandBuffer{second_frame}, 1u);
    pool->write_prechain_timestamp(vk::CommandBuffer{second_frame}, 1u, true);
    transition_image(second_frame, second_source->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    pool->write_prechain_timestamp(vk::CommandBuffer{second_frame}, 1u, false);
    pool->write_pass_timestamp(vk::CommandBuffer{second_frame}, 1u, 0u, true);
    transition_image(second_frame, second_target->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    pool->write_pass_timestamp(vk::CommandBuffer{second_frame}, 1u, 0u, false);
    pool->write_final_composition_timestamp(vk::CommandBuffer{second_frame}, 1u, true);
    transition_image(second_frame, second_target->image,
                     {.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    pool->write_final_composition_timestamp(vk::CommandBuffer{second_frame}, 1u, false);
    REQUIRE(vkEndCommandBuffer(second_frame) == VK_SUCCESS);
    REQUIRE(submit_and_wait(fixture.device, fixture.queue, second_frame));

    auto second_frame_results = pool->read_results(1u);
    REQUIRE(second_frame_results);
    CHECK_FALSE(second_frame_results->empty());

    auto still_pending_results = pool->read_results(0u);
    REQUIRE(still_pending_results);
    CHECK(still_pending_results->empty());

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &first_frame);
    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &second_frame);
}
