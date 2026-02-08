#pragma once

#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief Offscreen framebuffer backing a `vk::ImageView` for passes and history.
class Framebuffer {
public:
    /// @brief Creates a framebuffer with an image, memory, and view.
    /// @return A framebuffer or an error.
    [[nodiscard]] static auto create(vk::Device device, vk::PhysicalDevice physical_device,
                                     vk::Format format, vk::Extent2D extent)
        -> ResultPtr<Framebuffer>;

    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    /// @brief Resizes the framebuffer image and view.
    [[nodiscard]] auto resize(vk::Extent2D new_extent) -> Result<void>;
    /// @brief Releases image and view resources.
    void shutdown();

    [[nodiscard]] auto view() const -> vk::ImageView { return m_view; }
    [[nodiscard]] auto image() const -> vk::Image { return m_image; }
    [[nodiscard]] auto format() const -> vk::Format { return m_format; }
    [[nodiscard]] auto extent() const -> vk::Extent2D { return m_extent; }

private:
    Framebuffer() = default;
    [[nodiscard]] auto create_image() -> Result<void>;
    [[nodiscard]] auto allocate_memory() -> Result<void>;
    [[nodiscard]] auto create_image_view() -> Result<void>;

    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;
    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent2D m_extent;

    vk::Image m_image;
    vk::DeviceMemory m_memory;
    vk::ImageView m_view;
};

} // namespace goggles::render
