#include "goggles_filter_chain.h"

#include <array>
#include <atomic>
#include <bit>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace {

#if defined(__SANITIZE_ADDRESS__)
constexpr bool ASAN_BUILD = true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
constexpr bool ASAN_BUILD = true;
#else
constexpr bool ASAN_BUILD = false;
#endif
#else
constexpr bool ASAN_BUILD = false;
#endif

constexpr uint32_t TEST_SYNC_INDICES = 2u;
constexpr uint32_t INVALID_SYNC_INDICES = 9u;

auto make_cache_dir() -> std::filesystem::path {
    static std::atomic<uint64_t> counter{0u};
    auto path = std::filesystem::temp_directory_path() /
                ("goggles_filter_chain_cache_" + std::to_string(counter.fetch_add(1u)));
    std::filesystem::create_directories(path);
    return path;
}

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_filter_chain_c_api_tests";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS) {
            return;
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{m_instance});

        uint32_t device_count = 0u;
        if (vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr) != VK_SUCCESS ||
            device_count == 0u) {
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        if (vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data()) != VK_SUCCESS) {
            return;
        }

        for (const auto candidate : devices) {
            uint32_t family_count = 0u;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            if (family_count == 0u) {
                continue;
            }

            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());

            for (uint32_t family = 0u; family < family_count; ++family) {
                if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) {
                    continue;
                }

                m_physical_device = candidate;
                m_queue_family_index = family;
                goto found_queue;
            }
        }

    found_queue:
        if (m_physical_device == VK_NULL_HANDLE) {
            return;
        }

        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = m_queue_family_index;
        queue_info.queueCount = 1u;
        queue_info.pQueuePriorities = &queue_priority;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features13;
        vkGetPhysicalDeviceFeatures2(m_physical_device, &features2);
        if (features13.dynamicRendering != VK_TRUE) {
            return;
        }
        features13.dynamicRendering = VK_TRUE;
        device_info.pNext = &features13;

        if (vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device) != VK_SUCCESS) {
            return;
        }

        vkGetDeviceQueue(m_device, m_queue_family_index, 0u, &m_queue);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device{m_device});
    }

    ~VulkanRuntimeFixture() {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
        }
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    [[nodiscard]] bool available() const { return m_device != VK_NULL_HANDLE; }

    [[nodiscard]] auto context() const -> goggles_chain_vk_context_t {
        return goggles_chain_vk_context_t{
            .device = m_device,
            .physical_device = m_physical_device,
            .graphics_queue = m_queue,
            .graphics_queue_family_index = m_queue_family_index,
        };
    }

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queue_family_index = UINT32_MAX;
};

struct ChainGuard {
    ~ChainGuard() { goggles_chain_destroy(&chain); }
    goggles_chain_t* chain = nullptr;
};

struct CacheDirGuard {
    explicit CacheDirGuard(std::filesystem::path cache_dir) : dir(std::move(cache_dir)) {}

    ~CacheDirGuard() {
        if (!dir.empty()) {
            std::filesystem::remove_all(dir);
        }
    }

    std::filesystem::path dir;
};

struct CommandBufferGuard {
    CommandBufferGuard() = default;
    CommandBufferGuard(const CommandBufferGuard&) = delete;
    auto operator=(const CommandBufferGuard&) -> CommandBufferGuard& = delete;

    CommandBufferGuard(CommandBufferGuard&& other) noexcept { *this = std::move(other); }

    auto operator=(CommandBufferGuard&& other) noexcept -> CommandBufferGuard& {
        if (this == &other) {
            return *this;
        }
        cleanup();
        device = other.device;
        pool = other.pool;
        command_buffer = other.command_buffer;
        other.device = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
        other.command_buffer = VK_NULL_HANDLE;
        return *this;
    }

    ~CommandBufferGuard() { cleanup(); }

    void cleanup() {
        if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, pool, nullptr);
        }
        device = VK_NULL_HANDLE;
        pool = VK_NULL_HANDLE;
        command_buffer = VK_NULL_HANDLE;
    }

    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
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
        }
        if (device != VK_NULL_HANDLE && image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
        if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
        device = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        view = VK_NULL_HANDLE;
    }

    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

struct MemoryTypeQuery {
    uint32_t type_bits = 0u;
    VkMemoryPropertyFlags required_properties = 0u;
};

struct ImageLayoutTransition {
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

auto find_memory_type_index(VkPhysicalDevice physical_device, MemoryTypeQuery query)
    -> std::optional<uint32_t> {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        const uint32_t bit = (1u << index);
        if ((query.type_bits & bit) == 0u) {
            continue;
        }
        const VkMemoryPropertyFlags flags = memory_properties.memoryTypes[index].propertyFlags;
        if ((flags & query.required_properties) == query.required_properties) {
            return index;
        }
    }

    return std::nullopt;
}

auto create_command_buffer(VkDevice device, uint32_t queue_family_index)
    -> std::optional<CommandBufferGuard> {
    CommandBufferGuard guard{};
    guard.device = device;

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &guard.pool) != VK_SUCCESS) {
        return std::nullopt;
    }

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = guard.pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1u;
    if (vkAllocateCommandBuffers(device, &alloc_info, &guard.command_buffer) != VK_SUCCESS) {
        return std::nullopt;
    }

    return guard;
}

auto create_image(VkDevice device, VkPhysicalDevice physical_device, VkFormat format,
                  VkImageUsageFlags usage, VkExtent2D extent) -> std::optional<ImageGuard> {
    ImageGuard guard{};
    guard.device = device;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent = VkExtent3D{extent.width, extent.height, 1u};
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

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device, guard.image, &memory_requirements);
    const auto memory_type = find_memory_type_index(
        physical_device, {.type_bits = memory_requirements.memoryTypeBits,
                          .required_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
    if (!memory_type.has_value()) {
        return std::nullopt;
    }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
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
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0u;
    view_info.subresourceRange.levelCount = 1u;
    view_info.subresourceRange.baseArrayLayer = 0u;
    view_info.subresourceRange.layerCount = 1u;
    if (vkCreateImageView(device, &view_info, nullptr, &guard.view) != VK_SUCCESS) {
        return std::nullopt;
    }

    return guard;
}

void transition_image_layout(VkCommandBuffer command_buffer, VkImage image,
                             ImageLayoutTransition transition) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = transition.old_layout;
    barrier.newLayout = transition.new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0u;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.baseArrayLayer = 0u;
    barrier.subresourceRange.layerCount = 1u;

    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (transition.new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stage, 0u, 0u,
                         nullptr, 0u, nullptr, 1u, &barrier);
}

auto create_ready_chain(const goggles_chain_vk_context_t& vk_context,
                        const std::filesystem::path& shader_root,
                        const std::filesystem::path& cache_dir,
                        const std::filesystem::path& preset_path) -> goggles_chain_t* {
    goggles_chain_vk_create_info_t create_info = goggles_chain_vk_create_info_init();
    create_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    create_info.num_sync_indices = TEST_SYNC_INDICES;
    create_info.initial_prechain_resolution = {.width = 1u, .height = 1u};

    std::filesystem::create_directories(cache_dir);
    const std::string shader_dir_utf8 = shader_root.string();
    const std::string cache_dir_utf8 = cache_dir.string();
    create_info.shader_dir_utf8 = shader_dir_utf8.c_str();
    create_info.cache_dir_utf8 = cache_dir_utf8.c_str();

    goggles_chain_t* chain = nullptr;
    if (goggles_chain_create_vk(&vk_context, &create_info, &chain) != GOGGLES_CHAIN_STATUS_OK) {
        return nullptr;
    }

    std::string preset_utf8 = preset_path.string();
    if (goggles_chain_preset_load(chain, preset_utf8.c_str()) != GOGGLES_CHAIN_STATUS_OK) {
        goggles_chain_destroy(&chain);
        return nullptr;
    }

    return chain;
}

[[nodiscard]] auto has_begin_rendering_command(VkDevice device) -> bool {
    if (device == VK_NULL_HANDLE) {
        return false;
    }

    const PFN_vkVoidFunction begin_rendering = vkGetDeviceProcAddr(device, "vkCmdBeginRendering");
    const PFN_vkVoidFunction begin_rendering_khr =
        vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
    return begin_rendering != nullptr || begin_rendering_khr != nullptr;
}

template <typename HandleType>
auto sentinel_handle(uintptr_t value) -> HandleType {
    static_assert(sizeof(HandleType) == sizeof(uintptr_t));
    return std::bit_cast<HandleType>(value);
}

auto make_valid_record_info() -> goggles_chain_vk_record_info_t {
    auto info = goggles_chain_vk_record_info_init();
    info.struct_size = sizeof(info);
    info.command_buffer = sentinel_handle<VkCommandBuffer>(0x1);
    info.source_image = sentinel_handle<VkImage>(0x2);
    info.source_view = sentinel_handle<VkImageView>(0x3);
    info.target_view = sentinel_handle<VkImageView>(0x4);
    info.source_extent = {.width = 1u, .height = 1u};
    info.target_extent = {.width = 1u, .height = 1u};
    info.frame_index = 0u;
    info.scale_mode = GOGGLES_CHAIN_SCALE_MODE_STRETCH;
    info.integer_scale = 1u;
    return info;
}

auto find_control_descriptor(const goggles_chain_control_snapshot_t* snapshot,
                             goggles_chain_stage_t stage, std::string_view name)
    -> const goggles_chain_control_desc_t* {
    const auto* descriptors = goggles_chain_control_snapshot_get_data(snapshot);
    const size_t count = goggles_chain_control_snapshot_get_count(snapshot);
    if (descriptors == nullptr) {
        return nullptr;
    }

    for (size_t index = 0; index < count; ++index) {
        const auto& descriptor = descriptors[index];
        if (descriptor.stage == stage && descriptor.name_utf8 != nullptr &&
            std::string_view(descriptor.name_utf8) == name) {
            return &descriptor;
        }
    }

    return nullptr;
}

} // namespace

TEST_CASE("Filter chain C API lifecycle and out-param safety", "[filter_chain_c_api]") {
    if constexpr (ASAN_BUILD) {
        SKIP("Skipping Vulkan-backed C API lifecycle test under ASAN due external Vulkan loader "
             "leak noise");
    }

    REQUIRE(goggles_chain_destroy(nullptr) == GOGGLES_CHAIN_STATUS_OK);

    goggles_chain_t* runtime = nullptr;
    REQUIRE(goggles_chain_destroy(&runtime) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime == nullptr);
    REQUIRE(goggles_chain_destroy(&runtime) == GOGGLES_CHAIN_STATUS_OK);

    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed C API lifecycle test because no Vulkan graphics device is "
             "available");
    }
    const auto vk_context = fixture.context();

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    const std::string shader_dir_utf8 = shader_root.string();
    const std::string cache_dir_utf8 = cache_dir_guard.dir.string();

    goggles_chain_vk_create_info_t create_info = goggles_chain_vk_create_info_init();
    create_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    create_info.num_sync_indices = TEST_SYNC_INDICES;
    create_info.initial_prechain_resolution = {.width = 1u, .height = 1u};
    create_info.shader_dir_utf8 = shader_dir_utf8.c_str();
    create_info.cache_dir_utf8 = cache_dir_utf8.c_str();

    auto* invalid_chain = reinterpret_cast<goggles_chain_t*>(0x1);
    create_info.struct_size = sizeof(create_info) - 1u;
    REQUIRE(goggles_chain_create_vk(&vk_context, &create_info, &invalid_chain) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);
    REQUIRE(invalid_chain == nullptr);

    create_info.struct_size = sizeof(create_info);
    goggles_chain_t* chain = nullptr;
    REQUIRE(goggles_chain_create_vk(&vk_context, &create_info, &chain) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(chain != nullptr);

    goggles_chain_extent2d_t prechain_resolution{};
    REQUIRE(goggles_chain_prechain_resolution_get(chain, &prechain_resolution) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(prechain_resolution.width == 1u);
    REQUIRE(prechain_resolution.height == 1u);

    REQUIRE(goggles_chain_prechain_resolution_set(chain, {.width = 2u, .height = 3u}) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(goggles_chain_prechain_resolution_get(chain, &prechain_resolution) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(prechain_resolution.width == 2u);
    REQUIRE(prechain_resolution.height == 3u);

    goggles_chain_control_snapshot_t* snapshot = nullptr;
    REQUIRE(goggles_chain_control_list(chain, &snapshot) == GOGGLES_CHAIN_STATUS_NOT_INITIALIZED);
    REQUIRE(snapshot == nullptr);

    auto record_info = make_valid_record_info();
    REQUIRE(goggles_chain_record_vk(chain, &record_info) == GOGGLES_CHAIN_STATUS_NOT_INITIALIZED);

    REQUIRE(goggles_chain_control_set_value(chain, 0u, 0.5f) ==
            GOGGLES_CHAIN_STATUS_NOT_INITIALIZED);

    const std::string preset_path_utf8 = preset_path.string();
    REQUIRE(goggles_chain_preset_load(chain, preset_path_utf8.c_str()) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_list(chain, &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    goggles_chain_destroy(&chain);
    REQUIRE(chain == nullptr);
}

TEST_CASE("Filter chain C API validation matrix", "[filter_chain_c_api][validation]") {
    if constexpr (ASAN_BUILD) {
        SKIP("Skipping Vulkan-backed C API validation test under ASAN due external Vulkan loader "
             "leak noise");
    }

    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed C API validation test because no Vulkan graphics device is "
             "available");
    }
    const auto vk_context = fixture.context();

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    REQUIRE(std::filesystem::exists(shader_root));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    const std::string shader_dir_utf8 = shader_root.string();
    const std::string cache_dir_utf8 = cache_dir_guard.dir.string();

    goggles_chain_vk_create_info_t create_info = goggles_chain_vk_create_info_init();
    create_info.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    create_info.num_sync_indices = TEST_SYNC_INDICES;
    create_info.initial_prechain_resolution = {.width = 1u, .height = 1u};
    create_info.shader_dir_utf8 = shader_dir_utf8.c_str();
    create_info.cache_dir_utf8 = cache_dir_utf8.c_str();

    goggles_chain_t* runtime = nullptr;

    auto invalid = create_info;
    invalid.struct_size = sizeof(invalid) - 1u;
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);
    REQUIRE(runtime == nullptr);

    invalid = create_info;
    invalid.num_sync_indices = 0u;
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid.num_sync_indices = INVALID_SYNC_INDICES;
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid = create_info;
    invalid.initial_prechain_resolution = {.width = 0u, .height = 1u};
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid.initial_prechain_resolution = {.width = 1u, .height = 0u};
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid = create_info;
    invalid.target_format = VK_FORMAT_UNDEFINED;
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid = create_info;
    invalid.shader_dir_utf8 = nullptr;
    REQUIRE(goggles_chain_create_vk(&vk_context, &invalid, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    goggles_chain_vk_create_info_ex_t create_info_ex = goggles_chain_vk_create_info_ex_init();
    create_info_ex.target_format = VK_FORMAT_B8G8R8A8_UNORM;
    create_info_ex.num_sync_indices = TEST_SYNC_INDICES;
    create_info_ex.initial_prechain_resolution = {.width = 1u, .height = 1u};
    create_info_ex.shader_dir_utf8 = shader_dir_utf8.c_str();
    create_info_ex.shader_dir_len = shader_dir_utf8.size();
    create_info_ex.cache_dir_utf8 = cache_dir_utf8.c_str();
    create_info_ex.cache_dir_len = cache_dir_utf8.size();

    auto invalid_ex = create_info_ex;
    invalid_ex.struct_size = sizeof(invalid_ex) - 1u;
    REQUIRE(goggles_chain_create_vk_ex(&vk_context, &invalid_ex, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    const std::array<char, 1> invalid_bytes{static_cast<char>(0xffu)};
    invalid_ex = create_info_ex;
    invalid_ex.shader_dir_utf8 = invalid_bytes.data();
    invalid_ex.shader_dir_len = invalid_bytes.size();
    REQUIRE(goggles_chain_create_vk_ex(&vk_context, &invalid_ex, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    const std::array<char, 7> embedded_null = {'a', 'b', 'c', '\0', 'd', 'e', 'f'};
    invalid_ex = create_info_ex;
    invalid_ex.shader_dir_utf8 = embedded_null.data();
    invalid_ex.shader_dir_len = embedded_null.size();
    REQUIRE(goggles_chain_create_vk_ex(&vk_context, &invalid_ex, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_ex = create_info_ex;
    invalid_ex.shader_dir_utf8 = nullptr;
    invalid_ex.shader_dir_len = 1u;
    REQUIRE(goggles_chain_create_vk_ex(&vk_context, &invalid_ex, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_ex = create_info_ex;
    invalid_ex.cache_dir_utf8 = nullptr;
    invalid_ex.cache_dir_len = 2u;
    REQUIRE(goggles_chain_create_vk_ex(&vk_context, &invalid_ex, &runtime) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    auto no_cache_create = create_info;
    runtime = reinterpret_cast<goggles_chain_t*>(0x1);
    no_cache_create.cache_dir_utf8 = nullptr;
    REQUIRE(goggles_chain_create_vk(&vk_context, &no_cache_create, &runtime) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime != nullptr);
    REQUIRE(goggles_chain_destroy(&runtime) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime == nullptr);

    no_cache_create = create_info;
    runtime = reinterpret_cast<goggles_chain_t*>(0x1);
    no_cache_create.cache_dir_utf8 = "";
    REQUIRE(goggles_chain_create_vk(&vk_context, &no_cache_create, &runtime) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime != nullptr);
    REQUIRE(goggles_chain_destroy(&runtime) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime == nullptr);

    auto no_cache_create_ex = create_info_ex;
    runtime = reinterpret_cast<goggles_chain_t*>(0x1);
    no_cache_create_ex.cache_dir_utf8 = nullptr;
    no_cache_create_ex.cache_dir_len = 0u;
    REQUIRE(goggles_chain_create_vk_ex(&vk_context, &no_cache_create_ex, &runtime) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime != nullptr);
    REQUIRE(goggles_chain_destroy(&runtime) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(runtime == nullptr);

    goggles_chain_capabilities_t caps = goggles_chain_capabilities_init();
    caps.struct_size -= 1u;
    REQUIRE(goggles_chain_capabilities_get(&caps) == GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    caps = goggles_chain_capabilities_init();
    REQUIRE(goggles_chain_capabilities_get(&caps) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE((caps.capability_flags & GOGGLES_CHAIN_CAPABILITY_LAST_ERROR_INFO) != 0u);
    REQUIRE(caps.max_sync_indices > 0u);
}

TEST_CASE("Filter chain C API snapshot contract", "[filter_chain_c_api][snapshot]") {
    if constexpr (ASAN_BUILD) {
        SKIP("Skipping Vulkan-backed C API snapshot test under ASAN due external Vulkan loader "
             "leak noise");
    }

    REQUIRE(goggles_chain_control_snapshot_get_count(nullptr) == 0u);
    REQUIRE(goggles_chain_control_snapshot_get_data(nullptr) == nullptr);
    REQUIRE(goggles_chain_control_snapshot_destroy(nullptr) == GOGGLES_CHAIN_STATUS_OK);

    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed C API snapshot test because no Vulkan graphics device is "
             "available");
    }
    const auto vk_context = fixture.context();

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    ChainGuard guard;
    guard.chain = create_ready_chain(vk_context, shader_root, cache_dir_guard.dir, preset_path);
    REQUIRE(guard.chain != nullptr);

    goggles_chain_control_snapshot_t* snapshot = nullptr;
    REQUIRE(goggles_chain_control_list(guard.chain, &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    REQUIRE(goggles_chain_control_snapshot_get_count(snapshot) > 0u);
    REQUIRE(goggles_chain_control_snapshot_get_data(snapshot) != nullptr);
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot == nullptr);

    goggles_chain_control_snapshot_t* post_snapshot = nullptr;
    REQUIRE(goggles_chain_control_list_stage(guard.chain, GOGGLES_CHAIN_STAGE_POSTCHAIN,
                                             &post_snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(post_snapshot != nullptr);
    REQUIRE(goggles_chain_control_snapshot_get_count(post_snapshot) == 0u);
    REQUIRE(goggles_chain_control_snapshot_get_data(post_snapshot) == nullptr);
    REQUIRE(goggles_chain_control_snapshot_destroy(&post_snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(post_snapshot == nullptr);

    auto* invalid_stage_snapshot = reinterpret_cast<goggles_chain_control_snapshot_t*>(0x1);
    REQUIRE(goggles_chain_control_list_stage(guard.chain, static_cast<goggles_chain_stage_t>(0xff),
                                             &invalid_stage_snapshot) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);
    REQUIRE(invalid_stage_snapshot == nullptr);
}

TEST_CASE("Filter chain C API prechain filter_type preserves nearest mode",
          "[filter_chain_c_api][controls]") {
    if constexpr (ASAN_BUILD) {
        SKIP("Skipping Vulkan-backed C API prechain control test under ASAN due external Vulkan "
             "loader leak noise");
    }

    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed C API prechain control test because no Vulkan graphics "
             "device is available");
    }
    const auto vk_context = fixture.context();

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    ChainGuard guard;
    guard.chain = create_ready_chain(vk_context, shader_root, cache_dir_guard.dir, preset_path);
    REQUIRE(guard.chain != nullptr);

    goggles_chain_control_snapshot_t* snapshot = nullptr;
    REQUIRE(goggles_chain_control_list_stage(guard.chain, GOGGLES_CHAIN_STAGE_PRECHAIN,
                                             &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);

    const auto* filter_type =
        find_control_descriptor(snapshot, GOGGLES_CHAIN_STAGE_PRECHAIN, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->default_value == Catch::Approx(0.0F));
    REQUIRE(filter_type->min_value == Catch::Approx(0.0F));
    REQUIRE(filter_type->max_value == Catch::Approx(2.0F));
    REQUIRE(filter_type->step == Catch::Approx(1.0F));
    const auto filter_type_id = filter_type->control_id;
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_set_value(guard.chain, filter_type_id, 2.0F) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(goggles_chain_prechain_resolution_set(guard.chain, {.width = 2u, .height = 3u}) ==
            GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_list_stage(guard.chain, GOGGLES_CHAIN_STAGE_PRECHAIN,
                                             &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    filter_type = find_control_descriptor(snapshot, GOGGLES_CHAIN_STAGE_PRECHAIN, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(2.0F));
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_list(guard.chain, &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    filter_type = find_control_descriptor(snapshot, GOGGLES_CHAIN_STAGE_PRECHAIN, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(2.0F));
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_set_value(guard.chain, filter_type_id, 0.0F) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(goggles_chain_control_list_stage(guard.chain, GOGGLES_CHAIN_STAGE_PRECHAIN,
                                             &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    filter_type = find_control_descriptor(snapshot, GOGGLES_CHAIN_STAGE_PRECHAIN, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(0.0F));
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_set_value(guard.chain, filter_type_id, 1.0F) ==
            GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(goggles_chain_control_list(guard.chain, &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    filter_type = find_control_descriptor(snapshot, GOGGLES_CHAIN_STAGE_PRECHAIN, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(1.0F));
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);
}

TEST_CASE("Filter chain C API control and record validation", "[filter_chain_c_api][record]") {
    if constexpr (ASAN_BUILD) {
        SKIP("Skipping Vulkan-backed C API record test under ASAN due external Vulkan loader leak "
             "noise");
    }

    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed C API record test because no Vulkan graphics device is "
             "available");
    }
    const auto vk_context = fixture.context();

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    ChainGuard guard;
    guard.chain = create_ready_chain(vk_context, shader_root, cache_dir_guard.dir, preset_path);
    REQUIRE(guard.chain != nullptr);

    goggles_chain_control_snapshot_t* snapshot = nullptr;
    REQUIRE(goggles_chain_control_list(guard.chain, &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    REQUIRE(goggles_chain_control_snapshot_get_count(snapshot) > 0u);
    const auto* descriptors = goggles_chain_control_snapshot_get_data(snapshot);
    REQUIRE(descriptors != nullptr);
    const auto baseline_value = descriptors[0].current_value;
    const auto control_id = descriptors[0].control_id;
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_control_set_value(guard.chain, control_id,
                                            std::numeric_limits<float>::quiet_NaN()) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);
    REQUIRE(goggles_chain_control_set_value(guard.chain, control_id,
                                            std::numeric_limits<float>::infinity()) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);
    REQUIRE(goggles_chain_control_set_value(guard.chain, control_id,
                                            -std::numeric_limits<float>::infinity()) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    REQUIRE(goggles_chain_control_list(guard.chain, &snapshot) == GOGGLES_CHAIN_STATUS_OK);
    REQUIRE(snapshot != nullptr);
    const auto* updated = goggles_chain_control_snapshot_get_data(snapshot);
    REQUIRE(updated != nullptr);
    REQUIRE(updated[0].current_value == baseline_value);
    REQUIRE(goggles_chain_control_snapshot_destroy(&snapshot) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(goggles_chain_record_vk(guard.chain, nullptr) == GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    auto record_info = make_valid_record_info();
    auto invalid_record = record_info;
    invalid_record.struct_size = sizeof(invalid_record) - 1u;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.command_buffer = VK_NULL_HANDLE;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.source_image = VK_NULL_HANDLE;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.source_view = VK_NULL_HANDLE;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.target_view = VK_NULL_HANDLE;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.source_extent.width = 0u;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.target_extent.height = 0u;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.frame_index = TEST_SYNC_INDICES;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.scale_mode = GOGGLES_CHAIN_SCALE_MODE_INTEGER;
    invalid_record.integer_scale = 0u;
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    invalid_record = record_info;
    invalid_record.scale_mode = static_cast<goggles_chain_scale_mode_t>(0xff);
    REQUIRE(goggles_chain_record_vk(guard.chain, &invalid_record) ==
            GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT);

    if (!has_begin_rendering_command(vk_context.device)) {
        SUCCEED("Skipping record success-path check because vkCmdBeginRendering is unavailable");
        return;
    }

    const auto command_buffer =
        create_command_buffer(vk_context.device, vk_context.graphics_queue_family_index);
    REQUIRE(command_buffer.has_value());

    constexpr VkExtent2D EXTENT_1X1{1u, 1u};
    auto source_image =
        create_image(vk_context.device, vk_context.physical_device, VK_FORMAT_B8G8R8A8_UNORM,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, EXTENT_1X1);
    REQUIRE(source_image.has_value());
    auto target_image = create_image(
        vk_context.device, vk_context.physical_device, VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, EXTENT_1X1);
    REQUIRE(target_image.has_value());

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    REQUIRE(vkBeginCommandBuffer(command_buffer->command_buffer, &begin_info) == VK_SUCCESS);

    transition_image_layout(command_buffer->command_buffer, source_image->image,
                            {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                             .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    transition_image_layout(command_buffer->command_buffer, target_image->image,
                            {.old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                             .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    auto valid_record = goggles_chain_vk_record_info_init();
    valid_record.command_buffer = command_buffer->command_buffer;
    valid_record.source_image = source_image->image;
    valid_record.source_view = source_image->view;
    valid_record.target_view = target_image->view;
    valid_record.source_extent = {.width = EXTENT_1X1.width, .height = EXTENT_1X1.height};
    valid_record.target_extent = {.width = EXTENT_1X1.width, .height = EXTENT_1X1.height};
    valid_record.frame_index = 0u;
    valid_record.scale_mode = GOGGLES_CHAIN_SCALE_MODE_STRETCH;
    valid_record.integer_scale = 1u;
    REQUIRE(goggles_chain_record_vk(guard.chain, &valid_record) == GOGGLES_CHAIN_STATUS_OK);

    REQUIRE(vkEndCommandBuffer(command_buffer->command_buffer) == VK_SUCCESS);
}

TEST_CASE("Filter chain C API ABI durability", "[filter_chain_c_api][abi]") {
    STATIC_REQUIRE(sizeof(goggles_chain_status_t) == sizeof(uint32_t));
    STATIC_REQUIRE(sizeof(goggles_chain_stage_t) == sizeof(uint32_t));
    STATIC_REQUIRE(sizeof(goggles_chain_capability_flags_t) == sizeof(uint32_t));
    STATIC_REQUIRE(sizeof(goggles_chain_control_id_t) == sizeof(uint64_t));

    STATIC_REQUIRE(offsetof(GogglesChainVkCreateInfo, struct_size) == 0u);
    STATIC_REQUIRE(offsetof(GogglesChainStagePolicy, struct_size) == 0u);
    STATIC_REQUIRE(offsetof(GogglesChainCapabilities, struct_size) == 0u);
    STATIC_REQUIRE(offsetof(GogglesChainControlDesc, control_id) == 0u);
    STATIC_REQUIRE(offsetof(GogglesChainStagePolicy, enabled_stage_mask) == sizeof(uint32_t));

    REQUIRE(goggles_chain_api_version() == GOGGLES_CHAIN_API_VERSION);
    REQUIRE(goggles_chain_abi_version() == GOGGLES_CHAIN_ABI_VERSION);
    REQUIRE(goggles_chain_status_to_string(GOGGLES_CHAIN_STATUS_OK) == std::string_view("OK"));
}
