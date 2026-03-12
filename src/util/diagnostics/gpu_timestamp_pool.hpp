#pragma once

#include <cstdint>
#include <memory>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::diagnostics {

enum class GpuTimestampRegion : uint8_t { pass, prechain, final_composition };

struct GpuTimestampSample {
    GpuTimestampRegion region = GpuTimestampRegion::pass;
    uint32_t pass_ordinal = 0;
    double duration_us = 0.0;
};

struct GpuTimestampPoolCreateInfo {
    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t max_passes = 0;
    uint32_t frames_in_flight = 0;
};

class GpuTimestampPool {
public:
    ~GpuTimestampPool();

    GpuTimestampPool(const GpuTimestampPool&) = delete;
    auto operator=(const GpuTimestampPool&) -> GpuTimestampPool& = delete;
    GpuTimestampPool(GpuTimestampPool&&) = delete;
    auto operator=(GpuTimestampPool&&) -> GpuTimestampPool& = delete;

    [[nodiscard]] static auto create(vk::Device device, vk::PhysicalDevice physical_device,
                                     GpuTimestampPoolCreateInfo create_info)
        -> Result<std::unique_ptr<GpuTimestampPool>>;

    [[nodiscard]] static auto supports_timestamps(vk::PhysicalDevice physical_device,
                                                  uint32_t graphics_queue_family_index) -> bool;

    /// @brief Creates an explicit unavailable, no-op pool for deterministic diagnostics coverage.
    [[nodiscard]] static auto create_unavailable() -> std::unique_ptr<GpuTimestampPool>;

    void reset_frame(vk::CommandBuffer cmd, uint32_t frame_index);
    void write_pass_timestamp(vk::CommandBuffer cmd, uint32_t frame_index, uint32_t pass_ordinal,
                              bool is_start);
    void write_prechain_timestamp(vk::CommandBuffer cmd, uint32_t frame_index, bool is_start);
    void write_final_composition_timestamp(vk::CommandBuffer cmd, uint32_t frame_index,
                                           bool is_start);
    [[nodiscard]] auto read_results(uint32_t frame_index)
        -> Result<std::vector<GpuTimestampSample>>;

    void disable();
    [[nodiscard]] auto is_available() const -> bool;

private:
    GpuTimestampPool() = default;

    [[nodiscard]] auto query_index(uint32_t frame_index, uint32_t query_slot, bool is_start) const
        -> uint32_t;

    vk::Device m_device;
    vk::QueryPool m_pool;
    float m_timestamp_period = 0.0F;
    uint32_t m_max_passes = 0;
    uint32_t m_queries_per_frame = 0;
    uint32_t m_frames_in_flight = 0;
    bool m_available = false;
};

} // namespace goggles::diagnostics
