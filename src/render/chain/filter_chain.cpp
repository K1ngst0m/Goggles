#include "filter_chain.hpp"

#include <render/backend/vulkan_error.hpp>
#include <render/shader/retroarch_preprocessor.hpp>
#include <util/logging.hpp>

#include <cmath>

namespace goggles::render {

FilterChain::~FilterChain() {
    shutdown();
}

auto FilterChain::init(vk::Device device, vk::PhysicalDevice physical_device,
                       vk::Format swapchain_format, uint32_t num_sync_indices,
                       ShaderRuntime& shader_runtime,
                       const std::filesystem::path& shader_dir) -> Result<void> {
    if (m_initialized) {
        return {};
    }

    m_device = device;
    m_physical_device = physical_device;
    m_swapchain_format = swapchain_format;
    m_num_sync_indices = num_sync_indices;
    m_shader_runtime = &shader_runtime;
    m_shader_dir = shader_dir;

    GOGGLES_TRY(m_output_pass.init(device, swapchain_format, num_sync_indices, shader_runtime,
                                   shader_dir));

    m_initialized = true;
    GOGGLES_LOG_DEBUG("FilterChain initialized (passthrough mode)");
    return {};
}

auto FilterChain::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    if (!m_initialized) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "FilterChain not initialized");
    }

    PresetParser parser;
    auto preset_result = parser.load(preset_path);
    if (!preset_result) {
        return make_error<void>(preset_result.error().code, preset_result.error().message);
    }

    m_passes.clear();
    m_framebuffers.clear();
    m_preset = std::move(preset_result.value());

    RetroArchPreprocessor preprocessor;

    for (size_t i = 0; i < m_preset->passes.size(); ++i) {
        const auto& pass_config = m_preset->passes[i];

        auto preprocess_result = preprocessor.preprocess(pass_config.shader_path);
        if (!preprocess_result) {
            return make_error<void>(preprocess_result.error().code,
                                    preprocess_result.error().message);
        }

        auto& preprocessed = preprocess_result.value();
        auto pass = std::make_unique<FilterPass>();

        bool is_final = (i == m_preset->passes.size() - 1);
        vk::Format target_format = is_final ? m_swapchain_format : pass_config.framebuffer_format;

        auto init_result = pass->init_from_sources(
            m_device, m_physical_device, target_format, m_num_sync_indices,
            *m_shader_runtime, preprocessed.vertex_source, preprocessed.fragment_source,
            pass_config.shader_path.stem().string(), pass_config.filter_mode,
            preprocessed.parameters);

        if (!init_result) {
            return make_error<void>(init_result.error().code, init_result.error().message);
        }

        m_passes.push_back(std::move(pass));
    }

    m_framebuffers.resize(m_passes.empty() ? 0 : m_passes.size() - 1);

    GOGGLES_LOG_INFO("FilterChain loaded preset: {} ({} passes)", preset_path.filename().string(),
                     m_passes.size());
    return {};
}

void FilterChain::record(vk::CommandBuffer cmd, vk::ImageView original_view,
                         vk::Extent2D original_extent, vk::ImageView swapchain_view,
                         vk::Extent2D viewport_extent, uint32_t frame_index) {
    if (m_passes.empty()) {
        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = viewport_extent;
        ctx.target_image_view = swapchain_view;
        ctx.target_format = m_swapchain_format;
        ctx.source_texture = original_view;
        ctx.original_texture = original_view;

        m_output_pass.record(cmd, ctx);
        m_frame_count++;
        return;
    }

    auto fb_result = ensure_framebuffers(viewport_extent, original_extent);
    if (!fb_result) {
        GOGGLES_LOG_ERROR("Failed to ensure framebuffers: {}", fb_result.error().message);
        return;
    }

    vk::ImageView source_view = original_view;
    vk::Extent2D source_extent = original_extent;

    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& pass = m_passes[i];
        bool is_final = (i == m_passes.size() - 1);

        vk::ImageView target_view;
        vk::Extent2D target_extent;
        vk::Format target_format;

        if (is_final) {
            target_view = swapchain_view;
            target_extent = viewport_extent;
            target_format = m_swapchain_format;
        } else {
            target_view = m_framebuffers[i].view();
            target_extent = m_framebuffers[i].extent();
            target_format = m_framebuffers[i].format();
        }

        pass->set_source_size(source_extent.width, source_extent.height);
        pass->set_output_size(target_extent.width, target_extent.height);
        pass->set_original_size(original_extent.width, original_extent.height);
        pass->set_frame_count(m_frame_count);

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = target_extent;
        ctx.target_image_view = target_view;
        ctx.target_format = target_format;
        ctx.source_texture = source_view;
        ctx.original_texture = original_view;

        pass->record(cmd, ctx);

        if (!is_final) {
            record_image_barrier(cmd, m_framebuffers[i].image(),
                                 vk::ImageLayout::eColorAttachmentOptimal,
                                 vk::ImageLayout::eShaderReadOnlyOptimal,
                                 vk::AccessFlagBits::eColorAttachmentWrite,
                                 vk::AccessFlagBits::eShaderRead,
                                 vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                 vk::PipelineStageFlagBits::eFragmentShader);

            source_view = m_framebuffers[i].view();
            source_extent = m_framebuffers[i].extent();
        }
    }

    m_frame_count++;
}

auto FilterChain::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
    if (!m_preset || m_framebuffers.empty()) {
        return {};
    }

    for (size_t i = 0; i < m_framebuffers.size(); ++i) {
        const auto& pass_config = m_preset->passes[i];
        if (pass_config.scale_type_x == ScaleType::VIEWPORT ||
            pass_config.scale_type_y == ScaleType::VIEWPORT) {
            vk::Extent2D prev_extent =
                (i == 0) ? new_viewport_extent : m_framebuffers[i - 1].extent();
            auto new_size = calculate_pass_output_size(pass_config, prev_extent, new_viewport_extent);

            if (m_framebuffers[i].extent() != new_size) {
                GOGGLES_TRY(m_framebuffers[i].resize(new_size));
            }
        }
    }
    return {};
}

void FilterChain::shutdown() {
    m_passes.clear();
    m_framebuffers.clear();
    m_output_pass.shutdown();
    m_preset.reset();
    m_frame_count = 0;
    m_initialized = false;
}

auto FilterChain::ensure_framebuffers(vk::Extent2D viewport_extent,
                                      vk::Extent2D source_extent) -> Result<void> {
    if (!m_preset) {
        return {};
    }

    vk::Extent2D prev_extent = source_extent;

    for (size_t i = 0; i < m_framebuffers.size(); ++i) {
        const auto& pass_config = m_preset->passes[i];
        auto target_extent = calculate_pass_output_size(pass_config, prev_extent, viewport_extent);

        if (!m_framebuffers[i].is_initialized()) {
            GOGGLES_TRY(m_framebuffers[i].init(m_device, m_physical_device,
                                              pass_config.framebuffer_format, target_extent));
        } else if (m_framebuffers[i].extent() != target_extent) {
            GOGGLES_TRY(m_framebuffers[i].resize(target_extent));
        }

        prev_extent = target_extent;
    }
    return {};
}

auto FilterChain::calculate_pass_output_size(const ShaderPassConfig& pass_config,
                                             vk::Extent2D source_extent,
                                             vk::Extent2D viewport_extent) -> vk::Extent2D {
    uint32_t width = 0;
    uint32_t height = 0;

    switch (pass_config.scale_type_x) {
    case ScaleType::SOURCE:
        width = static_cast<uint32_t>(
            std::round(static_cast<float>(source_extent.width) * pass_config.scale_x));
        break;
    case ScaleType::VIEWPORT:
        width = static_cast<uint32_t>(
            std::round(static_cast<float>(viewport_extent.width) * pass_config.scale_x));
        break;
    case ScaleType::ABSOLUTE:
        width = static_cast<uint32_t>(pass_config.scale_x);
        break;
    }

    switch (pass_config.scale_type_y) {
    case ScaleType::SOURCE:
        height = static_cast<uint32_t>(
            std::round(static_cast<float>(source_extent.height) * pass_config.scale_y));
        break;
    case ScaleType::VIEWPORT:
        height = static_cast<uint32_t>(
            std::round(static_cast<float>(viewport_extent.height) * pass_config.scale_y));
        break;
    case ScaleType::ABSOLUTE:
        height = static_cast<uint32_t>(pass_config.scale_y);
        break;
    }

    return vk::Extent2D{std::max(1U, width), std::max(1U, height)};
}

void FilterChain::record_image_barrier(vk::CommandBuffer cmd, vk::Image image,
                                       vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                                       vk::AccessFlags src_access, vk::AccessFlags dst_access,
                                       vk::PipelineStageFlags src_stage,
                                       vk::PipelineStageFlags dst_stage) {
    vk::ImageMemoryBarrier barrier{};
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);
}

} // namespace goggles::render
