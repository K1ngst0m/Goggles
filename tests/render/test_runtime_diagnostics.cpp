#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <render/backend/filter_chain_controller.hpp>
#include <render/chain/chain_runtime.hpp>
#include <render/chain/debug_label_scope.hpp>
#include <string>
#include <util/diagnostics/test_harness_sink.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace {

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_runtime_diagnostics_tests";
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

struct DebugLabelCapture {
    uint32_t begin_calls = 0;
    uint32_t end_calls = 0;
    std::vector<std::string> labels;
};

struct DebugLabelDispatcherOverride {
    explicit DebugLabelDispatcherOverride(DebugLabelCapture* capture)
        : previous_begin(VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT),
          previous_end(VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT) {
        active_capture = capture;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT = &begin_stub;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT = &end_stub;
    }

    ~DebugLabelDispatcherOverride() {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT = previous_begin;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT = previous_end;
        active_capture = nullptr;
    }

    DebugLabelDispatcherOverride(const DebugLabelDispatcherOverride&) = delete;
    auto operator=(const DebugLabelDispatcherOverride&) -> DebugLabelDispatcherOverride& = delete;

    static VKAPI_ATTR void VKAPI_CALL begin_stub(VkCommandBuffer command_buffer,
                                                 const VkDebugUtilsLabelEXT* label) {
        (void)command_buffer;
        if (active_capture == nullptr) {
            return;
        }

        active_capture->begin_calls++;
        if (label != nullptr && label->pLabelName != nullptr) {
            active_capture->labels.emplace_back(label->pLabelName);
        }
    }

    static VKAPI_ATTR void VKAPI_CALL end_stub(VkCommandBuffer command_buffer) {
        (void)command_buffer;
        if (active_capture != nullptr) {
            active_capture->end_calls++;
        }
    }

    static void begin_label(vk::CommandBuffer command_buffer, const vk::DebugUtilsLabelEXT& label) {
        (void)command_buffer;
        if (active_capture == nullptr) {
            return;
        }

        active_capture->begin_calls++;
        if (label.pLabelName != nullptr) {
            active_capture->labels.emplace_back(label.pLabelName);
        }
    }

    static void end_label(vk::CommandBuffer command_buffer) {
        (void)command_buffer;
        if (active_capture != nullptr) {
            active_capture->end_calls++;
        }
    }

    inline static DebugLabelCapture* active_capture = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT previous_begin = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT previous_end = nullptr;
};

struct DebugLabelDispatcherDisable {
    DebugLabelDispatcherDisable()
        : previous_begin(VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT),
          previous_end(VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT = nullptr;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT = nullptr;
    }

    ~DebugLabelDispatcherDisable() {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT = previous_begin;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT = previous_end;
    }

    DebugLabelDispatcherDisable(const DebugLabelDispatcherDisable&) = delete;
    auto operator=(const DebugLabelDispatcherDisable&) -> DebugLabelDispatcherDisable& = delete;

    PFN_vkCmdBeginDebugUtilsLabelEXT previous_begin = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT previous_end = nullptr;
};

struct DebugLabelCaptureScope {
    explicit DebugLabelCaptureScope(DebugLabelCapture* capture) {
        DebugLabelDispatcherOverride::active_capture = capture;
    }

    ~DebugLabelCaptureScope() { DebugLabelDispatcherOverride::active_capture = nullptr; }

    DebugLabelCaptureScope(const DebugLabelCaptureScope&) = delete;
    auto operator=(const DebugLabelCaptureScope&) -> DebugLabelCaptureScope& = delete;
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

} // namespace

TEST_CASE("ScopedDebugLabel skips incomplete debug-utils dispatch",
          "[render][diagnostics][runtime][profiling]") {
    DebugLabelCapture capture{};
    const DebugLabelCaptureScope capture_scope(&capture);

    {
        const goggles::render::ScopedDebugLabel label(
            vk::CommandBuffer{}, "Pass 0 Null Begin", {0.18F, 0.46F, 0.92F, 1.0F},
            goggles::render::DebugLabelDispatch{.begin = nullptr,
                                                .end = &DebugLabelDispatcherOverride::end_label,
                                                .enabled = true});
        CHECK_FALSE(label.active());
    }
    CHECK(capture.begin_calls == 0u);
    CHECK(capture.end_calls == 0u);

    {
        const goggles::render::ScopedDebugLabel label(
            vk::CommandBuffer{}, "Pass 0 Null End", {0.18F, 0.46F, 0.92F, 1.0F},
            goggles::render::DebugLabelDispatch{.begin = &DebugLabelDispatcherOverride::begin_label,
                                                .end = nullptr,
                                                .enabled = true});
        CHECK_FALSE(label.active());
    }
    CHECK(capture.begin_calls == 0u);
    CHECK(capture.end_calls == 0u);

    {
        const goggles::render::ScopedDebugLabel label(
            vk::CommandBuffer{}, "Pass 0 Active", {0.18F, 0.46F, 0.92F, 1.0F},
            goggles::render::DebugLabelDispatch{.begin = &DebugLabelDispatcherOverride::begin_label,
                                                .end = &DebugLabelDispatcherOverride::end_label,
                                                .enabled = true});
        CHECK(label.active());
    }
    CHECK(capture.begin_calls == 1u);
    CHECK(capture.end_calls == 1u);
    REQUIRE(capture.labels.size() == 1u);
    CHECK(capture.labels.front() == "Pass 0 Active");
}

TEST_CASE("ChainRuntime emits runtime diagnostics ledgers", "[render][diagnostics][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping runtime diagnostics test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    const auto cache_dir = std::filesystem::temp_directory_path() / "goggles_runtime_diag_cache";
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = goggles::render::ChainRuntime::create(
        vk_ctx, vk::Format::eB8G8R8A8Unorm, 2u, {.shader_dir = shader_root, .cache_dir = cache_dir},
        {1u, 1u});
    REQUIRE(runtime_result);
    auto runtime = std::move(*runtime_result);

    runtime->create_diagnostic_session({});
    auto* session = runtime->diagnostic_session();
    REQUIRE(session != nullptr);
    session->register_sink(std::make_unique<goggles::diagnostics::TestHarnessSink>());

    REQUIRE(runtime->load_preset(preset_path));

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) == VK_SUCCESS);

    const VkExtent2D extent{.width = 1u, .height = 1u};
    auto source_image = create_image(fixture.device, fixture.physical_device, extent);
    auto target_image = create_image(fixture.device, fixture.physical_device, extent);
    REQUIRE(source_image.has_value());
    REQUIRE(target_image.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
    transition_image(command_buffer, source_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    runtime->record(vk::CommandBuffer{command_buffer}, vk::Image{source_image->image},
                    vk::ImageView{source_image->view}, vk::Extent2D{1u, 1u},
                    vk::ImageView{target_image->view}, vk::Extent2D{1u, 1u}, 0u);
    REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);

    REQUIRE_FALSE(session->binding_ledger().all_entries().empty());
    REQUIRE_FALSE(session->semantic_ledger().all_entries().empty());
    REQUIRE_FALSE(session->execution_timeline().events().empty());

    bool saw_prechain_start = false;
    bool saw_prechain_end = false;
    bool saw_final_composition_start = false;
    bool saw_final_composition_end = false;
    for (const auto& event : session->execution_timeline().events()) {
        saw_prechain_start = saw_prechain_start ||
                             event.type == goggles::diagnostics::TimelineEventType::prechain_start;
        saw_prechain_end =
            saw_prechain_end || event.type == goggles::diagnostics::TimelineEventType::prechain_end;
        saw_final_composition_start =
            saw_final_composition_start ||
            event.type == goggles::diagnostics::TimelineEventType::final_composition_start;
        saw_final_composition_end =
            saw_final_composition_end ||
            event.type == goggles::diagnostics::TimelineEventType::final_composition_end;
    }

    CHECK(saw_prechain_start);
    CHECK(saw_prechain_end);
    CHECK(saw_final_composition_start);
    CHECK(saw_final_composition_end);

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
}

TEST_CASE("Strict diagnostics forbid fallback pass execution", "[render][diagnostics][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping runtime diagnostics test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/history.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    const auto cache_dir =
        std::filesystem::temp_directory_path() / "goggles_runtime_diag_strict_cache";
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = goggles::render::ChainRuntime::create(
        vk_ctx, vk::Format::eB8G8R8A8Unorm, 2u, {.shader_dir = shader_root, .cache_dir = cache_dir},
        {1u, 1u});
    REQUIRE(runtime_result);
    auto runtime = std::move(*runtime_result);

    goggles::diagnostics::DiagnosticPolicy policy{};
    policy.mode = goggles::diagnostics::PolicyMode::strict;
    policy.promote_fallback_to_error = true;
    runtime->create_diagnostic_session(policy);

    auto* session = runtime->diagnostic_session();
    REQUIRE(session != nullptr);
    auto harness = std::make_unique<goggles::diagnostics::TestHarnessSink>();
    auto* harness_ptr = harness.get();
    session->register_sink(std::move(harness));

    REQUIRE(runtime->load_preset(preset_path));

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) == VK_SUCCESS);

    const VkExtent2D extent{.width = 1u, .height = 1u};
    auto source_image = create_image(fixture.device, fixture.physical_device, extent);
    auto target_image = create_image(fixture.device, fixture.physical_device, extent);
    REQUIRE(source_image.has_value());
    REQUIRE(target_image.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
    transition_image(command_buffer, source_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    runtime->record(vk::CommandBuffer{command_buffer}, vk::Image{source_image->image},
                    vk::ImageView{source_image->view}, vk::Extent2D{1u, 1u},
                    vk::ImageView{target_image->view}, vk::Extent2D{1u, 1u}, 0u);
    REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);

    REQUIRE(harness_ptr != nullptr);
    CHECK(session->event_count(goggles::diagnostics::Severity::error) > 0u);
    CHECK_FALSE(session->degradation_ledger().all_entries().empty());

    bool saw_pass_start = false;
    for (const auto& event : session->execution_timeline().events()) {
        if (event.type == goggles::diagnostics::TimelineEventType::pass_start) {
            saw_pass_start = true;
            break;
        }
    }
    CHECK_FALSE(saw_pass_start);

    const auto runtime_events =
        harness_ptr->events_by_category(goggles::diagnostics::Category::runtime);
    CHECK(std::any_of(runtime_events.begin(), runtime_events.end(), [](const auto& event) {
        return event.severity == goggles::diagnostics::Severity::error &&
               event.localization.stage == "record" &&
               event.message.find("skipping remaining effect passes") != std::string::npos;
    }));

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
}

TEST_CASE("ChainRuntime captures pass outputs", "[render][diagnostics][capture]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping runtime diagnostics test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    const auto cache_dir =
        std::filesystem::temp_directory_path() / "goggles_runtime_diag_capture_cache";
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = goggles::render::ChainRuntime::create(
        vk_ctx, vk::Format::eB8G8R8A8Unorm, 2u, {.shader_dir = shader_root, .cache_dir = cache_dir},
        {1u, 1u});
    REQUIRE(runtime_result);
    auto runtime = std::move(*runtime_result);

    REQUIRE(runtime->load_preset(preset_path));

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) == VK_SUCCESS);

    const VkExtent2D extent{.width = 1u, .height = 1u};
    auto source_image = create_image(fixture.device, fixture.physical_device, extent);
    auto target_image = create_image(fixture.device, fixture.physical_device, extent);
    REQUIRE(source_image.has_value());
    REQUIRE(target_image.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
    transition_image(command_buffer, source_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    runtime->record(vk::CommandBuffer{command_buffer}, vk::Image{source_image->image},
                    vk::ImageView{source_image->view}, vk::Extent2D{1u, 1u},
                    vk::ImageView{target_image->view}, vk::Extent2D{1u, 1u}, 0u);
    REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
    REQUIRE(submit_and_wait(fixture.device, fixture.queue, command_buffer));

    const auto capture_result = runtime->capture_pass_output(0u);
    REQUIRE(capture_result);
    CHECK(capture_result->width == 1u);
    CHECK(capture_result->height == 1u);
    CHECK(capture_result->rgba.size() == 4u);

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
}

TEST_CASE("FilterChainController auto-enables diagnostics from config",
          "[render][diagnostics][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping filter-chain controller diagnostics test because no Vulkan graphics device "
             "is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::backend_internal::FilterChainController controller;
    controller.preset_path = preset_path;

    goggles::Config::Diagnostics diagnostics{};
    diagnostics.configured = true;
    diagnostics.mode = "investigate";
    diagnostics.strict = true;
    diagnostics.tier = 1u;
    diagnostics.capture_frame_limit = 3u;
    diagnostics.retention_bytes = 4096u;

    const auto cache_dir =
        std::filesystem::temp_directory_path() / "goggles_controller_diag_runtime_cache";
    std::filesystem::create_directories(cache_dir);

    const auto recreate_result = controller.recreate_filter_chain(
        {.vulkan_context = {.device = vk::Device{fixture.device},
                            .physical_device = vk::PhysicalDevice{fixture.physical_device},
                            .command_pool = vk::CommandPool{fixture.command_pool},
                            .graphics_queue = vk::Queue{fixture.queue}},
         .graphics_queue_family_index = fixture.queue_family_index,
         .target_format = vk::Format::eB8G8R8A8Unorm,
         .num_sync_indices = 2u,
         .shader_dir = shader_root,
         .cache_dir = cache_dir,
         .initial_prechain_resolution = {1u, 1u},
         .diagnostics_config = diagnostics});
    REQUIRE(recreate_result);

    const auto summary = controller.filter_chain_runtime().diagnostics_summary();
    REQUIRE(summary);
    CHECK(summary->reporting_mode == goggles::render::ChainDiagnosticReportingMode::investigate);
    CHECK(summary->policy_mode == goggles::render::ChainDiagnosticPolicyMode::strict);
    CHECK(summary->info_count + summary->warning_count + summary->error_count > 0u);

    controller.shutdown([device = fixture.device]() {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    });
    std::filesystem::remove_all(cache_dir);
}

TEST_CASE("ChainRuntime Tier 1 diagnostics expose GPU timing evidence",
          "[render][diagnostics][runtime][profiling]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Tier 1 runtime diagnostics test because no Vulkan graphics device is "
             "available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    const auto cache_dir =
        std::filesystem::temp_directory_path() / "goggles_runtime_diag_tier1_cache";
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = goggles::render::ChainRuntime::create(
        vk_ctx, vk::Format::eB8G8R8A8Unorm, 2u, {.shader_dir = shader_root, .cache_dir = cache_dir},
        {1u, 1u});
    REQUIRE(runtime_result);
    auto runtime = std::move(*runtime_result);

    if (!goggles::diagnostics::GpuTimestampPool::supports_timestamps(
            vk::PhysicalDevice{fixture.physical_device}, fixture.queue_family_index)) {
        runtime->shutdown();
        std::filesystem::remove_all(cache_dir);
        SKIP("Skipping Tier 1 GPU timing runtime test because timestamps are unavailable");
    }

    goggles::diagnostics::DiagnosticPolicy policy{};
    policy.tier = goggles::diagnostics::ActivationTier::tier1;
    runtime->create_diagnostic_session(policy);

    auto* session = runtime->diagnostic_session();
    REQUIRE(session != nullptr);

    REQUIRE(runtime->load_preset(preset_path));

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;

    const auto record_frame = [&](VkCommandBuffer command_buffer) {
        const VkExtent2D extent{.width = 1u, .height = 1u};
        auto source_image = create_image(fixture.device, fixture.physical_device, extent);
        auto target_image = create_image(fixture.device, fixture.physical_device, extent);
        REQUIRE(source_image.has_value());
        REQUIRE(target_image.has_value());

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
        transition_image(command_buffer, source_image->image,
                         {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                          .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        transition_image(command_buffer, target_image->image,
                         {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                          .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

        runtime->record(vk::CommandBuffer{command_buffer}, vk::Image{source_image->image},
                        vk::ImageView{source_image->view}, vk::Extent2D{1u, 1u},
                        vk::ImageView{target_image->view}, vk::Extent2D{1u, 1u}, 0u);
        REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
        REQUIRE(submit_and_wait(fixture.device, fixture.queue, command_buffer));
        REQUIRE(vkResetCommandBuffer(command_buffer, 0u) == VK_SUCCESS);
    };

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) == VK_SUCCESS);
    record_frame(command_buffer);
    record_frame(command_buffer);

    const bool saw_unavailable_event =
        std::any_of(session->events().begin(), session->events().end(), [](const auto& event) {
            return event.category == goggles::diagnostics::Category::runtime &&
                   event.severity == goggles::diagnostics::Severity::info &&
                   event.message == "GPU timestamps are unavailable on this device";
        });
    CHECK_FALSE(saw_unavailable_event);

    std::vector<goggles::diagnostics::TimelineEvent> annotated_pass_events;
    bool saw_prechain_gpu_duration = false;
    bool saw_final_gpu_duration = false;
    for (const auto& event : session->execution_timeline().events()) {
        if (!event.gpu_duration_us.has_value()) {
            continue;
        }

        if (event.type == goggles::diagnostics::TimelineEventType::pass_end) {
            annotated_pass_events.push_back(event);
        } else if (event.type == goggles::diagnostics::TimelineEventType::prechain_end) {
            saw_prechain_gpu_duration = true;
        } else if (event.type == goggles::diagnostics::TimelineEventType::final_composition_end) {
            saw_final_gpu_duration = true;
        }
    }

    CHECK(saw_prechain_gpu_duration);
    CHECK(saw_final_gpu_duration);
    REQUIRE(annotated_pass_events.size() >= 2u);

    std::sort(annotated_pass_events.begin(), annotated_pass_events.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.gpu_duration_us.value() > rhs.gpu_duration_us.value();
              });
    CHECK(annotated_pass_events.front().gpu_duration_us.value() >=
          annotated_pass_events.back().gpu_duration_us.value());
    CHECK(std::any_of(annotated_pass_events.begin(), annotated_pass_events.end(),
                      [](const auto& event) { return event.pass_ordinal == 0u; }));
    CHECK(std::any_of(annotated_pass_events.begin(), annotated_pass_events.end(),
                      [](const auto& event) { return event.pass_ordinal == 1u; }));

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
}

TEST_CASE("ChainRuntime reports unavailable GPU timestamps deterministically",
          "[render][diagnostics][runtime][profiling]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Tier 1 runtime diagnostics test because no Vulkan graphics device is "
             "available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    const auto cache_dir =
        std::filesystem::temp_directory_path() / "goggles_runtime_diag_tier1_unavailable_cache";
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = goggles::render::ChainRuntime::create(
        vk_ctx, vk::Format::eB8G8R8A8Unorm, 2u, {.shader_dir = shader_root, .cache_dir = cache_dir},
        {1u, 1u});
    REQUIRE(runtime_result);
    auto runtime = std::move(*runtime_result);

    goggles::diagnostics::DiagnosticPolicy policy{};
    policy.tier = goggles::diagnostics::ActivationTier::tier1;
    policy.gpu_timestamp_availability =
        goggles::diagnostics::GpuTimestampAvailabilityMode::force_unavailable;
    runtime->create_diagnostic_session(policy);

    auto* session = runtime->diagnostic_session();
    REQUIRE(session != nullptr);
    REQUIRE(runtime->load_preset(preset_path));

    const bool saw_unavailable_event =
        std::any_of(session->events().begin(), session->events().end(), [](const auto& event) {
            return event.category == goggles::diagnostics::Category::runtime &&
                   event.severity == goggles::diagnostics::Severity::info &&
                   event.message == "GPU timestamps are unavailable on this device";
        });
    CHECK(saw_unavailable_event);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) == VK_SUCCESS);

    const VkExtent2D extent{.width = 1u, .height = 1u};
    auto source_image = create_image(fixture.device, fixture.physical_device, extent);
    auto target_image = create_image(fixture.device, fixture.physical_device, extent);
    REQUIRE(source_image.has_value());
    REQUIRE(target_image.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
    transition_image(command_buffer, source_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    transition_image(command_buffer, target_image->image,
                     {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    runtime->record(vk::CommandBuffer{command_buffer}, vk::Image{source_image->image},
                    vk::ImageView{source_image->view}, vk::Extent2D{1u, 1u},
                    vk::ImageView{target_image->view}, vk::Extent2D{1u, 1u}, 0u);
    REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
    REQUIRE(submit_and_wait(fixture.device, fixture.queue, command_buffer));

    const bool saw_gpu_duration =
        std::any_of(session->execution_timeline().events().begin(),
                    session->execution_timeline().events().end(),
                    [](const auto& event) { return event.gpu_duration_us.has_value(); });
    CHECK_FALSE(saw_gpu_duration);

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
}

TEST_CASE("ChainRuntime emits profiling debug labels when dispatch is available",
          "[render][diagnostics][runtime][profiling]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping debug label runtime test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/history.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    goggles::render::VulkanContext vk_ctx{
        .device = vk::Device{fixture.device},
        .physical_device = vk::PhysicalDevice{fixture.physical_device},
        .command_pool = vk::CommandPool{fixture.command_pool},
        .graphics_queue = vk::Queue{fixture.queue},
        .graphics_queue_family_index = fixture.queue_family_index,
    };

    const auto cache_dir =
        std::filesystem::temp_directory_path() / "goggles_runtime_diag_debug_label_cache";
    std::filesystem::create_directories(cache_dir);
    auto runtime_result = goggles::render::ChainRuntime::create(
        vk_ctx, vk::Format::eB8G8R8A8Unorm, 2u, {.shader_dir = shader_root, .cache_dir = cache_dir},
        {1u, 1u});
    REQUIRE(runtime_result);
    auto runtime = std::move(*runtime_result);

    goggles::diagnostics::DiagnosticPolicy policy{};
    policy.tier = goggles::diagnostics::ActivationTier::tier1;
    runtime->create_diagnostic_session(policy);
    REQUIRE(runtime->load_preset(preset_path));

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = fixture.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;

    const auto record_frame = [&](VkCommandBuffer command_buffer) {
        const VkExtent2D extent{.width = 1u, .height = 1u};
        auto source_image = create_image(fixture.device, fixture.physical_device, extent);
        auto target_image = create_image(fixture.device, fixture.physical_device, extent);
        REQUIRE(source_image.has_value());
        REQUIRE(target_image.has_value());

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        REQUIRE(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
        transition_image(command_buffer, source_image->image,
                         {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                          .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        transition_image(command_buffer, target_image->image,
                         {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                          .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

        runtime->record(vk::CommandBuffer{command_buffer}, vk::Image{source_image->image},
                        vk::ImageView{source_image->view}, vk::Extent2D{1u, 1u},
                        vk::ImageView{target_image->view}, vk::Extent2D{1u, 1u}, 0u);
        REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
        REQUIRE(submit_and_wait(fixture.device, fixture.queue, command_buffer));
        REQUIRE(vkResetCommandBuffer(command_buffer, 0u) == VK_SUCCESS);
    };

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(fixture.device, &alloc_info, &command_buffer) == VK_SUCCESS);

    DebugLabelCapture capture{};
    {
        const DebugLabelDispatcherOverride debug_labels(&capture);
        record_frame(command_buffer);
    }

    CHECK(capture.begin_calls > 0u);
    CHECK(capture.begin_calls == capture.end_calls);
    CHECK(std::any_of(capture.labels.begin(), capture.labels.end(),
                      [](const auto& label) { return label.starts_with("Pass "); }));
    CHECK(std::find(capture.labels.begin(), capture.labels.end(), "History Push") !=
          capture.labels.end());
    CHECK(std::find(capture.labels.begin(), capture.labels.end(), "Feedback Copy") !=
          capture.labels.end());

    {
        const DebugLabelDispatcherDisable debug_labels_disabled;
        record_frame(command_buffer);
    }

    vkFreeCommandBuffers(fixture.device, fixture.command_pool, 1u, &command_buffer);
    runtime->shutdown();
    std::filesystem::remove_all(cache_dir);
}
