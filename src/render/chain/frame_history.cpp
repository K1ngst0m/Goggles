#include "frame_history.hpp"

#include <algorithm>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

auto FrameHistory::init(vk::Device device, vk::PhysicalDevice physical_device, vk::Format format,
                        vk::Extent2D extent, uint32_t depth) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    if (m_initialized) {
        return {};
    }

    m_depth = std::min(depth, MAX_HISTORY);
    if (m_depth == 0) {
        m_initialized = true;
        return {};
    }

    for (uint32_t i = 0; i < m_depth; ++i) {
        m_buffers[i] = GOGGLES_TRY(Framebuffer::create(device, physical_device, format, extent));
    }

    m_initialized = true;
    GOGGLES_LOG_DEBUG("FrameHistory initialized with depth {}", m_depth);
    return {};
}

void FrameHistory::push(vk::CommandBuffer cmd, vk::Image source, vk::Extent2D extent) {
    GOGGLES_PROFILE_FUNCTION();
    if (!m_initialized || m_depth == 0) {
        return;
    }

    auto& target = m_buffers[m_write_index];

    auto target_extent = target->extent();
    if (extent.width != target_extent.width || extent.height != target_extent.height) {
        GOGGLES_LOG_WARN("FrameHistory::push extent mismatch: {}x{} vs {}x{}", extent.width,
                         extent.height, target_extent.width, target_extent.height);
        return;
    }

    vk::ImageMemoryBarrier src_barrier{};
    src_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    src_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    src_barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    src_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = source;
    src_barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::ImageMemoryBarrier dst_barrier{};
    dst_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    dst_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    dst_barrier.oldLayout = vk::ImageLayout::eUndefined;
    dst_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.image = target->image();
    dst_barrier.subresourceRange = src_barrier.subresourceRange;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                        {src_barrier, dst_barrier});

    vk::ImageCopy region{};
    region.srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.extent = vk::Extent3D{extent.width, extent.height, 1};

    cmd.copyImage(source, vk::ImageLayout::eTransferSrcOptimal, target->image(),
                  vk::ImageLayout::eTransferDstOptimal, region);

    vk::ImageMemoryBarrier post_src{};
    post_src.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    post_src.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    post_src.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    post_src.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    post_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_src.image = source;
    post_src.subresourceRange = src_barrier.subresourceRange;

    vk::ImageMemoryBarrier post_dst{};
    post_dst.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    post_dst.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    post_dst.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    post_dst.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    post_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_dst.image = target->image();
    post_dst.subresourceRange = src_barrier.subresourceRange;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
                        {post_src, post_dst});

    m_write_index = (m_write_index + 1) % m_depth;
    m_frame_count++;
}

auto FrameHistory::get(uint32_t age) const -> vk::ImageView {
    if (!m_initialized || m_depth == 0 || age >= m_depth) {
        return nullptr;
    }
    if (m_frame_count <= age) {
        return nullptr; // Not enough frames yet
    }
    uint32_t idx = (m_write_index + m_depth - 1 - age) % m_depth;
    return m_buffers[idx]->view();
}

auto FrameHistory::get_extent(uint32_t age) const -> vk::Extent2D {
    if (!m_initialized || m_depth == 0 || m_frame_count <= age || age >= m_depth) {
        return {0, 0};
    }
    uint32_t idx = (m_write_index + m_depth - 1 - age) % m_depth;
    return m_buffers[idx]->extent();
}

void FrameHistory::shutdown() {
    GOGGLES_PROFILE_FUNCTION();
    for (auto& buf : m_buffers) {
        buf.reset();
    }
    m_write_index = 0;
    m_depth = 0;
    m_frame_count = 0;
    m_initialized = false;
}

} // namespace goggles::render
