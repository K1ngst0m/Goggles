#include "gpu_timestamp_pool.hpp"

#include <format>
#include <vulkan/vulkan.hpp>

namespace goggles::diagnostics {

GpuTimestampPool::~GpuTimestampPool() {
    if (m_device && m_pool) {
        m_device.destroyQueryPool(m_pool);
    }
}

auto GpuTimestampPool::create(vk::Device device, vk::PhysicalDevice physical_device,
                              uint32_t max_passes, uint32_t frames_in_flight)
    -> Result<std::unique_ptr<GpuTimestampPool>> {
    auto pool = create_unavailable();
    pool->m_device = device;
    pool->m_max_passes = std::max(max_passes, 1U);
    pool->m_frames_in_flight = std::max(frames_in_flight, 1U);

    const auto properties = physical_device.getProperties();
    pool->m_timestamp_period = properties.limits.timestampPeriod;
    if (pool->m_timestamp_period <= 0.0F) {
        return pool;
    }

    pool->m_queries_per_frame = 2U * (pool->m_max_passes + 2U);

    vk::QueryPoolCreateInfo create_info{};
    create_info.queryType = vk::QueryType::eTimestamp;
    create_info.queryCount = pool->m_queries_per_frame * pool->m_frames_in_flight;

    vk::QueryPool query_pool{};
    const auto result = static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateQueryPool(
        static_cast<VkDevice>(device), reinterpret_cast<const VkQueryPoolCreateInfo*>(&create_info),
        nullptr, reinterpret_cast<VkQueryPool*>(&query_pool)));
    if (result != vk::Result::eSuccess) {
        return make_error<std::unique_ptr<GpuTimestampPool>>(
            ErrorCode::vulkan_init_failed,
            std::format("Failed to create GPU timestamp pool: {}", vk::to_string(result)));
    }

    pool->m_pool = query_pool;
    pool->m_available = true;
    return pool;
}

auto GpuTimestampPool::create_unavailable() -> std::unique_ptr<GpuTimestampPool> {
    auto pool = std::unique_ptr<GpuTimestampPool>(new GpuTimestampPool());
    pool->m_available = false;
    return pool;
}

void GpuTimestampPool::reset_frame(vk::CommandBuffer cmd, uint32_t frame_index) {
    if (!m_available) {
        return;
    }

    const auto first_query = frame_index * m_queries_per_frame;
    cmd.resetQueryPool(m_pool, first_query, m_queries_per_frame);
}

void GpuTimestampPool::write_pass_timestamp(vk::CommandBuffer cmd, uint32_t frame_index,
                                            uint32_t pass_ordinal, bool is_start) {
    if (!m_available || pass_ordinal >= m_max_passes) {
        return;
    }

    cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_pool,
                       query_index(frame_index, pass_ordinal, is_start));
}

void GpuTimestampPool::write_prechain_timestamp(vk::CommandBuffer cmd, uint32_t frame_index,
                                                bool is_start) {
    if (!m_available) {
        return;
    }

    cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_pool,
                       query_index(frame_index, m_max_passes, is_start));
}

void GpuTimestampPool::write_final_composition_timestamp(vk::CommandBuffer cmd,
                                                         uint32_t frame_index, bool is_start) {
    if (!m_available) {
        return;
    }

    cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_pool,
                       query_index(frame_index, m_max_passes + 1U, is_start));
}

auto GpuTimestampPool::read_results(uint32_t frame_index)
    -> Result<std::vector<GpuTimestampSample>> {
    std::vector<GpuTimestampSample> durations;
    if (!m_available) {
        return durations;
    }

    std::vector<uint64_t> raw_results(m_queries_per_frame, 0);
    const auto result = m_device.getQueryPoolResults(
        m_pool, frame_index * m_queries_per_frame, m_queries_per_frame,
        static_cast<size_t>(raw_results.size()) * sizeof(uint64_t), raw_results.data(),
        sizeof(uint64_t), vk::QueryResultFlagBits::e64);

    if (result == vk::Result::eNotReady) {
        return durations;
    }
    if (result != vk::Result::eSuccess) {
        return make_error<std::vector<GpuTimestampSample>>(
            ErrorCode::vulkan_init_failed,
            std::format("Failed to read GPU timestamp results: {}", vk::to_string(result)));
    }

    durations.reserve(static_cast<size_t>(m_max_passes) + 2U);
    for (uint32_t query_slot = 0; query_slot < m_max_passes + 2U; ++query_slot) {
        const auto start_index = 2ULL * static_cast<size_t>(query_slot);
        const auto end_index = start_index + 1U;
        const auto start = raw_results[start_index];
        const auto end = raw_results[end_index];
        if (start == 0 || end == 0 || end < start) {
            continue;
        }

        const auto duration_ticks = static_cast<double>(end - start);
        GpuTimestampSample sample{};
        sample.duration_us = (duration_ticks * static_cast<double>(m_timestamp_period)) / 1000.0;
        if (query_slot < m_max_passes) {
            sample.region = GpuTimestampRegion::pass;
            sample.pass_ordinal = query_slot;
        } else if (query_slot == m_max_passes) {
            sample.region = GpuTimestampRegion::prechain;
            sample.pass_ordinal = 0;
        } else {
            sample.region = GpuTimestampRegion::final_composition;
            sample.pass_ordinal = 0;
        }
        durations.push_back(sample);
    }

    return durations;
}

void GpuTimestampPool::disable() {
    m_available = false;
}

auto GpuTimestampPool::is_available() const -> bool {
    return m_available;
}

auto GpuTimestampPool::query_index(uint32_t frame_index, uint32_t query_slot, bool is_start) const
    -> uint32_t {
    return (frame_index * m_queries_per_frame) + (2U * query_slot) + (is_start ? 0U : 1U);
}

} // namespace goggles::diagnostics
