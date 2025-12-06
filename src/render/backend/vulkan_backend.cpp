#include "vulkan_backend.hpp"

#include "vulkan_error.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <render/chain/pass.hpp>
#include <unistd.h>
#include <util/logging.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace goggles::render {

namespace {

constexpr std::array REQUIRED_INSTANCE_EXTENSIONS = {
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
};

constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
};

} // namespace

VulkanBackend::~VulkanBackend() {
    shutdown();
}

auto VulkanBackend::init(SDL_Window* window, bool enable_validation,
                         const std::filesystem::path& shader_dir) -> Result<void> {
    if (m_initialized) {
        return {};
    }

    auto vk_get_instance_proc_addr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (vk_get_instance_proc_addr == nullptr) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to get vkGetInstanceProcAddr from SDL");
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

    m_window = window;
    m_enable_validation = enable_validation;
    m_shader_dir = shader_dir;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    GOGGLES_TRY(create_instance(enable_validation));
    GOGGLES_TRY(create_debug_messenger());
    GOGGLES_TRY(create_surface(window));
    GOGGLES_TRY(select_physical_device());
    GOGGLES_TRY(create_device());
    GOGGLES_TRY(create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 vk::Format::eB8G8R8A8Srgb));
    GOGGLES_TRY(create_command_resources());
    GOGGLES_TRY(create_sync_objects());
    GOGGLES_TRY(create_render_pass());
    GOGGLES_TRY(create_framebuffers());
    GOGGLES_TRY(init_output_pass());

    m_initialized = true;
    GOGGLES_LOG_INFO("Vulkan backend initialized: {}x{}", width, height);
    return {};
}

void VulkanBackend::shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_device) {
        auto wait_result = m_device->waitIdle();
        if (wait_result != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("waitIdle failed during shutdown: {}", vk::to_string(wait_result));
        }
    }

    m_output_pass.shutdown();
    m_shader_runtime.shutdown();
    cleanup_imported_image();

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
    m_framebuffers.clear();
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    m_command_pool.reset();
    m_render_pass.reset();
    m_swapchain.reset();
    m_device.reset();
    m_surface.reset();
    m_debug_messenger.reset();
    m_instance.reset();

    m_initialized = false;
    GOGGLES_LOG_INFO("Vulkan backend shutdown");
}

auto VulkanBackend::create_instance(bool enable_validation) -> Result<void> {
    uint32_t sdl_ext_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
    if (sdl_extensions == nullptr) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
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
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Goggles";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    vk::InstanceCreateInfo create_info{};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    auto [result, instance] = vk::createInstanceUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
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
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }

    m_surface = vk::UniqueSurfaceKHR(raw_surface, *m_instance);
    GOGGLES_LOG_DEBUG("Vulkan surface created");
    return {};
}

auto VulkanBackend::select_physical_device() -> Result<void> {
    auto [result, devices] = m_instance->enumeratePhysicalDevices();
    if (result != vk::Result::eSuccess || devices.empty()) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "No Vulkan devices found");
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

    return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
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
    vk11_features.pNext = &vk12_features;
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &vk11_features;
    m_physical_device.getFeatures2(&features2);

    if (!vk11_features.shaderDrawParameters) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "shaderDrawParameters not supported (required for vertex shaders)");
    }
    if (!vk12_features.timelineSemaphore) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Timeline semaphores not supported (required for frame sync)");
    }

    vk::PhysicalDeviceVulkan11Features vk11_enable{};
    vk11_enable.shaderDrawParameters = VK_TRUE;
    vk::PhysicalDeviceVulkan12Features vk12_enable{};
    vk12_enable.timelineSemaphore = VK_TRUE;
    vk11_enable.pNext = &vk12_enable;

    vk::DeviceCreateInfo create_info{};
    create_info.pNext = &vk11_enable;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size());
    create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();

    auto [result, device] = m_physical_device.createDeviceUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
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
    auto [cap_result, capabilities] = m_physical_device.getSurfaceCapabilitiesKHR(*m_surface);
    if (cap_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to query surface capabilities");
    }

    auto [fmt_result, formats] = m_physical_device.getSurfaceFormatsKHR(*m_surface);
    if (fmt_result != vk::Result::eSuccess || formats.empty()) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to query surface formats");
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
    create_info.presentMode = vk::PresentModeKHR::eFifo;
    create_info.clipped = VK_TRUE;

    auto [result, swapchain] = m_device->createSwapchainKHRUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create swapchain: " + vk::to_string(result));
    }

    m_swapchain = std::move(swapchain);
    m_swapchain_format = chosen_format.format;
    m_swapchain_extent = extent;

    auto [img_result, images] = m_device->getSwapchainImagesKHR(*m_swapchain);
    if (img_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to get swapchain images");
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
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to create image view");
        }
        m_swapchain_image_views.push_back(std::move(view));
    }

    GOGGLES_LOG_DEBUG("Swapchain created: {}x{}, {} images", extent.width, extent.height,
                      m_swapchain_images.size());
    return {};
}

void VulkanBackend::cleanup_swapchain() {
    m_framebuffers.clear();
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();
    m_swapchain.reset();
}

auto VulkanBackend::recreate_swapchain() -> Result<void> {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_window, &width, &height);

    while (width == 0 || height == 0) {
        SDL_GetWindowSize(m_window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    VK_TRY(m_device->waitIdle(), ErrorCode::VULKAN_DEVICE_LOST,
           "waitIdle failed before swapchain recreation");
    cleanup_swapchain();

    GOGGLES_TRY(create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 m_swapchain_format));
    GOGGLES_TRY(create_framebuffers());

    m_needs_resize = false;
    GOGGLES_LOG_INFO("Swapchain recreated: {}x{}", width, height);
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

    VK_TRY(m_device->waitIdle(), ErrorCode::VULKAN_DEVICE_LOST,
           "waitIdle failed before swapchain format change");
    m_output_pass.shutdown();
    cleanup_swapchain();
    m_render_pass.reset();

    GOGGLES_TRY(create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 target_format));
    GOGGLES_TRY(create_render_pass());
    GOGGLES_TRY(create_framebuffers());

    return init_output_pass();
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
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to create command pool");
    }
    m_command_pool = std::move(pool);

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = *m_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    auto [alloc_result, buffers] = m_device->allocateCommandBuffers(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
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
                return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to create fence");
            }
            frame.in_flight_fence = fence;
        }
        {
            auto [result, sem] = m_device->createSemaphore(sem_info);
            if (result != vk::Result::eSuccess) {
                return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                        "Failed to create semaphore");
            }
            frame.image_available_sem = sem;
        }
    }

    m_render_finished_sems.resize(m_swapchain_images.size());
    for (auto& sem : m_render_finished_sems) {
        auto [result, new_sem] = m_device->createSemaphore(sem_info);
        if (result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                    "Failed to create render finished semaphore");
        }
        sem = new_sem;
    }

    GOGGLES_LOG_DEBUG("Sync objects created");
    return {};
}

auto VulkanBackend::create_render_pass() -> Result<void> {
    vk::AttachmentDescription color_attachment{};
    color_attachment.format = m_swapchain_format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo create_info{};
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    auto [result, render_pass] = m_device->createRenderPassUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create render pass: " + vk::to_string(result));
    }

    m_render_pass = std::move(render_pass);
    GOGGLES_LOG_DEBUG("Render pass created");
    return {};
}

auto VulkanBackend::create_framebuffers() -> Result<void> {
    m_framebuffers.reserve(m_swapchain_image_views.size());

    for (const auto& view : m_swapchain_image_views) {
        vk::FramebufferCreateInfo create_info{};
        create_info.renderPass = *m_render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &*view;
        create_info.width = m_swapchain_extent.width;
        create_info.height = m_swapchain_extent.height;
        create_info.layers = 1;

        auto [result, framebuffer] = m_device->createFramebufferUnique(create_info);
        if (result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                    "Failed to create framebuffer: " + vk::to_string(result));
        }
        m_framebuffers.push_back(std::move(framebuffer));
    }

    GOGGLES_LOG_DEBUG("Framebuffers created: {}", m_framebuffers.size());
    return {};
}

auto VulkanBackend::init_output_pass() -> Result<void> {
    GOGGLES_TRY(m_shader_runtime.init());

    return m_output_pass.init(*m_device, *m_render_pass, MAX_FRAMES_IN_FLIGHT, m_shader_runtime,
                              m_shader_dir);
}

auto VulkanBackend::import_dmabuf(const FrameInfo& frame) -> Result<void> {
    bool same_config = m_imported_image && m_current_import.width == frame.width &&
                       m_current_import.height == frame.height &&
                       m_current_import.format == frame.format &&
                       m_current_import_fd == frame.dmabuf_fd;
    if (same_config) {
        return {};
    }

    VK_TRY(m_device->waitIdle(), ErrorCode::VULKAN_DEVICE_LOST, "waitIdle failed before reimport");
    cleanup_imported_image();

    vk::ExternalMemoryImageCreateInfo ext_mem_info{};
    ext_mem_info.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;

    vk::ImageCreateInfo image_info{};
    image_info.pNext = &ext_mem_info;
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = frame.format;
    image_info.extent = vk::Extent3D{frame.width, frame.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eLinear;
    image_info.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    auto [img_result, image] = m_device->createImage(image_info);
    if (img_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create DMA-BUF image: " + vk::to_string(img_result));
    }
    m_imported_image = image;

    auto mem_reqs = m_device->getImageMemoryRequirements(m_imported_image);

    auto [fd_props_result, fd_props] = m_device->getMemoryFdPropertiesKHR(
        vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, frame.dmabuf_fd);
    if (fd_props_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to get DMA-BUF fd properties: " +
                                    vk::to_string(fd_props_result));
    }

    auto mem_props = m_physical_device.getMemoryProperties();
    uint32_t mem_type_index = UINT32_MAX;
    uint32_t combined_bits = mem_reqs.memoryTypeBits & fd_props.memoryTypeBits;

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (combined_bits & (1U << i)) {
            mem_type_index = i;
            break;
        }
    }

    if (mem_type_index == UINT32_MAX) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "No suitable memory type for DMA-BUF import");
    }

    // Vulkan takes ownership of fd on success
    int import_fd = dup(frame.dmabuf_fd);
    if (import_fd < 0) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to dup DMA-BUF fd");
    }

    vk::ImportMemoryFdInfoKHR import_info{};
    import_info.handleType = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;
    import_info.fd = import_fd;

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.pNext = &import_info;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type_index;

    auto [alloc_result, memory] = m_device->allocateMemory(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        close(import_fd);
        cleanup_imported_image();
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to import DMA-BUF memory: " + vk::to_string(alloc_result));
    }
    m_imported_memory = memory;

    auto bind_result = m_device->bindImageMemory(m_imported_image, m_imported_memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to bind DMA-BUF memory: " + vk::to_string(bind_result));
    }

    vk::ImageViewCreateInfo view_info{};
    view_info.image = m_imported_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = frame.format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = m_device->createImageView(view_info);
    if (view_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create DMA-BUF image view: " +
                                    vk::to_string(view_result));
    }
    m_imported_image_view = view;

    m_current_import = frame;
    m_current_import.dmabuf_fd = -1;
    m_current_import_fd = frame.dmabuf_fd;

    GOGGLES_LOG_DEBUG("DMA-BUF imported: {}x{}, format={}", frame.width, frame.height,
                      vk::to_string(frame.format));
    return {};
}

void VulkanBackend::cleanup_imported_image() {
    if (m_device) {
        if (m_imported_image_view) {
            m_device->destroyImageView(m_imported_image_view);
            m_imported_image_view = nullptr;
        }
        if (m_imported_memory) {
            m_device->freeMemory(m_imported_memory);
            m_imported_memory = nullptr;
        }
        if (m_imported_image) {
            m_device->destroyImage(m_imported_image);
            m_imported_image = nullptr;
        }
    }
    m_current_import = {};
    m_current_import_fd = -1;
}

auto VulkanBackend::acquire_next_image() -> Result<uint32_t> {
    auto& frame = m_frames[m_current_frame];

    auto wait_result = m_device->waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::VULKAN_DEVICE_LOST, "Fence wait failed");
    }

    auto [result, image_index] =
        m_device->acquireNextImageKHR(*m_swapchain, UINT64_MAX, frame.image_available_sem, nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        m_needs_resize = true;
        if (result == vk::Result::eErrorOutOfDateKHR) {
            return make_error<uint32_t>(ErrorCode::VULKAN_INIT_FAILED, "Swapchain out of date");
        }
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        return make_error<uint32_t>(ErrorCode::VULKAN_DEVICE_LOST,
                                    "Failed to acquire swapchain image: " + vk::to_string(result));
    }

    auto reset_result = m_device->resetFences(frame.in_flight_fence);
    if (reset_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::VULKAN_DEVICE_LOST,
                                    "Fence reset failed: " + vk::to_string(reset_result));
    }
    return image_index;
}

auto VulkanBackend::record_render_commands(vk::CommandBuffer cmd, uint32_t image_index)
    -> Result<void> {
    VK_TRY(cmd.reset(), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer begin failed");

    vk::ImageMemoryBarrier src_barrier{};
    src_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    src_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    src_barrier.oldLayout = vk::ImageLayout::eUndefined;
    src_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = m_imported_image;
    src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    src_barrier.subresourceRange.baseMipLevel = 0;
    src_barrier.subresourceRange.levelCount = 1;
    src_barrier.subresourceRange.baseArrayLayer = 0;
    src_barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, src_barrier);

    PassContext ctx{};
    ctx.frame_index = m_current_frame;
    ctx.output_extent = m_swapchain_extent;
    ctx.target_framebuffer = *m_framebuffers[image_index];
    ctx.source_texture = m_imported_image_view;
    ctx.original_texture = m_imported_image_view;

    m_output_pass.record(cmd, ctx);

    VK_TRY(cmd.end(), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer end failed");

    return {};
}

auto VulkanBackend::record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index)
    -> Result<void> {
    VK_TRY(cmd.reset(), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer reset failed");

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_TRY(cmd.begin(begin_info), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer begin failed");

    vk::ImageMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_swapchain_images[image_index];
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                        {}, {}, {}, barrier);

    vk::ClearColorValue clear_color{std::array{0.0F, 0.0F, 0.0F, 1.0F}};
    vk::ImageSubresourceRange range{};
    range.aspectMask = vk::ImageAspectFlagBits::eColor;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    cmd.clearColorImage(m_swapchain_images[image_index], vk::ImageLayout::eTransferDstOptimal,
                        clear_color, range);

    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);

    VK_TRY(cmd.end(), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer end failed");

    return {};
}

auto VulkanBackend::submit_and_present(uint32_t image_index) -> Result<bool> {
    auto& frame = m_frames[m_current_frame];
    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore render_finished_sem = m_render_finished_sems[image_index];

    vk::SubmitInfo submit_info{};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame.image_available_sem;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_sem;

    auto submit_result = m_graphics_queue.submit(submit_info, frame.in_flight_fence);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<bool>(ErrorCode::VULKAN_DEVICE_LOST,
                                "Queue submit failed: " + vk::to_string(submit_result));
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
        return make_error<bool>(ErrorCode::VULKAN_DEVICE_LOST,
                                "Present failed: " + vk::to_string(present_result));
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return !m_needs_resize;
}

auto VulkanBackend::render_frame(const FrameInfo& frame_info) -> Result<bool> {
    if (!m_initialized) {
        return make_error<bool>(ErrorCode::VULKAN_INIT_FAILED, "Backend not initialized");
    }

    if (m_source_format != frame_info.format) {
        GOGGLES_TRY(recreate_swapchain_for_format(frame_info.format));
        m_source_format = frame_info.format;
    }

    GOGGLES_TRY(import_dmabuf(frame_info));

    auto acquire_result = acquire_next_image();
    if (!acquire_result) {
        return nonstd::make_unexpected(acquire_result.error());
    }
    uint32_t image_index = acquire_result.value();

    GOGGLES_TRY(record_render_commands(m_frames[m_current_frame].command_buffer, image_index));

    return submit_and_present(image_index);
}

auto VulkanBackend::render_clear() -> Result<bool> {
    if (!m_initialized) {
        return make_error<bool>(ErrorCode::VULKAN_INIT_FAILED, "Backend not initialized");
    }

    auto acquire_result = acquire_next_image();
    if (!acquire_result) {
        return nonstd::make_unexpected(acquire_result.error());
    }
    uint32_t image_index = acquire_result.value();

    GOGGLES_TRY(record_clear_commands(m_frames[m_current_frame].command_buffer, image_index));

    return submit_and_present(image_index);
}

auto VulkanBackend::handle_resize() -> Result<void> {
    if (!m_initialized) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Backend not initialized");
    }

    if (m_needs_resize) {
        return recreate_swapchain();
    }
    return {};
}

} // namespace goggles::render
