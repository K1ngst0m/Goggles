#pragma once

#include <array>
#include <cstdint>
#include <render/chain/framebuffer.hpp>

namespace goggles::render {

class FrameHistory {
public:
    static constexpr uint32_t MAX_HISTORY = 7;  // OriginalHistory0-6

    FrameHistory() = default;

    [[nodiscard]] auto init(vk::Device device, vk::PhysicalDevice physical_device,
                            vk::Format format, vk::Extent2D extent, uint32_t depth)
        -> Result<void>;

    void push(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D extent);

    [[nodiscard]] auto get(uint32_t age) const -> vk::ImageView;
    [[nodiscard]] auto get_extent(uint32_t age) const -> vk::Extent2D;

    [[nodiscard]] auto depth() const -> uint32_t { return m_depth; }
    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

    void shutdown();

private:
    std::array<Framebuffer, MAX_HISTORY> m_buffers;
    uint32_t m_write_index = 0;
    uint32_t m_depth = 0;
    uint32_t m_frame_count = 0;
    bool m_initialized = false;
};

}  // namespace goggles::render