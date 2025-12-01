#include "vulkan_backend.hpp"

#include <util/logging.hpp>

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <unistd.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace goggles::render {

namespace {

constexpr std::array REQUIRED_INSTANCE_EXTENSIONS = {
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
};

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

auto VulkanBackend::init(SDL_Window* window) -> Result<void> {
    if (m_initialized) {
        return {};
    }

    auto vk_get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (vk_get_instance_proc_addr == nullptr) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to get vkGetInstanceProcAddr from SDL");
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

    m_window = window;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    auto result = create_instance();
    if (!result) return result;

    result = create_surface(window);
    if (!result) return result;

    result = select_physical_device();
    if (!result) return result;

    result = create_device();
    if (!result) return result;

    result = create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    if (!result) return result;

    result = create_command_resources();
    if (!result) return result;

    result = create_sync_objects();
    if (!result) return result;

    m_initialized = true;
    GOGGLES_LOG_INFO("Vulkan backend initialized: {}x{}", width, height);
    return {};
}

void VulkanBackend::shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_device) {
        static_cast<void>(m_device->waitIdle());
    }

    cleanup_imported_image();

    if (m_device) {
        for (auto fence : m_in_flight_fences) {
            m_device->destroyFence(fence);
        }
        for (auto sem : m_image_available_sems) {
            m_device->destroySemaphore(sem);
        }
        for (auto sem : m_render_finished_sems) {
            m_device->destroySemaphore(sem);
        }
    }
    m_in_flight_fences.clear();
    m_image_available_sems.clear();
    m_render_finished_sems.clear();
    m_command_buffers.clear();
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    m_command_pool.reset();
    m_swapchain.reset();
    m_device.reset();
    m_surface.reset();
    m_instance.reset();

    m_initialized = false;
    GOGGLES_LOG_INFO("Vulkan backend shutdown");
}

auto VulkanBackend::create_instance() -> Result<void> {
    uint32_t sdl_ext_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
    if (sdl_extensions == nullptr) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }

    std::vector<const char*> extensions(sdl_extensions, sdl_extensions + sdl_ext_count);
    for (const auto* ext : REQUIRED_INSTANCE_EXTENSIONS) {
        if (std::find(extensions.begin(), extensions.end(), std::string_view(ext)) == extensions.end()) {
            extensions.push_back(ext);
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

    auto [result, instance] = vk::createInstanceUnique(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to create Vulkan instance: " + vk::to_string(result));
    }

    m_instance = std::move(instance);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_instance);

    GOGGLES_LOG_DEBUG("Vulkan instance created with {} extensions", extensions.size());
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

    vk::PhysicalDeviceFeatures features{};

    vk::DeviceCreateInfo create_info{};
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size());
    create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();
    create_info.pEnabledFeatures = &features;

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

auto VulkanBackend::create_swapchain(uint32_t width, uint32_t height) -> Result<void> {
    auto [cap_result, capabilities] = m_physical_device.getSurfaceCapabilitiesKHR(*m_surface);
    if (cap_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to query surface capabilities");
    }

    auto [fmt_result, formats] = m_physical_device.getSurfaceFormatsKHR(*m_surface);
    if (fmt_result != vk::Result::eSuccess || formats.empty()) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to query surface formats");
    }

    vk::SurfaceFormatKHR chosen_format = formats[0];
    for (const auto& format : formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            chosen_format = format;
            break;
        }
    }

    vk::Extent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::clamp(width, capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
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
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                             vk::ImageUsageFlagBits::eTransferDst;
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

    static_cast<void>(m_device->waitIdle());
    cleanup_swapchain();

    auto result = create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    if (!result) {
        return result;
    }

    m_needs_resize = false;
    GOGGLES_LOG_INFO("Swapchain recreated: {}x{}", width, height);
    return {};
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
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to allocate command buffers");
    }
    m_command_buffers = std::move(buffers);

    GOGGLES_LOG_DEBUG("Command pool and {} buffers created", MAX_FRAMES_IN_FLIGHT);
    return {};
}

auto VulkanBackend::create_sync_objects() -> Result<void> {
    m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    m_image_available_sems.resize(MAX_FRAMES_IN_FLIGHT);
    m_render_finished_sems.resize(MAX_FRAMES_IN_FLIGHT);

    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    vk::SemaphoreCreateInfo sem_info{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto [fence_result, fence] = m_device->createFence(fence_info);
        if (fence_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to create fence");
        }
        m_in_flight_fences[i] = fence;

        auto [sem1_result, sem1] = m_device->createSemaphore(sem_info);
        if (sem1_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to create semaphore");
        }
        m_image_available_sems[i] = sem1;

        auto [sem2_result, sem2] = m_device->createSemaphore(sem_info);
        if (sem2_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "Failed to create semaphore");
        }
        m_render_finished_sems[i] = sem2;
    }

    GOGGLES_LOG_DEBUG("Sync objects created");
    return {};
}

auto VulkanBackend::import_dmabuf(const FrameInfo& frame) -> Result<void> {
    if (m_imported_image && m_current_import.width == frame.width &&
        m_current_import.height == frame.height && m_current_import.format == frame.format) {
        return {};
    }

    auto wait_result = m_device->waitIdle();
    if (wait_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_DEVICE_LOST, "waitIdle failed before reimport");
    }
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
    image_info.usage = vk::ImageUsageFlagBits::eTransferSrc;
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
                                "Failed to get DMA-BUF fd properties: " + vk::to_string(fd_props_result));
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
                                "Failed to create DMA-BUF image view: " + vk::to_string(view_result));
    }
    m_imported_image_view = view;

    m_current_import = frame;
    m_current_import.dmabuf_fd = -1;

    GOGGLES_LOG_INFO("DMA-BUF imported: {}x{}, format={}", frame.width, frame.height,
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
}

auto VulkanBackend::acquire_next_image() -> Result<uint32_t> {
    auto wait_result = m_device->waitForFences(m_in_flight_fences[m_current_frame],
                                                VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::VULKAN_DEVICE_LOST, "Fence wait failed");
    }

    auto [result, image_index] = m_device->acquireNextImageKHR(
        *m_swapchain, UINT64_MAX, m_image_available_sems[m_current_frame], nullptr);

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

    static_cast<void>(m_device->resetFences(m_in_flight_fences[m_current_frame]));
    return image_index;
}

void VulkanBackend::record_blit_commands(vk::CommandBuffer cmd, uint32_t image_index) {
    static_cast<void>(cmd.reset());

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    static_cast<void>(cmd.begin(begin_info));

    vk::ImageMemoryBarrier src_barrier{};
    src_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    src_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    src_barrier.oldLayout = vk::ImageLayout::eUndefined;
    src_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = m_imported_image;
    src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    src_barrier.subresourceRange.baseMipLevel = 0;
    src_barrier.subresourceRange.levelCount = 1;
    src_barrier.subresourceRange.baseArrayLayer = 0;
    src_barrier.subresourceRange.layerCount = 1;

    vk::ImageMemoryBarrier dst_barrier{};
    dst_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    dst_barrier.oldLayout = vk::ImageLayout::eUndefined;
    dst_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
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
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, {}, {}, barriers);

    vk::ImageBlit blit_region{};
    blit_region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blit_region.srcSubresource.mipLevel = 0;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    blit_region.srcOffsets[1] = vk::Offset3D{
        static_cast<int32_t>(m_current_import.width),
        static_cast<int32_t>(m_current_import.height), 1};

    blit_region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blit_region.dstSubresource.mipLevel = 0;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    blit_region.dstOffsets[1] = vk::Offset3D{
        static_cast<int32_t>(m_swapchain_extent.width),
        static_cast<int32_t>(m_swapchain_extent.height), 1};

    cmd.blitImage(m_imported_image, vk::ImageLayout::eTransferSrcOptimal,
                  m_swapchain_images[image_index], vk::ImageLayout::eTransferDstOptimal,
                  blit_region, vk::Filter::eLinear);

    dst_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    dst_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    dst_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eBottomOfPipe,
                        {}, {}, {}, dst_barrier);

    static_cast<void>(cmd.end());
}

void VulkanBackend::record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index) {
    static_cast<void>(cmd.reset());

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    static_cast<void>(cmd.begin(begin_info));

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

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, {}, {}, barrier);

    vk::ClearColorValue clear_color{std::array{0.0F, 0.0F, 0.0F, 1.0F}};
    vk::ImageSubresourceRange range{};
    range.aspectMask = vk::ImageAspectFlagBits::eColor;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    cmd.clearColorImage(m_swapchain_images[image_index],
                        vk::ImageLayout::eTransferDstOptimal, clear_color, range);

    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eNone;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eBottomOfPipe,
                        {}, {}, {}, barrier);

    static_cast<void>(cmd.end());
}

auto VulkanBackend::submit_and_present(uint32_t image_index) -> Result<bool> {
    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTransfer;

    vk::SubmitInfo submit_info{};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &m_image_available_sems[m_current_frame];
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[m_current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_render_finished_sems[m_current_frame];

    auto submit_result = m_graphics_queue.submit(submit_info, m_in_flight_fences[m_current_frame]);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<bool>(ErrorCode::VULKAN_DEVICE_LOST,
                                 "Queue submit failed: " + vk::to_string(submit_result));
    }

    vk::PresentInfoKHR present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m_render_finished_sems[m_current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &*m_swapchain;
    present_info.pImageIndices = &image_index;

    auto present_result = m_graphics_queue.presentKHR(present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        m_needs_resize = true;
    }
    if (present_result != vk::Result::eSuccess &&
        present_result != vk::Result::eSuboptimalKHR &&
        present_result != vk::Result::eErrorOutOfDateKHR) {
        return make_error<bool>(ErrorCode::VULKAN_DEVICE_LOST,
                                 "Present failed: " + vk::to_string(present_result));
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return !m_needs_resize;
}

auto VulkanBackend::render_frame(const FrameInfo& frame) -> Result<bool> {
    if (!m_initialized) {
        return make_error<bool>(ErrorCode::VULKAN_INIT_FAILED, "Backend not initialized");
    }

    auto import_result = import_dmabuf(frame);
    if (!import_result) {
        return nonstd::make_unexpected(import_result.error());
    }

    auto acquire_result = acquire_next_image();
    if (!acquire_result) {
        return nonstd::make_unexpected(acquire_result.error());
    }
    uint32_t image_index = acquire_result.value();

    record_blit_commands(m_command_buffers[m_current_frame], image_index);
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

    record_clear_commands(m_command_buffers[m_current_frame], image_index);
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
