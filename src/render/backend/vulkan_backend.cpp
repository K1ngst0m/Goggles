#include "vulkan_backend.hpp"

#include "vulkan_error.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <render/chain/pass.hpp>
#include <util/job_system.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>
#include <util/unique_fd.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace goggles::render {

namespace {

constexpr std::array REQUIRED_INSTANCE_EXTENSIONS = {
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
};

constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,  VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
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

} // namespace

VulkanBackend::~VulkanBackend() {
    shutdown();
}

auto VulkanBackend::create(SDL_Window* window, bool enable_validation,
                           const std::filesystem::path& shader_dir, ScaleMode scale_mode,
                           uint32_t integer_scale) -> ResultPtr<VulkanBackend> {
    GOGGLES_PROFILE_FUNCTION();

    auto backend = std::unique_ptr<VulkanBackend>(new VulkanBackend());

    auto vk_get_instance_proc_addr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (vk_get_instance_proc_addr == nullptr) {
        return make_result_ptr_error<VulkanBackend>(ErrorCode::vulkan_init_failed,
                                                    "Failed to get vkGetInstanceProcAddr from SDL");
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

    backend->m_window = window;
    backend->m_enable_validation = enable_validation;
    backend->m_shader_dir = shader_dir;
    backend->m_scale_mode = scale_mode;
    backend->m_integer_scale = integer_scale;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    GOGGLES_TRY(backend->create_instance(enable_validation));
    GOGGLES_TRY(backend->create_debug_messenger());
    GOGGLES_TRY(backend->create_surface(window));
    GOGGLES_TRY(backend->select_physical_device());
    GOGGLES_TRY(backend->create_device());
    GOGGLES_TRY(backend->create_swapchain(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), vk::Format::eB8G8R8A8Srgb));
    GOGGLES_TRY(backend->create_command_resources());
    GOGGLES_TRY(backend->create_sync_objects());
    GOGGLES_TRY(backend->init_filter_chain());

    GOGGLES_LOG_INFO("Vulkan backend initialized: {}x{}", width, height);
    return make_result_ptr(std::move(backend));
}

void VulkanBackend::shutdown() {
    if (m_pending_load_future.valid()) {
        auto status = m_pending_load_future.wait_for(std::chrono::seconds(3));
        if (status == std::future_status::timeout) {
            GOGGLES_LOG_WARN("Shader load task still running during shutdown, may cause issues");
        }
    }

    if (m_device) {
        auto wait_result = m_device->waitIdle();
        if (wait_result != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("waitIdle failed during shutdown: {}", vk::to_string(wait_result));
        }
    }

    if (m_filter_chain) {
        m_filter_chain->shutdown();
    }
    if (m_shader_runtime) {
        m_shader_runtime->shutdown();
    }
    cleanup_imported_image();
    cleanup_sync_semaphores();

    if (m_device) {
        for (auto& frame : m_frames) {
            m_device->destroyFence(frame.in_flight_fence);
            m_device->destroySemaphore(frame.image_available_sem);
        }
        for (auto sem : m_render_finished_sems) {
            m_device->destroySemaphore(sem);
        }
    }
    m_frames = {};
    m_render_finished_sems.clear();
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    m_command_pool.reset();
    m_swapchain.reset();
    m_device.reset();
    m_surface.reset();
    m_debug_messenger.reset();
    m_instance.reset();

    GOGGLES_LOG_INFO("Vulkan backend shutdown");
}

auto VulkanBackend::create_instance(bool enable_validation) -> Result<void> {
    uint32_t sdl_ext_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
    if (sdl_extensions == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                std::string("SDL_Vulkan_GetInstanceExtensions failed: ") +
                                    SDL_GetError());
    }

    std::vector<const char*> extensions(sdl_extensions, sdl_extensions + sdl_ext_count);
    for (const auto* ext : REQUIRED_INSTANCE_EXTENSIONS) {
        if (std::find(extensions.begin(), extensions.end(), std::string_view(ext)) ==
            extensions.end()) {
            extensions.push_back(ext);
        }
    }

    std::vector<const char*> layers;

    if (enable_validation) {
        if (is_validation_layer_available()) {
            layers.push_back(VALIDATION_LAYER_NAME);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            GOGGLES_LOG_INFO("Vulkan validation layer enabled");
        } else {
            GOGGLES_LOG_WARN("Vulkan validation layer requested but not available");
        }
    }

    vk::ApplicationInfo app_info{};
    app_info.pApplicationName = "Goggles";
    app_info.applicationVersion =
        VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH);
    app_info.pEngineName = "Goggles";
    app_info.engineVersion =
        VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo create_info{};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    auto [result, instance] = vk::createInstanceUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create Vulkan instance: " + vk::to_string(result));
    }

    m_instance = std::move(instance);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_instance);

    GOGGLES_LOG_DEBUG("Vulkan instance created with {} extensions, {} layers", extensions.size(),
                      layers.size());
    return {};
}

auto VulkanBackend::create_debug_messenger() -> Result<void> {
    if (!m_enable_validation || !is_validation_layer_available()) {
        return {};
    }

    auto messenger_result = VulkanDebugMessenger::create(*m_instance);
    if (!messenger_result) {
        GOGGLES_LOG_WARN("Failed to create debug messenger: {}", messenger_result.error().message);
        return {};
    }

    m_debug_messenger = std::move(messenger_result.value());
    return {};
}

auto VulkanBackend::create_surface(SDL_Window* window) -> Result<void> {
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, *m_instance, nullptr, &raw_surface)) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }

    m_surface = vk::UniqueSurfaceKHR(raw_surface, *m_instance);
    GOGGLES_LOG_DEBUG("Vulkan surface created");
    return {};
}

auto VulkanBackend::select_physical_device() -> Result<void> {
    auto [result, devices] = m_instance->enumeratePhysicalDevices();
    if (result != vk::Result::eSuccess || devices.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "No Vulkan devices found");
    }

    for (const auto& device : devices) {
        auto queue_families = device.getQueueFamilyProperties();
        uint32_t graphics_family = UINT32_MAX;

        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            const auto& family = queue_families[i];
            if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
                auto [res, supported] = device.getSurfaceSupportKHR(i, *m_surface);
                if (res == vk::Result::eSuccess && supported) {
                    graphics_family = i;
                    break;
                }
            }
        }

        if (graphics_family == UINT32_MAX) {
            continue;
        }

        auto [ext_result, available_extensions] = device.enumerateDeviceExtensionProperties();
        if (ext_result != vk::Result::eSuccess) {
            continue;
        }

        bool all_extensions_found = true;
        for (const auto* required : REQUIRED_DEVICE_EXTENSIONS) {
            bool found = false;
            for (const auto& ext : available_extensions) {
                if (std::strcmp(ext.extensionName, required) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                all_extensions_found = false;
                break;
            }
        }

        if (!all_extensions_found) {
            continue;
        }

        m_physical_device = device;
        m_graphics_queue_family = graphics_family;

        auto props = device.getProperties();
        GOGGLES_LOG_INFO("Selected GPU: {}", props.deviceName.data());
        return {};
    }

    return make_error<void>(ErrorCode::vulkan_init_failed,
                            "No suitable GPU found with DMA-BUF support");
}

auto VulkanBackend::create_device() -> Result<void> {
    float queue_priority = 1.0F;
    vk::DeviceQueueCreateInfo queue_info{};
    queue_info.queueFamilyIndex = m_graphics_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    vk::PhysicalDeviceVulkan11Features vk11_features{};
    vk::PhysicalDeviceVulkan12Features vk12_features{};
    vk::PhysicalDeviceVulkan13Features vk13_features{};
    vk11_features.pNext = &vk12_features;
    vk12_features.pNext = &vk13_features;
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &vk11_features;
    m_physical_device.getFeatures2(&features2);

    if (!vk11_features.shaderDrawParameters) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "shaderDrawParameters not supported (required for vertex shaders)");
    }
    if (!vk12_features.timelineSemaphore) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Timeline semaphores not supported (required for frame sync)");
    }
    if (!vk13_features.dynamicRendering) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Dynamic rendering not supported (required for Vulkan 1.3)");
    }

    vk::PhysicalDeviceVulkan11Features vk11_enable{};
    vk11_enable.shaderDrawParameters = VK_TRUE;
    vk::PhysicalDeviceVulkan12Features vk12_enable{};
    vk12_enable.timelineSemaphore = VK_TRUE;
    vk::PhysicalDeviceVulkan13Features vk13_enable{};
    vk13_enable.dynamicRendering = VK_TRUE;
    vk11_enable.pNext = &vk12_enable;
    vk12_enable.pNext = &vk13_enable;

    vk::DeviceCreateInfo create_info{};
    create_info.pNext = &vk11_enable;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size());
    create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();

    auto [result, device] = m_physical_device.createDeviceUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create logical device: " + vk::to_string(result));
    }

    m_device = std::move(device);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_device);
    m_graphics_queue = m_device->getQueue(m_graphics_queue_family, 0);

    GOGGLES_LOG_DEBUG("Vulkan device created");
    return {};
}

auto VulkanBackend::create_swapchain(uint32_t width, uint32_t height, vk::Format preferred_format)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    auto [cap_result, capabilities] = m_physical_device.getSurfaceCapabilitiesKHR(*m_surface);
    if (cap_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to query surface capabilities");
    }

    auto [fmt_result, formats] = m_physical_device.getSurfaceFormatsKHR(*m_surface);
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
    create_info.surface = *m_surface;
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

    auto [pm_result, present_modes] = m_physical_device.getSurfacePresentModesKHR(*m_surface);
    vk::PresentModeKHR chosen_mode = vk::PresentModeKHR::eFifo;
    for (auto mode : present_modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            chosen_mode = vk::PresentModeKHR::eMailbox;
            break;
        }
    }
    create_info.presentMode = chosen_mode;
    create_info.clipped = VK_TRUE;

    auto [result, swapchain] = m_device->createSwapchainKHRUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create swapchain: " + vk::to_string(result));
    }

    m_swapchain = std::move(swapchain);
    m_swapchain_format = chosen_format.format;
    m_swapchain_extent = extent;

    auto [img_result, images] = m_device->getSwapchainImagesKHR(*m_swapchain);
    if (img_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to get swapchain images");
    }
    m_swapchain_images = std::move(images);

    m_swapchain_image_views.reserve(m_swapchain_images.size());
    for (auto image : m_swapchain_images) {
        vk::ImageViewCreateInfo view_info{};
        view_info.image = image;
        view_info.viewType = vk::ImageViewType::e2D;
        view_info.format = m_swapchain_format;
        view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        auto [view_result, view] = m_device->createImageViewUnique(view_info);
        if (view_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create image view");
        }
        m_swapchain_image_views.push_back(std::move(view));
    }

    GOGGLES_LOG_DEBUG("Swapchain created: {}x{}, {} images", extent.width, extent.height,
                      m_swapchain_images.size());
    return {};
}

void VulkanBackend::cleanup_swapchain() {
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();
    m_swapchain.reset();
}

auto VulkanBackend::recreate_swapchain() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_window, &width, &height);

    while (width == 0 || height == 0) {
        SDL_GetWindowSize(m_window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    VK_TRY(m_device->waitIdle(), ErrorCode::vulkan_device_lost,
           "waitIdle failed before swapchain recreation");
    cleanup_swapchain();

    GOGGLES_TRY(create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 m_swapchain_format));
    GOGGLES_TRY(m_filter_chain->handle_resize(m_swapchain_extent));

    m_needs_resize = false;
    GOGGLES_LOG_DEBUG("Swapchain recreated: {}x{}", width, height);
    return {};
}

auto VulkanBackend::recreate_swapchain_for_format(vk::Format source_format) -> Result<void> {
    vk::Format target_format = get_matching_swapchain_format(source_format);
    if (target_format == m_swapchain_format) {
        return {};
    }

    GOGGLES_LOG_INFO("Source format changed to {}, recreating swapchain with {}",
                     vk::to_string(source_format), vk::to_string(target_format));

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_window, &width, &height);

    VK_TRY(m_device->waitIdle(), ErrorCode::vulkan_device_lost,
           "waitIdle failed before swapchain format change");
    if (m_filter_chain) {
        m_filter_chain->shutdown();
    }
    cleanup_swapchain();

    GOGGLES_TRY(create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 target_format));

    GOGGLES_TRY(init_filter_chain());

    if (!m_preset_path.empty()) {
        auto result = m_filter_chain->load_preset(m_preset_path);
        if (!result) {
            GOGGLES_LOG_WARN("Failed to reload shader preset after format change: {}",
                             result.error().message);
        }
    }

    m_format_changed.store(true, std::memory_order_release);
    return {};
}

auto VulkanBackend::get_matching_swapchain_format(vk::Format source_format) -> vk::Format {
    if (is_srgb_format(source_format)) {
        return vk::Format::eB8G8R8A8Srgb;
    }
    return vk::Format::eB8G8R8A8Unorm;
}

auto VulkanBackend::is_srgb_format(vk::Format format) -> bool {
    switch (format) {
    case vk::Format::eR8Srgb:
    case vk::Format::eR8G8Srgb:
    case vk::Format::eR8G8B8Srgb:
    case vk::Format::eB8G8R8Srgb:
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eB8G8R8A8Srgb:
    case vk::Format::eA8B8G8R8SrgbPack32:
        return true;
    default:
        return false;
    }
}

auto VulkanBackend::create_command_resources() -> Result<void> {
    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = m_graphics_queue_family;

    auto [pool_result, pool] = m_device->createCommandPoolUnique(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create command pool");
    }
    m_command_pool = std::move(pool);

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = *m_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    auto [alloc_result, buffers] = m_device->allocateCommandBuffers(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate command buffers");
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_frames[i].command_buffer = buffers[i];
    }

    GOGGLES_LOG_DEBUG("Command pool and {} buffers created", MAX_FRAMES_IN_FLIGHT);
    return {};
}

auto VulkanBackend::create_sync_objects() -> Result<void> {
    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
    vk::SemaphoreCreateInfo sem_info{};

    for (auto& frame : m_frames) {
        {
            auto [result, fence] = m_device->createFence(fence_info);
            if (result != vk::Result::eSuccess) {
                return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create fence");
            }
            frame.in_flight_fence = fence;
        }
        {
            auto [result, sem] = m_device->createSemaphore(sem_info);
            if (result != vk::Result::eSuccess) {
                return make_error<void>(ErrorCode::vulkan_init_failed,
                                        "Failed to create semaphore");
            }
            frame.image_available_sem = sem;
        }
    }

    m_render_finished_sems.resize(m_swapchain_images.size());
    for (auto& sem : m_render_finished_sems) {
        auto [result, new_sem] = m_device->createSemaphore(sem_info);
        if (result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_init_failed,
                                    "Failed to create render finished semaphore");
        }
        sem = new_sem;
    }

    GOGGLES_LOG_DEBUG("Sync objects created");
    return {};
}

auto VulkanBackend::init_filter_chain() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    m_shader_runtime = GOGGLES_TRY(ShaderRuntime::create());

    VulkanContext vk_ctx{.device = *m_device,
                         .physical_device = m_physical_device,
                         .command_pool = *m_command_pool,
                         .graphics_queue = m_graphics_queue};
    m_filter_chain = GOGGLES_TRY(FilterChain::create(
        vk_ctx, m_swapchain_format, MAX_FRAMES_IN_FLIGHT, *m_shader_runtime, m_shader_dir));
    return {};
}

void VulkanBackend::load_shader_preset(const std::filesystem::path& preset_path) {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_filter_chain) {
        GOGGLES_LOG_WARN("Cannot load shader preset: VulkanBackend not initialized");
        return;
    }

    m_preset_path = preset_path;

    if (preset_path.empty()) {
        GOGGLES_LOG_DEBUG("No shader preset specified, using passthrough mode");
        return;
    }

    auto result = m_filter_chain->load_preset(preset_path);
    if (!result) {
        GOGGLES_LOG_WARN("Failed to load shader preset '{}': {} - falling back to passthrough",
                         preset_path.string(), result.error().message);
    }
}

auto VulkanBackend::import_dmabuf(const CaptureFrame& frame) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!frame.dmabuf_fd.valid()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Invalid DMA-BUF fd");
    }

    VK_TRY(m_device->waitIdle(), ErrorCode::vulkan_device_lost, "waitIdle failed before reimport");
    cleanup_imported_image();

    // Set up external memory info for DMA-BUF import
    vk::ExternalMemoryImageCreateInfo ext_mem_info{};
    ext_mem_info.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;

    // Set up modifier info - always use DRM format modifier tiling for proper import
    vk::ImageDrmFormatModifierExplicitCreateInfoEXT modifier_info{};
    vk::SubresourceLayout plane_layout{};

    // For single-plane images, provide a minimal plane layout
    // The actual layout is determined by the exporter and encoded in the modifier
    plane_layout.offset = 0;
    plane_layout.size = 0;
    plane_layout.rowPitch = frame.stride;
    plane_layout.arrayPitch = 0;
    plane_layout.depthPitch = 0;

    modifier_info.drmFormatModifier = frame.modifier;
    modifier_info.drmFormatModifierPlaneCount = 1;
    modifier_info.pPlaneLayouts = &plane_layout;

    // Chain: image_info -> ext_mem_info -> modifier_info
    ext_mem_info.pNext = &modifier_info;

    auto vk_format = static_cast<vk::Format>(frame.format);

    vk::ImageCreateInfo image_info{};
    image_info.pNext = &ext_mem_info;
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = vk_format;
    image_info.extent = vk::Extent3D{frame.width, frame.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eDrmFormatModifierEXT;
    image_info.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    auto [img_result, image] = m_device->createImage(image_info);
    if (img_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create DMA-BUF image: " + vk::to_string(img_result));
    }
    m_import.image = image;

    vk::ImageMemoryRequirementsInfo2 mem_reqs_info{};
    mem_reqs_info.image = m_import.image;

    vk::MemoryDedicatedRequirements dedicated_reqs{};
    vk::MemoryRequirements2 mem_reqs2{};
    mem_reqs2.pNext = &dedicated_reqs;

    m_device->getImageMemoryRequirements2(&mem_reqs_info, &mem_reqs2);
    auto mem_reqs = mem_reqs2.memoryRequirements;

    VkMemoryFdPropertiesKHR fd_props_raw{};
    fd_props_raw.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    auto fd_props_result =
        static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetMemoryFdPropertiesKHR(
            *m_device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, frame.dmabuf_fd.get(),
            &fd_props_raw));
    if (fd_props_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed, "Stale DMA-BUF fd, skipping frame");
    }
    vk::MemoryFdPropertiesKHR fd_props = fd_props_raw;

    auto mem_props = m_physical_device.getMemoryProperties();
    uint32_t combined_bits = mem_reqs.memoryTypeBits & fd_props.memoryTypeBits;
    uint32_t mem_type_index = find_memory_type(mem_props, combined_bits);

    if (mem_type_index == UINT32_MAX) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable memory type for DMA-BUF import");
    }

    // Vulkan takes ownership of fd on success
    auto import_fd = frame.dmabuf_fd.dup();
    if (!import_fd) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to dup DMA-BUF fd");
    }

    vk::ImportMemoryFdInfoKHR import_info{};
    import_info.handleType = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;
    import_info.fd = import_fd.get();

    vk::MemoryDedicatedAllocateInfo dedicated_alloc{};
    dedicated_alloc.image = m_import.image;

    if (dedicated_reqs.requiresDedicatedAllocation || dedicated_reqs.prefersDedicatedAllocation) {
        import_info.pNext = &dedicated_alloc;
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.pNext = &import_info;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type_index;

    auto [alloc_result, memory] = m_device->allocateMemory(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to import DMA-BUF memory: " + vk::to_string(alloc_result));
    }
    import_fd.release();
    m_import.memory = memory;

    auto bind_result = m_device->bindImageMemory(m_import.image, m_import.memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to bind DMA-BUF memory: " + vk::to_string(bind_result));
    }

    vk::ImageViewCreateInfo view_info{};
    view_info.image = m_import.image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk_format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = m_device->createImageView(view_info);
    if (view_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create DMA-BUF image view: " +
                                    vk::to_string(view_result));
    }
    m_import.view = view;
    m_import_extent = vk::Extent2D{frame.width, frame.height};

    GOGGLES_LOG_TRACE("DMA-BUF imported: {}x{}, format={}, modifier=0x{:x}", frame.width,
                      frame.height, vk::to_string(vk_format), frame.modifier);
    return {};
}

void VulkanBackend::cleanup_imported_image() {
    if (m_device) {
        if (m_import.view) {
            m_device->destroyImageView(m_import.view);
            m_import.view = nullptr;
        }
        if (m_import.memory) {
            m_device->freeMemory(m_import.memory);
            m_import.memory = nullptr;
        }
        if (m_import.image) {
            m_device->destroyImage(m_import.image);
            m_import.image = nullptr;
        }
    }
}

auto VulkanBackend::import_sync_semaphores(util::UniqueFd frame_ready_fd,
                                           util::UniqueFd frame_consumed_fd) -> Result<void> {
    cleanup_sync_semaphores();

    vk::SemaphoreTypeCreateInfo timeline_info{};
    timeline_info.semaphoreType = vk::SemaphoreType::eTimeline;
    timeline_info.initialValue = 0;

    vk::SemaphoreCreateInfo sem_info{};
    sem_info.pNext = &timeline_info;

    auto [res1, ready_sem] = m_device->createSemaphore(sem_info);
    VK_TRY(res1, ErrorCode::vulkan_init_failed, "Failed to create frame_ready semaphore");

    auto [res2, consumed_sem] = m_device->createSemaphore(sem_info);
    if (res2 != vk::Result::eSuccess) {
        m_device->destroySemaphore(ready_sem);
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create frame_consumed semaphore");
    }

    VkImportSemaphoreFdInfoKHR import_info{};
    import_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    import_info.flags = 0;

    import_info.semaphore = ready_sem;
    import_info.fd = frame_ready_fd.get();
    auto import_res = static_cast<vk::Result>(
        VULKAN_HPP_DEFAULT_DISPATCHER.vkImportSemaphoreFdKHR(*m_device, &import_info));
    if (import_res != vk::Result::eSuccess) {
        m_device->destroySemaphore(ready_sem);
        m_device->destroySemaphore(consumed_sem);
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to import frame_ready semaphore FD");
    }
    frame_ready_fd.release();

    import_info.semaphore = consumed_sem;
    import_info.fd = frame_consumed_fd.get();
    import_res = static_cast<vk::Result>(
        VULKAN_HPP_DEFAULT_DISPATCHER.vkImportSemaphoreFdKHR(*m_device, &import_info));
    if (import_res != vk::Result::eSuccess) {
        m_device->destroySemaphore(ready_sem);
        m_device->destroySemaphore(consumed_sem);
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to import frame_consumed semaphore FD");
    }
    frame_consumed_fd.release();

    m_frame_ready_sem = ready_sem;
    m_frame_consumed_sem = consumed_sem;
    m_sync_semaphores_imported = true;
    m_last_frame_number = 0;

    GOGGLES_LOG_INFO("Cross-process sync semaphores imported");
    return {};
}

void VulkanBackend::cleanup_sync_semaphores() {
    if (m_device) {
        if (m_frame_ready_sem || m_frame_consumed_sem) {
            static_cast<void>(m_device->waitIdle());
        }
        if (m_frame_ready_sem) {
            m_device->destroySemaphore(m_frame_ready_sem);
            m_frame_ready_sem = nullptr;
        }
        if (m_frame_consumed_sem) {
            m_device->destroySemaphore(m_frame_consumed_sem);
            m_frame_consumed_sem = nullptr;
        }
    }
    m_sync_semaphores_imported = false;
    m_last_frame_number = 0;
    m_last_signaled_frame = 0;
}

auto VulkanBackend::acquire_next_image() -> Result<uint32_t> {
    GOGGLES_PROFILE_SCOPE("AcquireImage");

    auto& frame = m_frames[m_current_frame];

    auto wait_result = m_device->waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost, "Fence wait failed");
    }

    uint32_t image_index = 0;
    auto result = static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(
        *m_device, *m_swapchain, UINT64_MAX, frame.image_available_sem, nullptr, &image_index));

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        m_needs_resize = true;
        if (result == vk::Result::eErrorOutOfDateKHR) {
            return make_error<uint32_t>(ErrorCode::vulkan_init_failed, "Swapchain out of date");
        }
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost,
                                    "Failed to acquire swapchain image: " + vk::to_string(result));
    }

    auto reset_result = m_device->resetFences(frame.in_flight_fence);
    if (reset_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost,
                                    "Fence reset failed: " + vk::to_string(reset_result));
    }
    return image_index;
}

auto VulkanBackend::record_render_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                           const UiRenderCallback& ui_callback) -> Result<void> {
    GOGGLES_PROFILE_SCOPE("RecordCommands");

    VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

    vk::ImageMemoryBarrier src_barrier{};
    src_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    src_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    src_barrier.oldLayout = vk::ImageLayout::eUndefined;
    src_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = m_import.image;
    src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    src_barrier.subresourceRange.baseMipLevel = 0;
    src_barrier.subresourceRange.levelCount = 1;
    src_barrier.subresourceRange.baseArrayLayer = 0;
    src_barrier.subresourceRange.layerCount = 1;

    vk::ImageMemoryBarrier dst_barrier{};
    dst_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    dst_barrier.oldLayout = vk::ImageLayout::eUndefined;
    dst_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.image = m_swapchain_images[image_index];
    dst_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    dst_barrier.subresourceRange.baseMipLevel = 0;
    dst_barrier.subresourceRange.levelCount = 1;
    dst_barrier.subresourceRange.baseArrayLayer = 0;
    dst_barrier.subresourceRange.layerCount = 1;

    std::array barriers = {src_barrier, dst_barrier};
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eFragmentShader |
                            vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {}, {}, {}, barriers);

    m_filter_chain->record(cmd, m_import.image, m_import.view, m_import_extent,
                           *m_swapchain_image_views[image_index], m_swapchain_extent,
                           m_current_frame, m_scale_mode, m_integer_scale);

    if (ui_callback) {
        ui_callback(cmd, *m_swapchain_image_views[image_index], m_swapchain_extent);
    }

    dst_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    dst_barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    dst_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, dst_barrier);

    VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");

    return {};
}

auto VulkanBackend::record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                          const UiRenderCallback& ui_callback) -> Result<void> {
    VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

    vk::ImageMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_swapchain_images[image_index];
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, barrier);

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = *m_swapchain_image_views[image_index];
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.clearValue.color = vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}};

    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = m_swapchain_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    cmd.beginRendering(rendering_info);
    cmd.endRendering();

    if (ui_callback) {
        ui_callback(cmd, *m_swapchain_image_views[image_index], m_swapchain_extent);
    }

    barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);

    VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");

    return {};
}

auto VulkanBackend::submit_and_present(uint32_t image_index) -> Result<bool> {
    GOGGLES_PROFILE_SCOPE("SubmitPresent");

    auto& frame = m_frames[m_current_frame];
    vk::Semaphore render_finished_sem = m_render_finished_sems[image_index];

    if (m_sync_semaphores_imported && m_last_frame_number > 0) {
        vk::SemaphoreWaitInfo wait_info{};
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &m_frame_ready_sem;
        wait_info.pValues = &m_last_frame_number;

        constexpr uint64_t TIMEOUT_NS = 100'000'000;
        auto wait_result = m_device->waitSemaphores(wait_info, TIMEOUT_NS);
        if (wait_result == vk::Result::eTimeout) {
            GOGGLES_LOG_WARN("Timeout waiting for frame_ready semaphore, layer disconnected?");
            cleanup_sync_semaphores();
        } else if (wait_result != vk::Result::eSuccess) {
            return make_error<bool>(ErrorCode::vulkan_device_lost,
                                    "Semaphore wait failed: " + vk::to_string(wait_result));
        }
    }

    std::array<vk::Semaphore, 1> wait_sems = {frame.image_available_sem};
    std::array<vk::PipelineStageFlags, 1> wait_stages = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::SubmitInfo submit_info{};
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_sems.size());
    submit_info.pWaitSemaphores = wait_sems.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;

    std::array<vk::Semaphore, 2> signal_sems;
    std::array<uint64_t, 2> signal_values = {0, 0};
    vk::TimelineSemaphoreSubmitInfo timeline_info{};

    bool should_signal_timeline =
        m_sync_semaphores_imported && m_last_frame_number > m_last_signaled_frame;

    if (should_signal_timeline) {
        signal_sems = {render_finished_sem, m_frame_consumed_sem};
        signal_values = {0, m_last_frame_number};
        timeline_info.signalSemaphoreValueCount = static_cast<uint32_t>(signal_values.size());
        timeline_info.pSignalSemaphoreValues = signal_values.data();
        submit_info.pNext = &timeline_info;
        submit_info.signalSemaphoreCount = 2;
    } else {
        signal_sems[0] = render_finished_sem;
        submit_info.signalSemaphoreCount = 1;
    }
    submit_info.pSignalSemaphores = signal_sems.data();

    auto submit_result = m_graphics_queue.submit(submit_info, frame.in_flight_fence);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<bool>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_result));
    }

    if (should_signal_timeline) {
        m_last_signaled_frame = m_last_frame_number;
    }

    vk::PresentInfoKHR present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_sem;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &*m_swapchain;
    present_info.pImageIndices = &image_index;

    auto present_result = m_graphics_queue.presentKHR(present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        m_needs_resize = true;
    }
    if (present_result != vk::Result::eSuccess && present_result != vk::Result::eSuboptimalKHR &&
        present_result != vk::Result::eErrorOutOfDateKHR) {
        return make_error<bool>(ErrorCode::vulkan_device_lost,
                                "Present failed: " + vk::to_string(present_result));
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return !m_needs_resize;
}

auto VulkanBackend::render_frame(const CaptureFrame& frame) -> Result<bool> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device) {
        return make_error<bool>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    ++m_frame_count;
    check_pending_chain_swap();
    cleanup_deferred_destroys();

    m_last_frame_number = frame.frame_number;

    auto vk_format = static_cast<vk::Format>(frame.format);
    if (m_source_format != vk_format) {
        GOGGLES_TRY(recreate_swapchain_for_format(vk_format));
        m_source_format = vk_format;
    }

    GOGGLES_TRY(import_dmabuf(frame));

    uint32_t image_index = GOGGLES_TRY(acquire_next_image());

    GOGGLES_TRY(record_render_commands(m_frames[m_current_frame].command_buffer, image_index));

    return submit_and_present(image_index);
}

auto VulkanBackend::render_clear() -> Result<bool> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device) {
        return make_error<bool>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    uint32_t image_index = GOGGLES_TRY(acquire_next_image());

    GOGGLES_TRY(record_clear_commands(m_frames[m_current_frame].command_buffer, image_index));

    return submit_and_present(image_index);
}

auto VulkanBackend::handle_resize() -> Result<void> {
    if (!m_device) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    return recreate_swapchain();
}

auto VulkanBackend::render_frame_with_ui(const CaptureFrame& frame,
                                         const UiRenderCallback& ui_callback) -> Result<bool> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device) {
        return make_error<bool>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    ++m_frame_count;
    check_pending_chain_swap();
    cleanup_deferred_destroys();

    m_last_frame_number = frame.frame_number;

    auto vk_format = static_cast<vk::Format>(frame.format);
    if (m_source_format != vk_format) {
        GOGGLES_TRY(recreate_swapchain_for_format(vk_format));
        m_source_format = vk_format;
    }

    GOGGLES_TRY(import_dmabuf(frame));

    uint32_t image_index = GOGGLES_TRY(acquire_next_image());

    GOGGLES_TRY(
        record_render_commands(m_frames[m_current_frame].command_buffer, image_index, ui_callback));

    return submit_and_present(image_index);
}

auto VulkanBackend::render_clear_with_ui(const UiRenderCallback& ui_callback) -> Result<bool> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device) {
        return make_error<bool>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    ++m_frame_count;
    check_pending_chain_swap();
    cleanup_deferred_destroys();

    uint32_t image_index = GOGGLES_TRY(acquire_next_image());

    GOGGLES_TRY(
        record_clear_commands(m_frames[m_current_frame].command_buffer, image_index, ui_callback));

    return submit_and_present(image_index);
}

auto VulkanBackend::reload_shader_preset(const std::filesystem::path& preset_path) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device || !m_filter_chain) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    if (m_pending_chain_ready.load(std::memory_order_acquire)) {
        GOGGLES_LOG_WARN("Shader reload already pending, ignoring request");
        return {};
    }

    if (m_pending_load_future.valid() &&
        m_pending_load_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        GOGGLES_LOG_WARN("Shader compilation in progress, ignoring request");
        return {};
    }

    m_pending_preset_path = preset_path;

    // Capture values needed by the async task
    auto swapchain_format = m_swapchain_format;
    auto shader_dir = m_shader_dir;
    auto device = *m_device;
    auto physical_device = m_physical_device;
    auto command_pool = *m_command_pool;
    auto graphics_queue = m_graphics_queue;

    m_pending_load_future = util::JobSystem::submit([=, this]() -> Result<void> {
        GOGGLES_PROFILE_SCOPE("AsyncShaderLoad");

        auto runtime_result = ShaderRuntime::create();
        if (!runtime_result) {
            GOGGLES_LOG_ERROR("Failed to create shader runtime: {}",
                              runtime_result.error().message);
            return make_error<void>(runtime_result.error().code, runtime_result.error().message);
        }

        VulkanContext vk_ctx{
            .device = device,
            .physical_device = physical_device,
            .command_pool = command_pool,
            .graphics_queue = graphics_queue,
        };

        auto chain_result = FilterChain::create(vk_ctx, swapchain_format, MAX_FRAMES_IN_FLIGHT,
                                                *runtime_result.value(), shader_dir);
        if (!chain_result) {
            GOGGLES_LOG_ERROR("Failed to create filter chain: {}", chain_result.error().message);
            return make_error<void>(chain_result.error().code, chain_result.error().message);
        }

        if (!preset_path.empty()) {
            auto load_result = chain_result.value()->load_preset(preset_path);
            if (!load_result) {
                GOGGLES_LOG_ERROR("Failed to load preset '{}': {}", preset_path.string(),
                                  load_result.error().message);
                return make_error<void>(load_result.error().code, load_result.error().message);
            }
        }

        m_pending_shader_runtime = std::move(runtime_result.value());
        m_pending_filter_chain = std::move(chain_result.value());
        m_pending_chain_ready.store(true, std::memory_order_release);

        GOGGLES_LOG_INFO("Shader preset compiled: {}",
                         preset_path.empty() ? "(passthrough)" : preset_path.string());
        return {};
    });

    return {};
}

void VulkanBackend::check_pending_chain_swap() {
    if (!m_pending_chain_ready.load(std::memory_order_acquire)) {
        return;
    }

    // Check if async task completed successfully
    if (m_pending_load_future.valid()) {
        auto result = m_pending_load_future.get();
        if (!result) {
            GOGGLES_LOG_ERROR("Async shader load failed: {}", result.error().message);
            m_pending_filter_chain.reset();
            m_pending_shader_runtime.reset();
            m_pending_chain_ready.store(false, std::memory_order_release);
            return;
        }
    }

    // Queue old chain for deferred destruction
    if (m_deferred_count < MAX_DEFERRED_DESTROYS) {
        m_deferred_destroys[m_deferred_count++] = {
            .chain = std::move(m_filter_chain),
            .runtime = std::move(m_shader_runtime),
            .destroy_after_frame = m_frame_count + MAX_FRAMES_IN_FLIGHT + 1,
        };
    } else {
        GOGGLES_LOG_WARN("Deferred destroy queue full, destroying immediately");
        m_filter_chain.reset();
        m_shader_runtime.reset();
    }

    // Swap in the new chain
    m_filter_chain = std::move(m_pending_filter_chain);
    m_shader_runtime = std::move(m_pending_shader_runtime);
    m_preset_path = m_pending_preset_path;
    m_pending_chain_ready.store(false, std::memory_order_release);
    m_chain_swapped.store(true, std::memory_order_release);

    GOGGLES_LOG_INFO("Shader chain swapped: {}",
                     m_preset_path.empty() ? "(passthrough)" : m_preset_path.string());
}

void VulkanBackend::cleanup_deferred_destroys() {
    size_t write_idx = 0;
    for (size_t i = 0; i < m_deferred_count; ++i) {
        if (m_frame_count >= m_deferred_destroys[i].destroy_after_frame) {
            GOGGLES_LOG_DEBUG("Destroying deferred filter chain");
            m_deferred_destroys[i] = {};
        } else {
            if (write_idx != i) {
                m_deferred_destroys[write_idx] = std::move(m_deferred_destroys[i]);
            }
            ++write_idx;
        }
    }
    m_deferred_count = write_idx;
}

void VulkanBackend::set_shader_enabled(bool enabled) {
    if (m_filter_chain) {
        m_filter_chain->set_bypass(!enabled);
    }
}

} // namespace goggles::render
