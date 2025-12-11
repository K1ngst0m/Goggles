#pragma once

#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    [[nodiscard]] auto init(vk::Device device, vk::PhysicalDevice physical_device,
                            vk::Format format, vk::Extent2D extent) -> Result<void>;
    [[nodiscard]] auto resize(vk::Extent2D new_extent) -> Result<void>;
    void shutdown();

    [[nodiscard]] auto view() const -> vk::ImageView { return *m_view; }
    [[nodiscard]] auto image() const -> vk::Image { return *m_image; }
    [[nodiscard]] auto format() const -> vk::Format { return m_format; }
    [[nodiscard]] auto extent() const -> vk::Extent2D { return m_extent; }
    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

private:
    [[nodiscard]] auto create_image() -> Result<void>;
    [[nodiscard]] auto allocate_memory() -> Result<void>;
    [[nodiscard]] auto create_image_view() -> Result<void>;

    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;
    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent2D m_extent{};

    vk::UniqueImage m_image;
    vk::UniqueDeviceMemory m_memory;
    vk::UniqueImageView m_view;

    bool m_initialized = false;
};

} // namespace goggles::render
