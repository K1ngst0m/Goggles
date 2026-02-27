#include "vulkan_backend.hpp"

#include "vulkan_error.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstring>
#include <format>
#include <render/chain/pass.hpp>
#include <stb_image_write.h>
#include <thread>
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

constexpr std::array OPTIONAL_DEVICE_EXTENSIONS = {
    VK_KHR_PRESENT_ID_EXTENSION_NAME,
    VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
};

// from drm_fourcc.h (kept local to avoid adding extra headers)
constexpr uint64_t DRM_FORMAT_MOD_INVALID = 0xffffffffffffffULL;

auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& mem_props, uint32_t type_bits)
    -> uint32_t {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_bits & (1U << i)) {
            return i;
        }
    }
    return UINT32_MAX;
}

struct DmabufImageCreateChain {
    vk::ExternalMemoryImageCreateInfo ext_mem_info;
    vk::ImageDrmFormatModifierExplicitCreateInfoEXT modifier_info;
    vk::SubresourceLayout plane_layout;
    vk::ImageCreateInfo image_info;
};

struct PhysicalDeviceCandidate {
    vk::PhysicalDevice device;
    uint32_t graphics_family;
    uint32_t index;
    bool present_wait_supported;
    int score;
};

struct ReadbackStagingBuffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    bool is_coherent = false;
};

auto to_ascii_lower(std::string value) -> std::string {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

auto get_device_name(vk::PhysicalDevice device) -> std::string {
    return device.getProperties().deviceName.data();
}

auto select_candidate_by_gpu_selector(const std::vector<PhysicalDeviceCandidate>& candidates,
                                      const std::string& gpu_selector,
                                      const std::string& available_gpus)
    -> Result<const PhysicalDeviceCandidate*> {
    uint32_t selector_index = 0;
    const auto parse_end = gpu_selector.data() + gpu_selector.size();
    auto [parsed_end, parsed_ec] = std::from_chars(gpu_selector.data(), parse_end, selector_index);
    const bool numeric_selector = parsed_ec == std::errc{} && parsed_end == parse_end;

    if (numeric_selector) {
        auto it = std::find_if(candidates.begin(), candidates.end(),
                               [selector_index](const PhysicalDeviceCandidate& c) {
                                   return c.index == selector_index;
                               });
        if (it == candidates.end()) {
            return make_error<const PhysicalDeviceCandidate*>(
                ErrorCode::vulkan_init_failed,
                "GPU selector '" + gpu_selector +
                    "' is invalid or unsuitable. Available GPUs: " + available_gpus);
        }
        return &*it;
    }

    const auto selector_lower = to_ascii_lower(gpu_selector);
    std::vector<const PhysicalDeviceCandidate*> matched_candidates;
    for (const auto& candidate : candidates) {
        const auto device_name = to_ascii_lower(get_device_name(candidate.device));
        if (device_name.find(selector_lower) != std::string::npos) {
            matched_candidates.push_back(&candidate);
        }
    }

    if (matched_candidates.empty()) {
        return make_error<const PhysicalDeviceCandidate*>(
            ErrorCode::vulkan_init_failed,
            "GPU selector '" + gpu_selector +
                "' is invalid or unsuitable. Available GPUs: " + available_gpus);
    }

    if (matched_candidates.size() > 1) {
        std::string matched_gpus;
        for (const auto* candidate : matched_candidates) {
            const auto gpu_name = get_device_name(candidate->device);
            matched_gpus += std::format("{}[{}] {}", matched_gpus.empty() ? "" : ", ",
                                        candidate->index, gpu_name);
        }
        return make_error<const PhysicalDeviceCandidate*>(
            ErrorCode::vulkan_init_failed, "GPU selector '" + gpu_selector +
                                               "' matches multiple suitable GPUs: " + matched_gpus +
                                               ". Please use a numeric index.");
    }

    return matched_candidates.front();
}

static void init_dmabuf_image_create_chain(const util::ExternalImage& frame, vk::Format vk_format,
                                           DmabufImageCreateChain* chain) {
    chain->ext_mem_info = vk::ExternalMemoryImageCreateInfo{};
    chain->modifier_info = vk::ImageDrmFormatModifierExplicitCreateInfoEXT{};
    chain->plane_layout = vk::SubresourceLayout{};
    chain->image_info = vk::ImageCreateInfo{};

    chain->ext_mem_info.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;

    chain->plane_layout.offset = frame.offset;
    chain->plane_layout.size = 0;
    chain->plane_layout.rowPitch = frame.stride;
    chain->plane_layout.arrayPitch = 0;
    chain->plane_layout.depthPitch = 0;

    chain->modifier_info.drmFormatModifier = frame.modifier;
    chain->modifier_info.drmFormatModifierPlaneCount = 1;
    chain->modifier_info.pPlaneLayouts = &chain->plane_layout;

    chain->ext_mem_info.pNext = &chain->modifier_info;

    chain->image_info.pNext = &chain->ext_mem_info;
    chain->image_info.imageType = vk::ImageType::e2D;
    chain->image_info.format = vk_format;
    chain->image_info.extent = vk::Extent3D{frame.width, frame.height, 1};
    chain->image_info.mipLevels = 1;
    chain->image_info.arrayLayers = 1;
    chain->image_info.samples = vk::SampleCountFlagBits::e1;
    chain->image_info.tiling = vk::ImageTiling::eDrmFormatModifierEXT;
    chain->image_info.usage =
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    chain->image_info.sharingMode = vk::SharingMode::eExclusive;
    chain->image_info.initialLayout = vk::ImageLayout::eUndefined;
}

static auto get_dmabuf_memory_type_bits(vk::Device device, int fd) -> Result<uint32_t> {
    VkMemoryFdPropertiesKHR fd_props_raw{};
    fd_props_raw.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;

    auto fd_props_result =
        static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetMemoryFdPropertiesKHR(
            device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &fd_props_raw));
    if (fd_props_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_init_failed,
                                    "Stale DMA-BUF fd, skipping frame");
    }

    return fd_props_raw.memoryTypeBits;
}

static auto allocate_imported_dmabuf_memory(vk::Device device, vk::Image image, vk::DeviceSize size,
                                            uint32_t mem_type_index, util::UniqueFd import_fd,
                                            const vk::MemoryDedicatedRequirements& dedicated_reqs)
    -> Result<vk::DeviceMemory> {
    vk::ImportMemoryFdInfoKHR import_info{};
    import_info.handleType = vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT;
    import_info.fd = import_fd.get();

    vk::MemoryDedicatedAllocateInfo dedicated_alloc{};
    dedicated_alloc.image = image;
    if (dedicated_reqs.requiresDedicatedAllocation || dedicated_reqs.prefersDedicatedAllocation) {
        import_info.pNext = &dedicated_alloc;
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.pNext = &import_info;
    alloc_info.allocationSize = size;
    alloc_info.memoryTypeIndex = mem_type_index;

    auto [alloc_result, memory] = device.allocateMemory(alloc_info);
    if (alloc_result != vk::Result::eSuccess) {
        return make_error<vk::DeviceMemory>(ErrorCode::vulkan_init_failed,
                                            "Failed to import DMA-BUF memory: " +
                                                vk::to_string(alloc_result));
    }

    // Vulkan takes ownership of fd on success.
    import_fd.release();
    return memory;
}

static auto create_imported_image_view(vk::Device device, vk::Image image, vk::Format format)
    -> Result<vk::ImageView> {
    vk::ImageViewCreateInfo view_info{};
    view_info.image = image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = device.createImageView(view_info);
    if (view_result != vk::Result::eSuccess) {
        return make_error<vk::ImageView>(ErrorCode::vulkan_init_failed,
                                         "Failed to create DMA-BUF image view: " +
                                             vk::to_string(view_result));
    }
    return view;
}

static auto create_readback_staging_buffer(vk::Device device, vk::PhysicalDevice physical_device,
                                           vk::DeviceSize size) -> Result<ReadbackStagingBuffer> {
    vk::BufferCreateInfo buf_info{};
    buf_info.size = size;
    buf_info.usage = vk::BufferUsageFlagBits::eTransferDst;
    buf_info.sharingMode = vk::SharingMode::eExclusive;

    auto [buf_result, buffer] = device.createBuffer(buf_info);
    if (buf_result != vk::Result::eSuccess) {
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to create staging buffer: " +
                                                     vk::to_string(buf_result));
    }

    auto buf_mem_reqs = device.getBufferMemoryRequirements(buffer);
    auto mem_props = physical_device.getMemoryProperties();
    uint32_t staging_mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((buf_mem_reqs.memoryTypeBits & (1U << i)) &&
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
            if ((buf_mem_reqs.memoryTypeBits & (1U << i)) &&
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
    staging_alloc.allocationSize = buf_mem_reqs.size;
    staging_alloc.memoryTypeIndex = staging_mem_type;
    auto [alloc_result, memory] = device.allocateMemory(staging_alloc);
    if (alloc_result != vk::Result::eSuccess) {
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to allocate staging memory: " +
                                                     vk::to_string(alloc_result));
    }

    auto bind_res = device.bindBufferMemory(buffer, memory, 0);
    if (bind_res != vk::Result::eSuccess) {
        device.freeMemory(memory);
        device.destroyBuffer(buffer);
        return make_error<ReadbackStagingBuffer>(ErrorCode::vulkan_init_failed,
                                                 "Failed to bind staging buffer memory: " +
                                                     vk::to_string(bind_res));
    }

    ReadbackStagingBuffer staging{};
    staging.buffer = buffer;
    staging.memory = memory;
    staging.is_coherent = (mem_props.memoryTypes[staging_mem_type].propertyFlags &
                           vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags{};
    return staging;
}

static void destroy_readback_staging_buffer(vk::Device device, ReadbackStagingBuffer& staging) {
    if (staging.memory) {
        device.freeMemory(staging.memory);
        staging.memory = nullptr;
    }
    if (staging.buffer) {
        device.destroyBuffer(staging.buffer);
        staging.buffer = nullptr;
    }
}

static auto submit_readback_copy(vk::Device device, vk::Queue queue, vk::CommandBuffer cmd,
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
    auto submit_res = queue.submit(submit_info, fence);
    if (submit_res != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_res));
    }

    auto fence_wait = device.waitForFences(fence, VK_TRUE, UINT64_MAX);
    if (fence_wait != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost, "Fence wait failed during readback");
    }
    return {};
}

} // namespace

VulkanBackend::~VulkanBackend() {
    shutdown();
}

auto VulkanBackend::create(SDL_Window* window, bool enable_validation,
                           const std::filesystem::path& shader_dir,
                           const std::filesystem::path& cache_dir, const RenderSettings& settings)
    -> ResultPtr<VulkanBackend> {
    GOGGLES_PROFILE_FUNCTION();

    auto backend = std::unique_ptr<VulkanBackend>(new VulkanBackend());

    auto vk_get_instance_proc_addr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (vk_get_instance_proc_addr == nullptr) {
        return make_result_ptr_error<VulkanBackend>(ErrorCode::vulkan_init_failed,
                                                    "Failed to get vkGetInstanceProcAddr from SDL");
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

    backend->m_enable_validation = enable_validation;
    backend->m_shader_dir = shader_dir;
    backend->m_cache_dir = cache_dir;
    if (backend->m_cache_dir.empty()) {
        try {
            backend->m_cache_dir = std::filesystem::temp_directory_path() / "goggles" / "shaders";
        } catch (...) {
            backend->m_cache_dir = "/tmp/goggles/shaders";
        }
    }
    backend->m_scale_mode = settings.scale_mode;
    backend->m_integer_scale = settings.integer_scale;
    backend->m_gpu_selector = settings.gpu_selector;
    backend->m_source_resolution = vk::Extent2D{settings.source_width, settings.source_height};
    backend->update_target_fps(settings.target_fps);

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
        return make_result_ptr_error<VulkanBackend>(ErrorCode::unknown_error,
                                                    "SDL_GetWindowSizeInPixels failed: " +
                                                        std::string(SDL_GetError()));
    }

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

auto VulkanBackend::create_headless(bool enable_validation, const std::filesystem::path& shader_dir,
                                    const std::filesystem::path& cache_dir,
                                    const RenderSettings& settings) -> ResultPtr<VulkanBackend> {
    GOGGLES_PROFILE_FUNCTION();

    auto backend = std::unique_ptr<VulkanBackend>(new VulkanBackend());
    backend->m_headless = true;
    backend->m_enable_validation = enable_validation;
    backend->m_shader_dir = shader_dir;
    backend->m_cache_dir = cache_dir;
    if (backend->m_cache_dir.empty()) {
        try {
            backend->m_cache_dir = std::filesystem::temp_directory_path() / "goggles" / "shaders";
        } catch (...) {
            backend->m_cache_dir = "/tmp/goggles/shaders";
        }
    }
    backend->m_scale_mode = settings.scale_mode;
    backend->m_integer_scale = settings.integer_scale;
    backend->m_gpu_selector = settings.gpu_selector;
    backend->m_source_resolution = vk::Extent2D{settings.source_width, settings.source_height};
    backend->update_target_fps(settings.target_fps);

    // Load Vulkan loader directly without SDL.
    PFN_vkGetInstanceProcAddr loader = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    if (!loader) {
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    }

    GOGGLES_TRY(backend->create_instance_headless(enable_validation));
    GOGGLES_TRY(backend->create_debug_messenger());
    GOGGLES_TRY(backend->select_physical_device_headless());
    GOGGLES_TRY(backend->create_device());
    GOGGLES_TRY(backend->create_command_resources());
    GOGGLES_TRY(backend->create_sync_objects_headless());
    GOGGLES_TRY(backend->create_offscreen_image());
    GOGGLES_TRY(backend->init_filter_chain());

    GOGGLES_LOG_INFO("Vulkan headless backend initialized: {}x{}",
                     backend->m_offscreen_extent.width, backend->m_offscreen_extent.height);
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
        auto wait_result = m_device.waitIdle();
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
    if (m_headless && m_device) {
        if (m_offscreen_view) {
            m_device.destroyImageView(m_offscreen_view);
            m_offscreen_view = nullptr;
        }
        if (m_offscreen_image) {
            m_device.destroyImage(m_offscreen_image);
            m_offscreen_image = nullptr;
        }
        if (m_offscreen_memory) {
            m_device.freeMemory(m_offscreen_memory);
            m_offscreen_memory = nullptr;
        }
    }

    if (m_device) {
        for (auto& frame : m_frames) {
            m_device.destroyFence(frame.in_flight_fence);
            if (frame.image_available_sem) {
                m_device.destroySemaphore(frame.image_available_sem);
            }
            if (frame.pending_acquire_sync_sem) {
                m_device.destroySemaphore(frame.pending_acquire_sync_sem);
            }
        }
        for (auto sem : m_render_finished_sems) {
            m_device.destroySemaphore(sem);
        }
        for (auto view : m_swapchain_image_views) {
            m_device.destroyImageView(view);
        }
        if (m_swapchain) {
            m_device.destroySwapchainKHR(m_swapchain);
        }
        if (m_command_pool) {
            m_device.destroyCommandPool(m_command_pool);
        }
    }
    m_frames = {};
    m_render_finished_sems.clear();
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    if (m_device) {
        m_device.destroy();
        m_device = nullptr;
    }
    if (m_instance && m_surface) {
        m_instance.destroySurfaceKHR(m_surface);
        m_surface = nullptr;
    }
    m_debug_messenger.reset();
    if (m_instance) {
        m_instance.destroy();
        m_instance = nullptr;
    }

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

    auto [result, instance] = vk::createInstance(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create Vulkan instance: " + vk::to_string(result));
    }

    m_instance = instance;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

    GOGGLES_LOG_DEBUG("Vulkan instance created with {} extensions, {} layers", extensions.size(),
                      layers.size());
    return {};
}

auto VulkanBackend::create_instance_headless(bool enable_validation) -> Result<void> {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    std::vector<const char*> extensions;
    extensions.reserve(REQUIRED_INSTANCE_EXTENSIONS.size() + 1);
    for (const auto* ext : REQUIRED_INSTANCE_EXTENSIONS) {
        extensions.push_back(ext);
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

    auto [result, instance] = vk::createInstance(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create Vulkan instance: " + vk::to_string(result));
    }

    m_instance = instance;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);
    return {};
}

auto VulkanBackend::create_debug_messenger() -> Result<void> {
    if (!m_enable_validation || !is_validation_layer_available()) {
        return {};
    }

    auto messenger_result = VulkanDebugMessenger::create(m_instance);
    if (!messenger_result) {
        GOGGLES_LOG_WARN("Failed to create debug messenger: {}", messenger_result.error().message);
        return {};
    }

    m_debug_messenger = std::move(messenger_result.value());
    return {};
}

auto VulkanBackend::create_surface(SDL_Window* window) -> Result<void> {
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &raw_surface)) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }

    m_surface = raw_surface;
    GOGGLES_LOG_DEBUG("Vulkan surface created");
    return {};
}

auto VulkanBackend::select_physical_device() -> Result<void> {
    auto [result, devices] = m_instance.enumeratePhysicalDevices();
    if (result != vk::Result::eSuccess || devices.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "No Vulkan devices found");
    }

    std::vector<PhysicalDeviceCandidate> candidates;
    std::string available_gpus;

    GOGGLES_LOG_INFO("Available GPUs:");
    for (size_t idx = 0; idx < devices.size(); ++idx) {
        const auto& device = devices[idx];
        auto props = device.getProperties();
        auto queue_families = device.getQueueFamilyProperties();
        uint32_t graphics_family = UINT32_MAX;

        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            const auto& family = queue_families[i];
            if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
                auto [res, supported] = device.getSurfaceSupportKHR(i, m_surface);
                if (res == vk::Result::eSuccess && supported) {
                    graphics_family = i;
                    break;
                }
            }
        }

        bool surface_ok = graphics_family != UINT32_MAX;
        bool extensions_ok = false;
        bool present_wait_supported = false;

        if (surface_ok) {
            auto [ext_result, available_extensions] = device.enumerateDeviceExtensionProperties();
            if (ext_result == vk::Result::eSuccess) {
                extensions_ok = true;
                for (const auto* required : REQUIRED_DEVICE_EXTENSIONS) {
                    bool found = false;
                    for (const auto& ext : available_extensions) {
                        if (std::strcmp(ext.extensionName, required) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        extensions_ok = false;
                        break;
                    }
                }

                if (extensions_ok) {
                    bool present_id_ok = false;
                    bool present_wait_ok = false;
                    for (const auto& ext : available_extensions) {
                        if (std::strcmp(ext.extensionName, VK_KHR_PRESENT_ID_EXTENSION_NAME) == 0) {
                            present_id_ok = true;
                        } else if (std::strcmp(ext.extensionName,
                                               VK_KHR_PRESENT_WAIT_EXTENSION_NAME) == 0) {
                            present_wait_ok = true;
                        }
                    }
                    present_wait_supported = present_id_ok && present_wait_ok;
                }
            }
        }

        const char* status = (surface_ok && extensions_ok)
                                 ? "suitable"
                                 : (!surface_ok ? "no surface support" : "missing extensions");
        GOGGLES_LOG_INFO("  [{}] {} ({})", idx, props.deviceName.data(), status);
        available_gpus += std::format("{}[{}] {}", available_gpus.empty() ? "" : ", ", idx,
                                      props.deviceName.data());

        if (surface_ok && extensions_ok) {
            int score = 0;
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                score += 1000;
            } else if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
                score += 500;
            }
            candidates.push_back({.device = device,
                                  .graphics_family = graphics_family,
                                  .index = static_cast<uint32_t>(idx),
                                  .present_wait_supported = present_wait_supported,
                                  .score = score});
        }
    }

    if (candidates.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable GPU found with DMA-BUF and surface support");
    }

    const PhysicalDeviceCandidate* selected = nullptr;
    if (!m_gpu_selector.empty()) {
        auto selected_result =
            select_candidate_by_gpu_selector(candidates, m_gpu_selector, available_gpus);
        if (!selected_result) {
            return make_error<void>(selected_result.error().code, selected_result.error().message,
                                    selected_result.error().location);
        }
        selected = selected_result.value();
    } else {
        auto best =
            std::max_element(candidates.begin(), candidates.end(),
                             [](const PhysicalDeviceCandidate& a,
                                const PhysicalDeviceCandidate& b) { return a.score < b.score; });
        selected = &*best;
    }

    m_physical_device = selected->device;
    m_graphics_queue_family = selected->graphics_family;
    m_gpu_index = selected->index;
    m_present_wait_supported = selected->present_wait_supported;

    vk::PhysicalDeviceIDProperties id_props{};
    vk::PhysicalDeviceProperties2 props2{};
    props2.pNext = &id_props;
    m_physical_device.getProperties2(&props2);

    std::array<char, 37> uuid_hex{};
    std::snprintf(uuid_hex.data(), uuid_hex.size(),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  id_props.deviceUUID[0], id_props.deviceUUID[1], id_props.deviceUUID[2],
                  id_props.deviceUUID[3], id_props.deviceUUID[4], id_props.deviceUUID[5],
                  id_props.deviceUUID[6], id_props.deviceUUID[7], id_props.deviceUUID[8],
                  id_props.deviceUUID[9], id_props.deviceUUID[10], id_props.deviceUUID[11],
                  id_props.deviceUUID[12], id_props.deviceUUID[13], id_props.deviceUUID[14],
                  id_props.deviceUUID[15]);
    m_gpu_uuid = uuid_hex.data();

    GOGGLES_LOG_INFO("Selected GPU: {} (UUID: {})", props2.properties.deviceName.data(),
                     m_gpu_uuid);
    return {};
}

auto VulkanBackend::select_physical_device_headless() -> Result<void> {
    auto [result, devices] = m_instance.enumeratePhysicalDevices();
    if (result != vk::Result::eSuccess || devices.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "No Vulkan devices found");
    }

    std::vector<const char*> headless_required_extensions;
    headless_required_extensions.reserve(REQUIRED_DEVICE_EXTENSIONS.size());
    for (const auto* ext : REQUIRED_DEVICE_EXTENSIONS) {
        if (std::strcmp(ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME) != 0) {
            headless_required_extensions.push_back(ext);
        }
    }

    std::vector<PhysicalDeviceCandidate> candidates;
    std::string available_gpus;

    GOGGLES_LOG_INFO("Available GPUs (headless):");
    for (size_t idx = 0; idx < devices.size(); ++idx) {
        const auto& device = devices[idx];
        auto props = device.getProperties();
        auto queue_families = device.getQueueFamilyProperties();
        uint32_t graphics_family = UINT32_MAX;

        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphics_family = i;
                break;
            }
        }

        bool graphics_ok = graphics_family != UINT32_MAX;
        bool extensions_ok = false;

        if (graphics_ok) {
            auto [ext_result, available_extensions] = device.enumerateDeviceExtensionProperties();
            if (ext_result == vk::Result::eSuccess) {
                extensions_ok = true;
                for (const auto* required : headless_required_extensions) {
                    bool found = false;
                    for (const auto& ext : available_extensions) {
                        if (std::strcmp(ext.extensionName, required) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        extensions_ok = false;
                        break;
                    }
                }
            }
        }

        const char* status = (graphics_ok && extensions_ok)
                                 ? "suitable"
                                 : (!graphics_ok ? "no graphics queue" : "missing extensions");
        GOGGLES_LOG_INFO("  [{}] {} ({})", idx, props.deviceName.data(), status);
        available_gpus += std::format("{}[{}] {}", available_gpus.empty() ? "" : ", ", idx,
                                      props.deviceName.data());

        if (graphics_ok && extensions_ok) {
            int score = 0;
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                score += 1000;
            } else if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
                score += 500;
            }
            candidates.push_back({.device = device,
                                  .graphics_family = graphics_family,
                                  .index = static_cast<uint32_t>(idx),
                                  .present_wait_supported = false,
                                  .score = score});
        }
    }

    if (candidates.empty()) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable GPU found with DMA-BUF support (headless)");
    }

    const PhysicalDeviceCandidate* selected = nullptr;
    if (!m_gpu_selector.empty()) {
        auto selected_result =
            select_candidate_by_gpu_selector(candidates, m_gpu_selector, available_gpus);
        if (!selected_result) {
            return make_error<void>(selected_result.error().code, selected_result.error().message,
                                    selected_result.error().location);
        }
        selected = selected_result.value();
    } else {
        auto best =
            std::max_element(candidates.begin(), candidates.end(),
                             [](const PhysicalDeviceCandidate& a,
                                const PhysicalDeviceCandidate& b) { return a.score < b.score; });
        selected = &*best;
    }

    m_physical_device = selected->device;
    m_graphics_queue_family = selected->graphics_family;
    m_gpu_index = selected->index;
    m_present_wait_supported = false;

    vk::PhysicalDeviceIDProperties id_props{};
    vk::PhysicalDeviceProperties2 props2{};
    props2.pNext = &id_props;
    m_physical_device.getProperties2(&props2);

    std::array<char, 37> uuid_hex{};
    std::snprintf(uuid_hex.data(), uuid_hex.size(),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  id_props.deviceUUID[0], id_props.deviceUUID[1], id_props.deviceUUID[2],
                  id_props.deviceUUID[3], id_props.deviceUUID[4], id_props.deviceUUID[5],
                  id_props.deviceUUID[6], id_props.deviceUUID[7], id_props.deviceUUID[8],
                  id_props.deviceUUID[9], id_props.deviceUUID[10], id_props.deviceUUID[11],
                  id_props.deviceUUID[12], id_props.deviceUUID[13], id_props.deviceUUID[14],
                  id_props.deviceUUID[15]);
    m_gpu_uuid = uuid_hex.data();

    GOGGLES_LOG_INFO("Selected GPU (headless): {} (UUID: {})", props2.properties.deviceName.data(),
                     m_gpu_uuid);
    return {};
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
    vk::PhysicalDevicePresentIdFeaturesKHR present_id_features{};
    vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_features{};
    vk11_features.pNext = &vk12_features;
    vk12_features.pNext = &vk13_features;

    if (m_present_wait_supported) {
        vk13_features.pNext = &present_id_features;
        present_id_features.pNext = &present_wait_features;
    }

    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &vk11_features;
    m_physical_device.getFeatures2(&features2);

    const bool present_wait_features_ok = (present_id_features.presentId != VK_FALSE) &&
                                          (present_wait_features.presentWait != VK_FALSE);
    const bool present_wait_ready = m_present_wait_supported && present_wait_features_ok;
    if (m_present_wait_supported && !present_wait_ready) {
        GOGGLES_LOG_WARN(
            "VK_KHR_present_id/VK_KHR_present_wait extensions present but features disabled; "
            "falling back to mailbox/throttle");
    }

    m_present_wait_supported = present_wait_ready;

    if (!vk11_features.shaderDrawParameters) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "shaderDrawParameters not supported (required for vertex shaders)");
    }
    if (!vk13_features.dynamicRendering) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Dynamic rendering not supported (required for Vulkan 1.3)");
    }

    vk::PhysicalDeviceVulkan11Features vk11_enable{};
    vk11_enable.shaderDrawParameters = VK_TRUE;
    vk::PhysicalDeviceVulkan12Features vk12_enable{};
    vk::PhysicalDeviceVulkan13Features vk13_enable{};
    vk13_enable.dynamicRendering = VK_TRUE;
    vk::PhysicalDevicePresentIdFeaturesKHR present_id_enable{};
    vk::PhysicalDevicePresentWaitFeaturesKHR present_wait_enable{};
    vk11_enable.pNext = &vk12_enable;
    vk12_enable.pNext = &vk13_enable;
    if (m_present_wait_supported) {
        present_id_enable.presentId = VK_TRUE;
        present_wait_enable.presentWait = VK_TRUE;
        vk13_enable.pNext = &present_id_enable;
        present_id_enable.pNext = &present_wait_enable;
    }

    std::array<const char*, REQUIRED_DEVICE_EXTENSIONS.size() + OPTIONAL_DEVICE_EXTENSIONS.size()>
        extensions{};
    size_t extension_count = 0;
    for (const auto* ext : REQUIRED_DEVICE_EXTENSIONS) {
        if (m_headless && std::strcmp(ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            continue;
        }
        extensions[extension_count++] = ext;
    }
    if (!m_headless && m_present_wait_supported) {
        for (const auto* ext : OPTIONAL_DEVICE_EXTENSIONS) {
            extensions[extension_count++] = ext;
        }
    }

    vk::DeviceCreateInfo create_info{};
    create_info.pNext = &vk11_enable;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extension_count);
    create_info.ppEnabledExtensionNames = extensions.data();

    auto [result, device] = m_physical_device.createDevice(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create logical device: " + vk::to_string(result));
    }

    m_device = device;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
    m_graphics_queue = m_device.getQueue(m_graphics_queue_family, 0);

    GOGGLES_LOG_DEBUG("Vulkan device created");
    return {};
}

auto VulkanBackend::create_swapchain(uint32_t width, uint32_t height, vk::Format preferred_format)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    auto [cap_result, capabilities] = m_physical_device.getSurfaceCapabilitiesKHR(m_surface);
    if (cap_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to query surface capabilities");
    }

    auto [fmt_result, formats] = m_physical_device.getSurfaceFormatsKHR(m_surface);
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
    create_info.surface = m_surface;
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

    auto [pm_result, present_modes] = m_physical_device.getSurfacePresentModesKHR(m_surface);
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

    if (m_present_wait_supported) {
        chosen_mode = vk::PresentModeKHR::eFifo;
    } else if (mailbox_supported) {
        chosen_mode = vk::PresentModeKHR::eMailbox;
    }

    create_info.presentMode = chosen_mode;
    create_info.clipped = VK_TRUE;

    auto [result, swapchain] = m_device.createSwapchainKHR(create_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create swapchain: " + vk::to_string(result));
    }

    m_swapchain = swapchain;
    m_swapchain_format = chosen_format.format;
    m_swapchain_extent = extent;

    auto [img_result, images] = m_device.getSwapchainImagesKHR(m_swapchain);
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

        auto [view_result, view] = m_device.createImageView(view_info);
        if (view_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create image view");
        }
        m_swapchain_image_views.push_back(view);
    }

    m_present_id = 0;
    m_last_present_time = std::chrono::steady_clock::time_point{};

    GOGGLES_LOG_DEBUG("Swapchain created: {}x{}, {} images", extent.width, extent.height,
                      m_swapchain_images.size());
    return {};
}

void VulkanBackend::cleanup_swapchain() {
    if (m_device) {
        for (auto view : m_swapchain_image_views) {
            m_device.destroyImageView(view);
        }
        if (m_swapchain) {
            m_device.destroySwapchainKHR(m_swapchain);
        }
    }
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();
    m_swapchain = nullptr;
}

auto VulkanBackend::recreate_swapchain(uint32_t width, uint32_t height, vk::Format source_format)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (width == 0 || height == 0) {
        return make_error<void>(ErrorCode::unknown_error, "Swapchain size is zero");
    }

    vk::Format target_format = m_swapchain_format;
    bool recreate_filter_chain = false;
    if (source_format != vk::Format::eUndefined) {
        target_format = get_matching_swapchain_format(source_format);
        recreate_filter_chain = target_format != m_swapchain_format;
        if (recreate_filter_chain) {
            GOGGLES_LOG_INFO("Source format changed to {}, recreating swapchain with {}",
                             vk::to_string(source_format), vk::to_string(target_format));
        }
    }

    VK_TRY(m_device.waitIdle(), ErrorCode::vulkan_device_lost,
           "waitIdle failed before swapchain recreation");

    if (recreate_filter_chain) {
        m_filter_chain->shutdown();
    }
    cleanup_swapchain();

    GOGGLES_TRY(create_swapchain(width, height, target_format));

    if (recreate_filter_chain) {
        GOGGLES_TRY(init_filter_chain());

        if (!m_preset_path.empty()) {
            auto result = m_filter_chain->load_preset(m_preset_path);
            if (!result) {
                GOGGLES_LOG_WARN("Failed to reload shader preset after format change: {}",
                                 result.error().message);
            }
        }

        m_source_format = source_format;
        m_format_changed.store(true, std::memory_order_release);
    } else {
        GOGGLES_TRY(m_filter_chain->handle_resize(m_swapchain_extent));
    }

    m_needs_resize = false;
    GOGGLES_LOG_DEBUG("Swapchain recreated: {}x{}", width, height);
    return {};
}

void VulkanBackend::wait_all_frames() {
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> fences{};
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        fences[i] = m_frames[i].in_flight_fence;
    }
    auto result = m_device.waitForFences(fences, VK_TRUE, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        GOGGLES_LOG_WARN("wait_all_frames failed: {}", vk::to_string(result));
    }
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

    auto [pool_result, pool] = m_device.createCommandPool(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create command pool");
    }
    m_command_pool = pool;

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    auto [alloc_result, buffers] = m_device.allocateCommandBuffers(alloc_info);
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
            auto [result, fence] = m_device.createFence(fence_info);
            if (result != vk::Result::eSuccess) {
                return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create fence");
            }
            frame.in_flight_fence = fence;
        }
        {
            auto [result, sem] = m_device.createSemaphore(sem_info);
            if (result != vk::Result::eSuccess) {
                return make_error<void>(ErrorCode::vulkan_init_failed,
                                        "Failed to create semaphore");
            }
            frame.image_available_sem = sem;
        }
    }

    m_render_finished_sems.resize(m_swapchain_images.size());
    for (auto& sem : m_render_finished_sems) {
        auto [result, new_sem] = m_device.createSemaphore(sem_info);
        if (result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_init_failed,
                                    "Failed to create render finished semaphore");
        }
        sem = new_sem;
    }

    GOGGLES_LOG_DEBUG("Sync objects created");
    return {};
}

auto VulkanBackend::create_sync_objects_headless() -> Result<void> {
    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    for (auto& frame : m_frames) {
        auto [result, fence] = m_device.createFence(fence_info);
        if (result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to create fence");
        }
        frame.in_flight_fence = fence;
    }

    GOGGLES_LOG_DEBUG("Headless sync objects created");
    return {};
}

auto VulkanBackend::create_offscreen_image() -> Result<void> {
    uint32_t width = m_source_resolution.width;
    uint32_t height = m_source_resolution.height;
    if (width == 0 || height == 0) {
        width = 1920;
        height = 1080;
    }
    m_offscreen_extent = vk::Extent2D{width, height};
    m_swapchain_format = vk::Format::eR8G8B8A8Unorm;
    m_swapchain_extent = m_offscreen_extent;

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

    auto [img_result, image] = m_device.createImage(image_info);
    if (img_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create offscreen image: " + vk::to_string(img_result));
    }
    m_offscreen_image = image;

    auto mem_reqs = m_device.getImageMemoryRequirements(m_offscreen_image);
    auto mem_props = m_physical_device.getMemoryProperties();
    uint32_t mem_type = find_memory_type(mem_props, mem_reqs.memoryTypeBits);
    if (mem_type == UINT32_MAX) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable memory type for offscreen image");
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    auto [mem_result, memory] = m_device.allocateMemory(alloc_info);
    if (mem_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate offscreen memory: " +
                                    vk::to_string(mem_result));
    }
    m_offscreen_memory = memory;

    auto bind_result = m_device.bindImageMemory(m_offscreen_image, m_offscreen_memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to bind offscreen image memory: " +
                                    vk::to_string(bind_result));
    }

    vk::ImageViewCreateInfo view_info{};
    view_info.image = m_offscreen_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk::Format::eR8G8B8A8Unorm;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = m_device.createImageView(view_info);
    if (view_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create offscreen image view: " +
                                    vk::to_string(view_result));
    }
    m_offscreen_view = view;

    GOGGLES_LOG_DEBUG("Offscreen image created: {}x{} R8G8B8A8Unorm", width, height);
    return {};
}

auto VulkanBackend::init_filter_chain() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    m_shader_runtime = GOGGLES_TRY(ShaderRuntime::create(m_cache_dir));

    VulkanContext vk_ctx{.device = m_device,
                         .physical_device = m_physical_device,
                         .command_pool = m_command_pool,
                         .graphics_queue = m_graphics_queue};
    m_filter_chain =
        GOGGLES_TRY(FilterChain::create(vk_ctx, m_swapchain_format, MAX_FRAMES_IN_FLIGHT,
                                        *m_shader_runtime, m_shader_dir, m_source_resolution));
    apply_filter_chain_policy();
    return {};
}

void VulkanBackend::load_shader_preset(const std::filesystem::path& preset_path) {
    GOGGLES_PROFILE_FUNCTION();

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

auto VulkanBackend::import_external_image(const util::ExternalImage& frame) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!frame.handle.valid()) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Invalid DMA-BUF fd");
    }

    if (frame.modifier == DRM_FORMAT_MOD_INVALID) {
        return make_error<void>(ErrorCode::invalid_data,
                                std::format("Invalid DMA-BUF modifier 0x{:x} "
                                            "(DRM_FORMAT_MOD_INVALID); capture did not provide a "
                                            "valid modifier, cannot import",
                                            frame.modifier));
    }

    VK_TRY(m_device.waitIdle(), ErrorCode::vulkan_device_lost, "waitIdle failed before reimport");
    cleanup_imported_image();

    auto vk_format = frame.format;
    DmabufImageCreateChain chain{};
    init_dmabuf_image_create_chain(frame, vk_format, &chain);

    auto [img_result, image] = m_device.createImage(chain.image_info);
    if (img_result != vk::Result::eSuccess) {
        return make_error<void>(
            ErrorCode::vulkan_init_failed,
            std::format("Failed to create DMA-BUF image (format={}, modifier=0x{:x}): {}",
                        vk::to_string(vk_format), frame.modifier, vk::to_string(img_result)));
    }
    m_import.image = image;

    vk::ImageMemoryRequirementsInfo2 mem_reqs_info{};
    mem_reqs_info.image = m_import.image;

    vk::MemoryDedicatedRequirements dedicated_reqs{};
    vk::MemoryRequirements2 mem_reqs2{};
    mem_reqs2.pNext = &dedicated_reqs;

    m_device.getImageMemoryRequirements2(&mem_reqs_info, &mem_reqs2);
    auto mem_reqs = mem_reqs2.memoryRequirements;

    auto fd_type_bits_result = get_dmabuf_memory_type_bits(m_device, frame.handle.get());
    if (!fd_type_bits_result) {
        cleanup_imported_image();
        return make_error<void>(fd_type_bits_result.error().code,
                                fd_type_bits_result.error().message);
    }

    auto mem_props = m_physical_device.getMemoryProperties();
    uint32_t combined_bits = mem_reqs.memoryTypeBits & fd_type_bits_result.value();
    uint32_t mem_type_index = find_memory_type(mem_props, combined_bits);

    if (mem_type_index == UINT32_MAX) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable memory type for DMA-BUF import");
    }

    // Vulkan takes ownership of fd on success
    auto import_fd = frame.handle.dup();
    if (!import_fd) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed, "Failed to dup DMA-BUF fd");
    }

    auto mem_result =
        allocate_imported_dmabuf_memory(m_device, m_import.image, mem_reqs.size, mem_type_index,
                                        std::move(import_fd), dedicated_reqs);
    if (!mem_result) {
        cleanup_imported_image();
        return make_error<void>(mem_result.error().code, mem_result.error().message);
    }
    auto memory = mem_result.value();
    m_import.memory = memory;

    auto bind_result = m_device.bindImageMemory(m_import.image, m_import.memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        cleanup_imported_image();
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to bind DMA-BUF memory: " + vk::to_string(bind_result));
    }

    auto view_result = create_imported_image_view(m_device, m_import.image, vk_format);
    if (!view_result) {
        cleanup_imported_image();
        return make_error<void>(view_result.error().code, view_result.error().message);
    }
    m_import.view = view_result.value();
    m_import_extent = vk::Extent2D{frame.width, frame.height};

    GOGGLES_LOG_TRACE("DMA-BUF imported: {}x{}, format={}, modifier=0x{:x}", frame.width,
                      frame.height, vk::to_string(vk_format), frame.modifier);
    return {};
}

void VulkanBackend::cleanup_imported_image() {
    if (m_device) {
        if (m_import.view) {
            m_device.destroyImageView(m_import.view);
            m_import.view = nullptr;
        }
        if (m_import.memory) {
            m_device.freeMemory(m_import.memory);
            m_import.memory = nullptr;
        }
        if (m_import.image) {
            m_device.destroyImage(m_import.image);
            m_import.image = nullptr;
        }
    }
}

auto VulkanBackend::get_prechain_resolution() const -> vk::Extent2D {
    return m_source_resolution;
}

auto VulkanBackend::acquire_next_image() -> Result<uint32_t> {
    GOGGLES_PROFILE_SCOPE("AcquireImage");

    auto& frame = m_frames[m_current_frame];

    auto wait_result = m_device.waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<uint32_t>(ErrorCode::vulkan_device_lost, "Fence wait failed");
    }

    // GPU finished with this frame slot — safe to destroy deferred sync semaphore
    if (frame.pending_acquire_sync_sem) {
        m_device.destroySemaphore(frame.pending_acquire_sync_sem);
        frame.pending_acquire_sync_sem = nullptr;
    }

    uint32_t image_index = 0;
    auto result = static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX, frame.image_available_sem, nullptr, &image_index));

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

    auto reset_result = m_device.resetFences(frame.in_flight_fence);
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
                           m_swapchain_image_views[image_index], m_swapchain_extent,
                           m_current_frame, m_scale_mode, m_integer_scale);

    if (ui_callback) {
        ui_callback(cmd, m_swapchain_image_views[image_index], m_swapchain_extent);
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
    color_attachment.imageView = m_swapchain_image_views[image_index];
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
        ui_callback(cmd, m_swapchain_image_views[image_index], m_swapchain_extent);
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

auto VulkanBackend::submit_and_present(uint32_t image_index, util::UniqueFd sync_fd)
    -> Result<void> {
    GOGGLES_PROFILE_SCOPE("SubmitPresent");

    auto& frame = m_frames[m_current_frame];
    vk::Semaphore render_finished_sem = m_render_finished_sems[image_index];

    // Import sync_fd as a temporary binary semaphore for GPU-side acquire wait
    vk::Semaphore acquire_sync_sem;
    bool have_acquire_sync = false;

    if (sync_fd.valid()) {
        vk::SemaphoreCreateInfo sem_ci{};
        auto [sem_res, sem] = m_device.createSemaphore(sem_ci);
        if (sem_res == vk::Result::eSuccess) {
            VkImportSemaphoreFdInfoKHR import_info{};
            import_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
            import_info.semaphore = sem;
            import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
            import_info.fd = sync_fd.get();
            import_info.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;

            auto import_res = static_cast<vk::Result>(
                VULKAN_HPP_DEFAULT_DISPATCHER.vkImportSemaphoreFdKHR(m_device, &import_info));
            if (import_res == vk::Result::eSuccess) {
                sync_fd.release();
                acquire_sync_sem = sem;
                have_acquire_sync = true;
            } else {
                m_device.destroySemaphore(sem);
                GOGGLES_LOG_WARN("Failed to import sync_fd as semaphore");
            }
        }
    }

    std::array<vk::Semaphore, 2> wait_sems;
    std::array<vk::PipelineStageFlags, 2> wait_stages;
    uint32_t wait_count = 1;
    wait_sems[0] = frame.image_available_sem;
    wait_stages[0] = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    if (have_acquire_sync) {
        wait_sems[wait_count] = acquire_sync_sem;
        wait_stages[wait_count] = vk::PipelineStageFlagBits::eFragmentShader;
        wait_count++;
    }

    vk::SubmitInfo submit_info{};
    submit_info.waitSemaphoreCount = wait_count;
    submit_info.pWaitSemaphores = wait_sems.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_sem;

    auto submit_result = m_graphics_queue.submit(submit_info, frame.in_flight_fence);
    if (submit_result != vk::Result::eSuccess) {
        if (have_acquire_sync) {
            m_device.destroySemaphore(acquire_sync_sem);
        }
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_result));
    }

    vk::PresentInfoKHR present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_sem;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &image_index;

    vk::PresentIdKHR present_id{};
    uint64_t present_value = 0;
    if (m_present_wait_supported) {
        present_value = ++m_present_id;
        present_id.swapchainCount = 1;
        present_id.pPresentIds = &present_value;
        present_info.pNext = &present_id;
    }

    auto present_result = m_graphics_queue.presentKHR(present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        m_needs_resize = true;
    } else if (present_result != vk::Result::eSuccess) {
        if (have_acquire_sync) {
            if (frame.pending_acquire_sync_sem) {
                m_device.destroySemaphore(frame.pending_acquire_sync_sem);
            }
            frame.pending_acquire_sync_sem = acquire_sync_sem;
        }
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Present failed: " + vk::to_string(present_result));
    }

    // Defer semaphore destruction until the in-flight fence signals
    if (have_acquire_sync) {
        if (frame.pending_acquire_sync_sem) {
            m_device.destroySemaphore(frame.pending_acquire_sync_sem);
        }
        frame.pending_acquire_sync_sem = acquire_sync_sem;
    }

    if (m_target_fps == 0) {
        // Uncapped mode: avoid any extra pacing waits.
    } else if (m_present_wait_supported && present_value > 0) {
        auto wait_result = apply_present_wait(present_value);
        if (!wait_result) {
            return make_error<void>(wait_result.error().code, wait_result.error().message);
        }
    } else {
        throttle_present();
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return {};
}

auto VulkanBackend::apply_present_wait(uint64_t present_id) -> Result<void> {
    if (m_target_fps == 0) {
        return {};
    }

    constexpr uint64_t MAX_TIMEOUT_NS = 1'000'000'000ULL; // 1 second max
    const uint64_t timeout_ns =
        std::min(MAX_TIMEOUT_NS, static_cast<uint64_t>(1'000'000'000ULL / m_target_fps));
    auto wait_result = static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkWaitForPresentKHR(
        m_device, m_swapchain, present_id, timeout_ns));
    if (wait_result == vk::Result::eSuccess || wait_result == vk::Result::eTimeout ||
        wait_result == vk::Result::eSuboptimalKHR) {
        return {};
    }
    if (wait_result == vk::Result::eErrorOutOfDateKHR ||
        wait_result == vk::Result::eErrorSurfaceLostKHR) {
        m_needs_resize = true;
        return {};
    }
    return make_error<void>(ErrorCode::vulkan_device_lost,
                            "vkWaitForPresentKHR failed: " + vk::to_string(wait_result));
}

void VulkanBackend::throttle_present() {
    if (m_target_fps == 0) {
        return;
    }

    using clock = std::chrono::steady_clock;
    auto frame_duration = std::chrono::nanoseconds(1'000'000'000ULL / m_target_fps);

    if (m_last_present_time.time_since_epoch().count() == 0) {
        m_last_present_time = clock::now();
        return;
    }

    auto next_frame = m_last_present_time + frame_duration;
    auto now = clock::now();
    if (now < next_frame) {
        std::this_thread::sleep_until(next_frame);
        m_last_present_time = next_frame;
    } else {
        m_last_present_time = now;
    }
}

auto VulkanBackend::submit_headless(vk::CommandBuffer cmd, const util::ExternalImageFrame* frame,
                                    FrameResources& hframe) -> Result<void> {
    // Import sync_fd as a temporary binary semaphore so the GPU waits for the
    // client to finish writing before sampling the imported DMA-BUF.
    vk::Semaphore acquire_sync_sem;
    bool have_acquire_sync = false;

    if (frame && frame->sync_fd.valid()) {
        auto sync_dup = util::UniqueFd::dup_from(frame->sync_fd.get());
        if (sync_dup.valid()) {
            vk::SemaphoreCreateInfo sem_ci{};
            auto [sem_res, sem] = m_device.createSemaphore(sem_ci);
            if (sem_res == vk::Result::eSuccess) {
                VkImportSemaphoreFdInfoKHR import_info{};
                import_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
                import_info.semaphore = sem;
                import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
                import_info.fd = sync_dup.get();
                import_info.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;

                auto import_res = static_cast<vk::Result>(
                    VULKAN_HPP_DEFAULT_DISPATCHER.vkImportSemaphoreFdKHR(m_device, &import_info));
                if (import_res == vk::Result::eSuccess) {
                    sync_dup.release();
                    acquire_sync_sem = sem;
                    have_acquire_sync = true;
                } else {
                    m_device.destroySemaphore(sem);
                    GOGGLES_LOG_WARN("Failed to import sync_fd as semaphore in headless path");
                }
            }
        }
    }

    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eFragmentShader;
    vk::SubmitInfo submit_info{};
    if (have_acquire_sync) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &acquire_sync_sem;
        submit_info.pWaitDstStageMask = &wait_stage;
    }
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    auto submit_result = m_graphics_queue.submit(submit_info, hframe.in_flight_fence);
    if (submit_result != vk::Result::eSuccess) {
        if (have_acquire_sync) {
            m_device.destroySemaphore(acquire_sync_sem);
        }
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Queue submit failed: " + vk::to_string(submit_result));
    }

    // Defer destruction until the fence signals on the next frame to avoid
    // destroying the semaphore while the GPU pipeline is still consuming it.
    if (have_acquire_sync) {
        hframe.pending_acquire_sync_sem = acquire_sync_sem;
    }

    return {};
}

auto VulkanBackend::render(const util::ExternalImageFrame* frame,
                           const UiRenderCallback& ui_callback) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Backend not initialized");
    }

    ++m_frame_count;
    check_pending_chain_swap();
    cleanup_deferred_destroys();

    if (m_headless) {
        auto& hframe = m_frames[0];

        auto wait_result = m_device.waitForFences(hframe.in_flight_fence, VK_TRUE, UINT64_MAX);
        if (wait_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_device_lost, "Fence wait failed");
        }
        auto reset_result = m_device.resetFences(hframe.in_flight_fence);
        if (reset_result != vk::Result::eSuccess) {
            return make_error<void>(ErrorCode::vulkan_device_lost,
                                    "Fence reset failed: " + vk::to_string(reset_result));
        }

        // Fence has signalled so the previous frame's wait semaphore is consumed
        if (hframe.pending_acquire_sync_sem) {
            m_device.destroySemaphore(hframe.pending_acquire_sync_sem);
            hframe.pending_acquire_sync_sem = nullptr;
        }

        auto cmd = hframe.command_buffer;
        VK_TRY(cmd.reset(), ErrorCode::vulkan_device_lost, "Command buffer reset failed");

        vk::CommandBufferBeginInfo begin_info{};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        VK_TRY(cmd.begin(begin_info), ErrorCode::vulkan_device_lost, "Command buffer begin failed");

        if (frame) {
            GOGGLES_TRY(import_external_image(frame->image));

            vk::ImageMemoryBarrier src_barrier{};
            src_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            src_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            src_barrier.oldLayout = vk::ImageLayout::eUndefined;
            src_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            src_barrier.image = m_import.image;
            src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            src_barrier.subresourceRange.levelCount = 1;
            src_barrier.subresourceRange.layerCount = 1;

            vk::ImageMemoryBarrier dst_barrier{};
            dst_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            dst_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            dst_barrier.oldLayout = vk::ImageLayout::eUndefined;
            dst_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            dst_barrier.image = m_offscreen_image;
            dst_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            dst_barrier.subresourceRange.levelCount = 1;
            dst_barrier.subresourceRange.layerCount = 1;

            std::array barriers = {src_barrier, dst_barrier};
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eFragmentShader |
                                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                {}, {}, {}, barriers);

            m_filter_chain->record(cmd, m_import.image, m_import.view, m_import_extent,
                                   m_offscreen_view, m_offscreen_extent, 0, m_scale_mode,
                                   m_integer_scale);
        } else {
            vk::ImageMemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            barrier.oldLayout = vk::ImageLayout::eUndefined;
            barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = m_offscreen_image;
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                barrier);

            vk::RenderingAttachmentInfo color_attachment{};
            color_attachment.imageView = m_offscreen_view;
            color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
            color_attachment.clearValue.color =
                vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}};

            vk::RenderingInfo rendering_info{};
            rendering_info.renderArea.offset = vk::Offset2D{0, 0};
            rendering_info.renderArea.extent = m_offscreen_extent;
            rendering_info.layerCount = 1;
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachments = &color_attachment;

            cmd.beginRendering(rendering_info);
            cmd.endRendering();
        }

        VK_TRY(cmd.end(), ErrorCode::vulkan_device_lost, "Command buffer end failed");

        return submit_headless(cmd, frame, hframe);
    }

    uint32_t image_index = GOGGLES_TRY(acquire_next_image());

    if (frame) {
        GOGGLES_TRY(import_external_image(frame->image));
        GOGGLES_TRY(record_render_commands(m_frames[m_current_frame].command_buffer, image_index,
                                           ui_callback));
    } else {
        GOGGLES_TRY(record_clear_commands(m_frames[m_current_frame].command_buffer, image_index,
                                          ui_callback));
    }

    util::UniqueFd acquire_fd;
    if (frame && frame->sync_fd.valid()) {
        acquire_fd = util::UniqueFd::dup_from(frame->sync_fd.get());
    }

    return submit_and_present(image_index, std::move(acquire_fd));
}

auto VulkanBackend::readback_to_png(const std::filesystem::path& output) -> Result<void> {
    if (!m_headless || !m_offscreen_image) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "readback_to_png requires headless mode");
    }

    auto& frame = m_frames[0];
    auto wait_result = m_device.waitForFences(frame.in_flight_fence, VK_TRUE, UINT64_MAX);
    if (wait_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_device_lost, "Fence wait failed before readback");
    }

    const uint32_t width = m_offscreen_extent.width;
    const uint32_t height = m_offscreen_extent.height;
    const vk::DeviceSize buffer_size = static_cast<vk::DeviceSize>(width) * height * 4;

    auto staging_result = create_readback_staging_buffer(m_device, m_physical_device, buffer_size);
    if (!staging_result) {
        return make_error<void>(staging_result.error().code, staging_result.error().message,
                                staging_result.error().location);
    }
    auto staging = staging_result.value();

    auto copy_result = submit_readback_copy(m_device, m_graphics_queue, frame.command_buffer,
                                            frame.in_flight_fence, m_offscreen_image,
                                            staging.buffer, width, height);
    if (!copy_result) {
        destroy_readback_staging_buffer(m_device, staging);
        return make_error<void>(copy_result.error().code, copy_result.error().message,
                                copy_result.error().location);
    }

    auto [map_result, data] = m_device.mapMemory(staging.memory, 0, buffer_size);
    if (map_result != vk::Result::eSuccess) {
        destroy_readback_staging_buffer(m_device, staging);
        return make_error<void>(ErrorCode::vulkan_device_lost,
                                "Failed to map staging memory: " + vk::to_string(map_result));
    }

    if (!staging.is_coherent) {
        vk::MappedMemoryRange range{};
        range.memory = staging.memory;
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;
        auto invalidate = m_device.invalidateMappedMemoryRanges(range);
        if (invalidate != vk::Result::eSuccess) {
            GOGGLES_LOG_WARN("invalidateMappedMemoryRanges failed: {}", vk::to_string(invalidate));
        }
    }

    int png_result = stbi_write_png(output.c_str(), static_cast<int>(width),
                                    static_cast<int>(height), 4, data, static_cast<int>(width * 4));

    m_device.unmapMemory(staging.memory);
    destroy_readback_staging_buffer(m_device, staging);

    if (png_result == 0) {
        return make_error<void>(ErrorCode::file_write_failed,
                                "stbi_write_png failed for: " + output.string());
    }

    GOGGLES_LOG_INFO("PNG written: {} ({}x{})", output.string(), width, height);
    return {};
}

auto VulkanBackend::reload_shader_preset(const std::filesystem::path& preset_path) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (!m_device) {
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
    auto cache_dir = m_cache_dir;
    auto device = m_device;
    auto physical_device = m_physical_device;
    auto command_pool = m_command_pool;
    auto graphics_queue = m_graphics_queue;
    auto source_resolution = m_source_resolution;

    m_pending_load_future = util::JobSystem::submit([=, this]() -> Result<void> {
        GOGGLES_PROFILE_SCOPE("AsyncShaderLoad");

        auto runtime_result = ShaderRuntime::create(cache_dir);
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

        auto chain_result =
            FilterChain::create(vk_ctx, swapchain_format, MAX_FRAMES_IN_FLIGHT,
                                *runtime_result.value(), shader_dir, source_resolution);
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
    apply_filter_chain_policy();
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

void VulkanBackend::set_filter_chain_policy(const FilterChainStagePolicy& policy) {
    m_prechain_policy_enabled = policy.prechain_enabled;
    m_effect_stage_policy_enabled = policy.effect_stage_enabled;
    apply_filter_chain_policy();
}

void VulkanBackend::apply_filter_chain_policy() {
    if (!m_filter_chain) {
        return;
    }
    m_filter_chain->set_prechain_enabled(m_prechain_policy_enabled);
    m_filter_chain->set_bypass(!m_effect_stage_policy_enabled);
}

} // namespace goggles::render
