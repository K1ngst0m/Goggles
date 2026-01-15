#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <render/chain/framebuffer.hpp>

namespace goggles::render {

class FrameHistory {
public:
    static constexpr uint32_t MAX_HISTORY = 7; // OriginalHistory0-6

    FrameHistory() = default;

    /// @brief Initializes history buffers for the given format and extent.
    [[nodiscard]] auto init(vk::Device device, vk::PhysicalDevice physical_device,
                            vk::Format format, vk::Extent2D extent, uint32_t depth) -> Result<void>;

    /// @brief Pushes a new frame into history (copying from `source`).
    void push(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D extent);

    /// @brief Returns the image view for a history frame age (0 = most recent).
    [[nodiscard]] auto get(uint32_t age) const -> vk::ImageView;
    /// @brief Returns the extent for a history frame age (0 = most recent).
    [[nodiscard]] auto get_extent(uint32_t age) const -> vk::Extent2D;

    [[nodiscard]] auto depth() const -> uint32_t { return m_depth; }
    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

    /// @brief Releases all history buffers.
    void shutdown();

private:
    std::array<std::unique_ptr<Framebuffer>, MAX_HISTORY> m_buffers;
    uint32_t m_write_index = 0;
    uint32_t m_depth = 0;
    uint32_t m_frame_count = 0;
    bool m_initialized = false;
};

} // namespace goggles::render
