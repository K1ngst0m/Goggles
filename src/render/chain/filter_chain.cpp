#include "filter_chain.hpp"

#include <cmath>
#include <render/backend/vulkan_error.hpp>
#include <render/shader/retroarch_preprocessor.hpp>
#include <util/logging.hpp>

namespace goggles::render {

FilterChain::~FilterChain() {
    shutdown();
}

auto FilterChain::init(vk::Device device, vk::PhysicalDevice physical_device,
                       vk::Format swapchain_format, uint32_t num_sync_indices,
                       ShaderRuntime& shader_runtime, const std::filesystem::path& shader_dir)
    -> Result<void> {
    if (m_initialized) {
        return {};
    }

    m_device = device;
    m_physical_device = physical_device;
    m_swapchain_format = swapchain_format;
    m_num_sync_indices = num_sync_indices;
    m_shader_runtime = &shader_runtime;
    m_shader_dir = shader_dir;

    GOGGLES_TRY(
        m_output_pass.init(device, swapchain_format, num_sync_indices, shader_runtime, shader_dir));

    m_initialized = true;
    GOGGLES_LOG_DEBUG("FilterChain initialized (passthrough mode)");
    return {};
}

auto FilterChain::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    if (!m_initialized) {
        return make_error<void>(ErrorCode::VULKAN_INIT_FAILED, "FilterChain not initialized");
    }

    PresetParser parser;
    m_preset = GOGGLES_TRY(parser.load(preset_path));

    m_passes.clear();
    m_framebuffers.clear();

    RetroArchPreprocessor preprocessor;

    for (const auto& pass_config : m_preset->passes) {
        auto preprocessed = GOGGLES_TRY(preprocessor.preprocess(pass_config.shader_path));
        auto pass = std::make_unique<FilterPass>();

        GOGGLES_TRY(pass->init_from_sources(
            m_device, m_physical_device, pass_config.framebuffer_format, m_num_sync_indices,
            *m_shader_runtime, preprocessed.vertex_source, preprocessed.fragment_source,
            pass_config.shader_path.stem().string(), pass_config.filter_mode,
            preprocessed.parameters));

        m_passes.push_back(std::move(pass));
    }

    m_framebuffers.resize(m_passes.size());

    GOGGLES_LOG_INFO("FilterChain loaded preset: {} ({} passes)", preset_path.filename().string(),
                     m_passes.size());
    return {};
}

void FilterChain::record(vk::CommandBuffer cmd, vk::ImageView original_view,
                         vk::Extent2D original_extent, vk::ImageView swapchain_view,
                         vk::Extent2D viewport_extent, uint32_t frame_index, ScaleMode scale_mode,
                         uint32_t integer_scale) {
    m_last_scale_mode = scale_mode;
    m_last_integer_scale = integer_scale;
    m_last_source_extent = original_extent;

    if (m_passes.empty()) {
        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = viewport_extent;
        ctx.source_extent = original_extent;
        ctx.target_image_view = swapchain_view;
        ctx.target_format = m_swapchain_format;
        ctx.source_texture = original_view;
        ctx.original_texture = original_view;
        ctx.scale_mode = scale_mode;
        ctx.integer_scale = integer_scale;

        m_output_pass.record(cmd, ctx);
        m_frame_count++;
        return;
    }

    auto vp =
        calculate_viewport(original_extent.width, original_extent.height, viewport_extent.width,
                           viewport_extent.height, scale_mode, integer_scale);

    GOGGLES_MUST(ensure_framebuffers({.viewport = viewport_extent, .source = original_extent}, {vp.width, vp.height}));

    vk::ImageView source_view = original_view;
    vk::Extent2D source_extent = original_extent;

    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& pass = m_passes[i];

        vk::ImageView target_view = m_framebuffers[i].view();
        vk::Extent2D target_extent = m_framebuffers[i].extent();
        vk::Format target_format = m_framebuffers[i].format();

        vk::ImageMemoryBarrier pre_barrier{};
        pre_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        pre_barrier.oldLayout = vk::ImageLayout::eUndefined;
        pre_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_barrier.image = m_framebuffers[i].image();
        pre_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        pre_barrier.subresourceRange.baseMipLevel = 0;
        pre_barrier.subresourceRange.levelCount = 1;
        pre_barrier.subresourceRange.baseArrayLayer = 0;
        pre_barrier.subresourceRange.layerCount = 1;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                            vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                            pre_barrier);

        pass->set_source_size(source_extent.width, source_extent.height);
        pass->set_output_size(target_extent.width, target_extent.height);
        pass->set_original_size(original_extent.width, original_extent.height);
        pass->set_frame_count(m_frame_count);
        pass->set_final_viewport_size(vp.width, vp.height);

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = target_extent;
        ctx.source_extent = source_extent;
        ctx.target_image_view = target_view;
        ctx.target_format = target_format;
        ctx.source_texture = source_view;
        ctx.original_texture = original_view;
        ctx.scale_mode = scale_mode;
        ctx.integer_scale = integer_scale;

        pass->record(cmd, ctx);

        vk::ImageMemoryBarrier barrier{};
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_framebuffers[i].image();
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        source_view = m_framebuffers[i].view();
        source_extent = m_framebuffers[i].extent();
    }

    PassContext output_ctx{};
    output_ctx.frame_index = frame_index;
    output_ctx.output_extent = viewport_extent;
    output_ctx.source_extent = original_extent;
    output_ctx.target_image_view = swapchain_view;
    output_ctx.target_format = m_swapchain_format;
    output_ctx.source_texture = source_view;
    output_ctx.original_texture = original_view;
    output_ctx.scale_mode = scale_mode;
    output_ctx.integer_scale = integer_scale;

    m_output_pass.record(cmd, output_ctx);

    m_frame_count++;
}

auto FilterChain::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
    GOGGLES_LOG_DEBUG("FilterChain::handle_resize called: {}x{}", new_viewport_extent.width,
                      new_viewport_extent.height);

    if (!m_preset || m_framebuffers.empty()) {
        GOGGLES_LOG_DEBUG("handle_resize: no preset or framebuffers");
        return {};
    }

    auto vp = calculate_viewport(m_last_source_extent.width, m_last_source_extent.height,
                                 new_viewport_extent.width, new_viewport_extent.height,
                                 m_last_scale_mode, m_last_integer_scale);

    for (size_t i = 0; i < m_framebuffers.size(); ++i) {
        const auto& pass_config = m_preset->passes[i];
        if (pass_config.scale_type_x == ScaleType::VIEWPORT ||
            pass_config.scale_type_y == ScaleType::VIEWPORT) {
            vk::Extent2D prev_extent =
                (i == 0) ? m_last_source_extent : m_framebuffers[i - 1].extent();
            auto new_size =
                calculate_pass_output_size(pass_config, prev_extent, {vp.width, vp.height});

            GOGGLES_LOG_DEBUG("handle_resize: fb[{}] current={}x{}, new={}x{}", i,
                              m_framebuffers[i].extent().width, m_framebuffers[i].extent().height,
                              new_size.width, new_size.height);

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

auto FilterChain::ensure_framebuffers(const FramebufferExtents& extents,
                                      vk::Extent2D viewport_extent) -> Result<void> {
    if (!m_preset) {
        return {};
    }

    vk::Extent2D prev_extent = extents.source;

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

} // namespace goggles::render
