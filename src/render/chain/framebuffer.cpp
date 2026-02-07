#include "framebuffer.hpp"

#include <render/backend/vulkan_error.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& mem_props, uint32_t type_bits,
                      vk::MemoryPropertyFlags required_flags) -> uint32_t {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_bits & (1U << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & required_flags) == required_flags) {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace

Framebuffer::~Framebuffer() {
    shutdown();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_device(other.m_device), m_physical_device(other.m_physical_device),
      m_format(other.m_format), m_extent(other.m_extent), m_image(std::move(other.m_image)),
      m_memory(std::move(other.m_memory)), m_view(std::move(other.m_view)) {}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_device = other.m_device;
        m_physical_device = other.m_physical_device;
        m_format = other.m_format;
        m_extent = other.m_extent;
        m_image = std::move(other.m_image);
        m_memory = std::move(other.m_memory);
        m_view = std::move(other.m_view);
    }
    return *this;
}

auto Framebuffer::create(vk::Device device, vk::PhysicalDevice physical_device, vk::Format format,
                         vk::Extent2D extent) -> ResultPtr<Framebuffer> {
    GOGGLES_PROFILE_FUNCTION();
    auto framebuffer = std::unique_ptr<Framebuffer>(new Framebuffer());

    framebuffer->m_device = device;
    framebuffer->m_physical_device = physical_device;
    framebuffer->m_format = format;
    framebuffer->m_extent = extent;

    GOGGLES_TRY(framebuffer->create_image());
    GOGGLES_TRY(framebuffer->allocate_memory());
    GOGGLES_TRY(framebuffer->create_image_view());

    GOGGLES_LOG_DEBUG("Framebuffer created: {}x{}, format={}", extent.width, extent.height,
                      vk::to_string(format));
    return make_result_ptr(std::move(framebuffer));
}

auto Framebuffer::resize(vk::Extent2D new_extent) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    if (m_extent == new_extent) {
        return {};
    }

    m_view.reset();
    m_memory.reset();
    m_image.reset();

    m_extent = new_extent;

    GOGGLES_TRY(create_image());
    GOGGLES_TRY(allocate_memory());
    GOGGLES_TRY(create_image_view());

    GOGGLES_LOG_DEBUG("Framebuffer resized: {}x{}", new_extent.width, new_extent.height);
    return {};
}

void Framebuffer::shutdown() {
    GOGGLES_PROFILE_FUNCTION();
    if (m_device) {
        static_cast<void>(m_device.waitIdle());
    }
    m_view.reset();
    m_memory.reset();
    m_image.reset();
    m_format = vk::Format::eUndefined;
    m_extent = vk::Extent2D{0, 0};
}

auto Framebuffer::create_image() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = m_format;
    image_info.extent = vk::Extent3D{m_extent.width, m_extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                       vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    auto [result, image] = m_device.createImageUnique(image_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create framebuffer image: " + vk::to_string(result));
    }
    m_image = std::move(image);
    return {};
}

auto Framebuffer::allocate_memory() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    auto mem_reqs = m_device.getImageMemoryRequirements(*m_image);
    auto mem_props = m_physical_device.getMemoryProperties();

    uint32_t mem_type_index = find_memory_type(mem_props, mem_reqs.memoryTypeBits,
                                               vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (mem_type_index == UINT32_MAX) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "No suitable memory type for framebuffer");
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type_index;

    auto [result, memory] = m_device.allocateMemoryUnique(alloc_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to allocate framebuffer memory: " + vk::to_string(result));
    }
    m_memory = std::move(memory);

    VK_TRY(m_device.bindImageMemory(*m_image, *m_memory, 0), ErrorCode::vulkan_init_failed,
           "Failed to bind framebuffer memory");

    return {};
}

auto Framebuffer::create_image_view() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    vk::ImageViewCreateInfo view_info{};
    view_info.image = *m_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = m_format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [result, view] = m_device.createImageViewUnique(view_info);
    if (result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::vulkan_init_failed,
                                "Failed to create framebuffer image view: " +
                                    vk::to_string(result));
    }
    m_view = std::move(view);
    return {};
}

} // namespace goggles::render
