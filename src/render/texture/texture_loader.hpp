#pragma once

#include <filesystem>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief Width/height pair used during texture upload.
struct ImageSize {
    uint32_t width;
    uint32_t height;
};

/// @brief Loaded GPU texture resources.
struct TextureData {
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageView view;
    vk::Extent2D extent{0, 0};
    uint32_t mip_levels{1};
};

/// @brief Options controlling texture loading and mipmap generation.
struct TextureLoadConfig {
    bool generate_mipmaps{false};
    bool linear{false};
};

/// @brief Loads textures from disk and uploads them to Vulkan images.
class TextureLoader {
public:
    TextureLoader(vk::Device device, vk::PhysicalDevice physical_device, vk::CommandPool cmd_pool,
                  vk::Queue queue);

    /// @brief Loads an image file and uploads it to the GPU.
    /// @param path Path to the image file.
    /// @param config Loading options.
    /// @return Texture data or an error.
    [[nodiscard]] auto load_from_file(const std::filesystem::path& path,
                                      const TextureLoadConfig& config = {}) -> Result<TextureData>;

private:
    struct StagingResources {
        vk::Buffer buffer;
        vk::DeviceMemory memory;
    };

    struct ImageResources {
        vk::Image image;
        vk::DeviceMemory memory;
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

    [[nodiscard]] auto find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties)
        -> uint32_t;

    [[nodiscard]] static auto calculate_mip_levels(uint32_t width, uint32_t height) -> uint32_t;

    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;
    vk::CommandPool m_cmd_pool;
    vk::Queue m_queue;

    bool m_srgb_supports_linear;
    bool m_unorm_supports_linear;
};

} // namespace goggles::render
