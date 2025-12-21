#include "texture_loader.hpp"

#include <cmath>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <util/logging.hpp>

namespace goggles::render {

namespace {
constexpr int RGBA_CHANNELS = 4;
} // namespace

TextureLoader::TextureLoader(vk::Device device, vk::PhysicalDevice physical_device,
                             vk::CommandPool cmd_pool, vk::Queue queue)
    : m_device(device), m_physical_device(physical_device), m_cmd_pool(cmd_pool), m_queue(queue) {
    auto srgb_props = physical_device.getFormatProperties(vk::Format::eR8G8B8A8Srgb);
    m_srgb_supports_linear = static_cast<bool>(
        srgb_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

    auto unorm_props = physical_device.getFormatProperties(vk::Format::eR8G8B8A8Unorm);
    m_unorm_supports_linear = static_cast<bool>(
        unorm_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
}

auto TextureLoader::load_from_file(const std::filesystem::path& path,
                                    const TextureLoadConfig& config) -> Result<TextureData> {
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, RGBA_CHANNELS);
    if (pixels == nullptr) {
        return make_error<TextureData>(ErrorCode::FILE_NOT_FOUND,
                                       "Failed to load texture: " + path.string());
    }
    if (width <= 0 || height <= 0) {
      stbi_image_free(pixels);
      return make_error<TextureData>(ErrorCode::INVALID_DATA,
                                     "Invalid texture dimensions: " + path.string());
    }

    uint32_t mip_levels = 1;
    if (config.generate_mipmaps) {
        mip_levels = calculate_mip_levels(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }

    auto result = upload_to_gpu(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 mip_levels, config.linear);

    stbi_image_free(pixels);

    if (!result) {
        return result;
    }

    GOGGLES_LOG_DEBUG("Loaded texture: {} ({}x{}, {} mip levels)", path.filename().string(), width,
                      height, mip_levels);

    return result;
}

auto TextureLoader::create_staging_buffer(vk::DeviceSize size, const uint8_t* pixels)
    -> Result<StagingResources> {
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    auto [buffer_result, buffer] = m_device.createBufferUnique(buffer_info);
    if (buffer_result != vk::Result::eSuccess) {
        return make_error<StagingResources>(ErrorCode::VULKAN_INIT_FAILED,
                                            "Failed to create staging buffer: " +
                                                vk::to_string(buffer_result));
    }

    auto mem_reqs = m_device.getBufferMemoryRequirements(*buffer);
    uint32_t mem_type = find_memory_type(
        mem_reqs.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (mem_type == UINT32_MAX) {
        return make_error<StagingResources>(ErrorCode::VULKAN_INIT_FAILED,
                                            "No suitable memory type for staging buffer");
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    auto [mem_result, memory] = m_device.allocateMemoryUnique(alloc_info);
    if (mem_result != vk::Result::eSuccess) {
        return make_error<StagingResources>(ErrorCode::VULKAN_INIT_FAILED,
                                            "Failed to allocate staging memory: " +
                                                vk::to_string(mem_result));
    }

    auto bind_result = m_device.bindBufferMemory(*buffer, *memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        return make_error<StagingResources>(ErrorCode::VULKAN_INIT_FAILED,
                                            "Failed to bind staging buffer memory: " +
                                                vk::to_string(bind_result));
    }

    auto [map_result, data] = m_device.mapMemory(*memory, 0, size);
    if (map_result != vk::Result::eSuccess) {
        return make_error<StagingResources>(ErrorCode::VULKAN_INIT_FAILED,
                                            "Failed to map staging memory: " + vk::to_string(map_result));
    }
    std::memcpy(data, pixels, size);
    m_device.unmapMemory(*memory);

    return StagingResources{.buffer = std::move(buffer), .memory = std::move(memory)};
}

auto TextureLoader::create_texture_image(ImageSize size, uint32_t mip_levels, bool linear)
    -> Result<ImageResources> {
    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = linear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
    image_info.extent = vk::Extent3D{size.width, size.height, 1};
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                       vk::ImageUsageFlagBits::eSampled;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    auto [image_result, image] = m_device.createImageUnique(image_info);
    if (image_result != vk::Result::eSuccess) {
        return make_error<ImageResources>(ErrorCode::VULKAN_INIT_FAILED,
                                          "Failed to create image: " + vk::to_string(image_result));
    }

    auto mem_reqs = m_device.getImageMemoryRequirements(*image);
    uint32_t mem_type =
        find_memory_type(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (mem_type == UINT32_MAX) {
        return make_error<ImageResources>(ErrorCode::VULKAN_INIT_FAILED,
                                          "No suitable memory type for image");
    }

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    auto [mem_result, memory] = m_device.allocateMemoryUnique(alloc_info);
    if (mem_result != vk::Result::eSuccess) {
        return make_error<ImageResources>(ErrorCode::VULKAN_INIT_FAILED,
                                          "Failed to allocate image memory: " +
                                              vk::to_string(mem_result));
    }

    auto bind_result = m_device.bindImageMemory(*image, *memory, 0);
    if (bind_result != vk::Result::eSuccess) {
        return make_error<ImageResources>(ErrorCode::VULKAN_INIT_FAILED,
                                          "Failed to bind image memory: " + vk::to_string(bind_result));
    }

    return ImageResources{.image = std::move(image), .memory = std::move(memory)};
}

auto TextureLoader::record_and_submit_transfer(vk::Buffer staging_buffer, vk::Image image,
                                                ImageSize size, uint32_t mip_levels,
                                                vk::Format format) -> Result<void> {
    vk::CommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.commandPool = m_cmd_pool;
    cmd_alloc_info.level = vk::CommandBufferLevel::ePrimary;
    cmd_alloc_info.commandBufferCount = 1;

    auto [cmd_result, cmd_buffers] = m_device.allocateCommandBuffersUnique(cmd_alloc_info);
    if (cmd_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to allocate command buffer: " + vk::to_string(cmd_result));
    }
    auto& cmd = cmd_buffers[0];

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    auto begin_result = cmd->begin(begin_info);
    if (begin_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to begin command buffer: " + vk::to_string(begin_result));
    }

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                         vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{size.width, size.height, 1};

    cmd->copyBufferToImage(staging_buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    if (mip_levels > 1) {
        generate_mipmaps(*cmd, image, format, vk::Extent2D{size.width, size.height}, mip_levels);
    } else {
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                             vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
    }

    auto end_result = cmd->end();
    if (end_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to end command buffer: " + vk::to_string(end_result));
    }

    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &(*cmd);

    auto submit_result = m_queue.submit(1, &submit_info, nullptr);
    if (submit_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to submit transfer command buffer: " +
                                    vk::to_string(submit_result));
    }

    auto wait_result = m_queue.waitIdle();
    if (wait_result != vk::Result::eSuccess) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED,
                                "Failed to wait for queue idle: " + vk::to_string(wait_result));
    }

    return {};
}

auto TextureLoader::upload_to_gpu(const uint8_t* pixels, uint32_t width, uint32_t height,
                                   uint32_t mip_levels, bool linear) -> Result<TextureData> {
    vk::DeviceSize image_size = static_cast<vk::DeviceSize>(width) * height * RGBA_CHANNELS;

    vk::Format format = linear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;

    auto staging = GOGGLES_TRY(create_staging_buffer(image_size, pixels));
    auto image_resources =
        GOGGLES_TRY(create_texture_image(ImageSize{width, height}, mip_levels, linear));

    GOGGLES_TRY(record_and_submit_transfer(*staging.buffer, *image_resources.image,
                                            ImageSize{width, height}, mip_levels, format));

    vk::ImageViewCreateInfo view_info{};
    view_info.image = *image_resources.image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    auto [view_result, view] = m_device.createImageViewUnique(view_info);
    if (view_result != vk::Result::eSuccess) {
        return make_error<TextureData>(ErrorCode::VULKAN_INIT_FAILED,
                                       "Failed to create image view: " + vk::to_string(view_result));
    }

    return TextureData{
        .image = std::move(image_resources.image),
        .memory = std::move(image_resources.memory),
        .view = std::move(view),
        .extent = vk::Extent2D{width, height},
        .mip_levels = mip_levels,
    };
}

void TextureLoader::generate_mipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Format format,
                                      vk::Extent2D extent, uint32_t mip_levels) {
    bool supports_linear = (format == vk::Format::eR8G8B8A8Srgb) ? m_srgb_supports_linear
                                                                   : m_unorm_supports_linear;
    vk::Filter filter = supports_linear ? vk::Filter::eLinear : vk::Filter::eNearest;

    if (!supports_linear) {
        GOGGLES_LOG_WARN("Format {} does not support linear filtering for mipmaps, using nearest",
                         vk::to_string(format));
    }

    vk::ImageMemoryBarrier barrier{};
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    auto mip_width = static_cast<int32_t>(extent.width);
    auto mip_height = static_cast<int32_t>(extent.height);

    for (uint32_t i = 1; i < mip_levels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

        vk::ImageBlit blit{};
        blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.srcOffsets[1] = vk::Offset3D{mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.dstOffsets[1] = vk::Offset3D{mip_width > 1 ? mip_width / 2 : 1,
                                          mip_height > 1 ? mip_height / 2 : 1, 1};
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        cmd.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                      vk::ImageLayout::eTransferDstOptimal, blit, filter);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mip_width > 1) {
            mip_width /= 2;
        }
        if (mip_height > 1) {
            mip_height /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
}

auto TextureLoader::find_memory_type(uint32_t type_filter,
                                      vk::MemoryPropertyFlags properties) -> uint32_t {
    auto mem_props = m_physical_device.getMemoryProperties();

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1U << i)) != 0U &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

auto TextureLoader::calculate_mip_levels(uint32_t width, uint32_t height) -> uint32_t {
    if (width == 0 || height == 0) {
        return 1;
    }
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

} // namespace goggles::render
