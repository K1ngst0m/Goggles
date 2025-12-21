#pragma once

#include <filesystem>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

struct ImageSize {
    uint32_t width;
    uint32_t height;
};

struct TextureData {
    vk::UniqueImage image;
    vk::UniqueDeviceMemory memory;
    vk::UniqueImageView view;
    vk::Extent2D extent{0, 0};
    uint32_t mip_levels{1};
};

struct TextureLoadConfig {
    bool generate_mipmaps{false};
    bool linear{false};
};

class TextureLoader {
public:
    TextureLoader(vk::Device device, vk::PhysicalDevice physical_device,
                  vk::CommandPool cmd_pool, vk::Queue queue);

    [[nodiscard]] auto load_from_file(const std::filesystem::path& path,
                                       const TextureLoadConfig& config = {}) -> Result<TextureData>;

private:
    struct StagingResources {
        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;
    };

    struct ImageResources {
        vk::UniqueImage image;
        vk::UniqueDeviceMemory memory;
    };

    [[nodiscard]] auto upload_to_gpu(const uint8_t* pixels, uint32_t width, uint32_t height,
                                      uint32_t mip_levels, bool linear) -> Result<TextureData>;

    [[nodiscard]] auto create_staging_buffer(vk::DeviceSize size, const uint8_t* pixels)
        -> Result<StagingResources>;

    [[nodiscard]] auto create_texture_image(ImageSize size, uint32_t mip_levels, bool linear)
        -> Result<ImageResources>;

    [[nodiscard]] auto record_and_submit_transfer(vk::Buffer staging_buffer, vk::Image image,
                                                   ImageSize size, uint32_t mip_levels,
                                                   vk::Format format) -> Result<void>;

    void generate_mipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Format format,
                          vk::Extent2D extent, uint32_t mip_levels);

    [[nodiscard]] auto find_memory_type(uint32_t type_filter,
                                         vk::MemoryPropertyFlags properties) -> uint32_t;

    [[nodiscard]] static auto calculate_mip_levels(uint32_t width, uint32_t height) -> uint32_t;

    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;
    vk::CommandPool m_cmd_pool;
    vk::Queue m_queue;

    bool m_srgb_supports_linear;
    bool m_unorm_supports_linear;
};

} // namespace goggles::render
