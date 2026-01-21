#include "filter_chain.hpp"

#include "downsample_pass.hpp"

#include <charconv>
#include <cmath>
#include <render/backend/vulkan_error.hpp>
#include <render/shader/retroarch_preprocessor.hpp>
#include <unordered_set>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

auto parse_original_history_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "OriginalHistory";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    uint32_t index = 0;
    const auto* end = suffix.data() + suffix.size();
    auto [ptr, ec] = std::from_chars(suffix.data(), end, index);
    if (ptr != end) {
        return std::nullopt;
    }
    return index;
}

constexpr std::string_view FEEDBACK_SUFFIX = "Feedback";

auto parse_feedback_alias(std::string_view name) -> std::optional<std::string> {
    if (!name.ends_with(FEEDBACK_SUFFIX)) {
        return std::nullopt;
    }
    auto alias = name.substr(0, name.size() - FEEDBACK_SUFFIX.size());
    if (alias.empty()) {
        return std::nullopt;
    }
    return std::string(alias);
}

auto parse_pass_feedback_index(std::string_view name) -> std::optional<size_t> {
    constexpr std::string_view PREFIX = "PassFeedback";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    size_t index = 0;
    const auto* end = suffix.data() + suffix.size();
    auto [ptr, ec] = std::from_chars(suffix.data(), end, index);
    if (ptr != end) {
        return std::nullopt;
    }
    return index;
}

} // namespace

FilterChain::~FilterChain() {
    shutdown();
}

auto FilterChain::create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                         uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                         const std::filesystem::path& shader_dir, vk::Extent2D source_resolution)
    -> ResultPtr<FilterChain> {
    GOGGLES_PROFILE_FUNCTION();

    auto chain = std::unique_ptr<FilterChain>(new FilterChain());

    chain->m_vk_ctx = vk_ctx;
    chain->m_swapchain_format = swapchain_format;
    chain->m_num_sync_indices = num_sync_indices;
    chain->m_shader_runtime = &shader_runtime;
    chain->m_shader_dir = shader_dir;
    chain->m_source_resolution = source_resolution;

    OutputPassConfig output_config{
        .target_format = swapchain_format,
        .num_sync_indices = num_sync_indices,
        .shader_dir = shader_dir,
    };
    chain->m_output_pass = GOGGLES_TRY(OutputPass::create(vk_ctx, shader_runtime, output_config));

    chain->m_texture_loader = std::make_unique<TextureLoader>(
        vk_ctx.device, vk_ctx.physical_device, vk_ctx.command_pool, vk_ctx.graphics_queue);

    if (source_resolution.width > 0 && source_resolution.height > 0) {
        DownsamplePassConfig downsample_config{
            .target_format = vk::Format::eR8G8B8A8Unorm,
            .num_sync_indices = num_sync_indices,
            .shader_dir = shader_dir,
        };
        chain->m_prechain_passes.push_back(
            GOGGLES_TRY(DownsamplePass::create(vk_ctx, shader_runtime, downsample_config)));

        chain->m_prechain_framebuffers.push_back(GOGGLES_TRY(Framebuffer::create(
            vk_ctx.device, vk_ctx.physical_device, vk::Format::eR8G8B8A8Unorm, source_resolution)));

        GOGGLES_LOG_INFO("FilterChain pre-chain enabled: {}x{}", source_resolution.width,
                         source_resolution.height);
    } else if (source_resolution.width > 0 || source_resolution.height > 0) {
        GOGGLES_LOG_INFO("FilterChain pre-chain pending: width={}, height={}",
                         source_resolution.width, source_resolution.height);
    }

    GOGGLES_LOG_DEBUG("FilterChain initialized (passthrough mode)");
    return make_result_ptr(std::move(chain));
}

auto FilterChain::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    PresetParser parser;
    auto preset_result = parser.load(preset_path);
    if (!preset_result) {
        return make_error<void>(preset_result.error().code, preset_result.error().message);
    }

    std::vector<std::unique_ptr<FilterPass>> new_passes;
    std::unordered_map<std::string, size_t> new_alias_map;
    RetroArchPreprocessor preprocessor;

    for (size_t i = 0; i < preset_result->passes.size(); ++i) {
        const auto& pass_config = preset_result->passes[i];
        auto preprocessed = preprocessor.preprocess(pass_config.shader_path);
        if (!preprocessed) {
            return make_error<void>(preprocessed.error().code, preprocessed.error().message);
        }

        FilterPassConfig config{
            .target_format = pass_config.framebuffer_format,
            .num_sync_indices = m_num_sync_indices,
            .vertex_source = preprocessed->vertex_source,
            .fragment_source = preprocessed->fragment_source,
            .shader_name = pass_config.shader_path.stem().string(),
            .filter_mode = pass_config.filter_mode,
            .mipmap = pass_config.mipmap,
            .wrap_mode = pass_config.wrap_mode,
            .parameters = preprocessed->parameters,
        };
        auto pass = GOGGLES_TRY(FilterPass::create(m_vk_ctx, *m_shader_runtime, config));

        for (const auto& override : preset_result->parameters) {
            pass->set_parameter_override(override.name, override.value);
        }
        auto ubo_result = pass->update_ubo_parameters();
        if (!ubo_result) {
            return make_error<void>(ubo_result.error().code, ubo_result.error().message);
        }

        new_passes.push_back(std::move(pass));

        if (pass_config.alias.has_value()) {
            new_alias_map[*pass_config.alias] = i;
        }
    }

    m_preset = std::move(*preset_result);
    m_passes = std::move(new_passes);
    m_alias_to_pass_index = std::move(new_alias_map);
    m_framebuffers.clear();
    m_framebuffers.resize(m_passes.size());
    m_texture_registry.clear();

    // Reset frame history when switching presets (new preset may need different depth)
    m_frame_history.shutdown();

    // Detect required frame history depth and feedback passes from shader texture bindings
    m_required_history_depth = 0;
    m_feedback_framebuffers.clear();
    std::unordered_set<size_t> feedback_pass_indices;
    for (const auto& pass : m_passes) {
        for (const auto& tex : pass->texture_bindings()) {
            if (auto idx = parse_original_history_index(tex.name)) {
                m_required_history_depth = std::max(m_required_history_depth, *idx + 1);
            }
            if (auto alias = parse_feedback_alias(tex.name)) {
                if (auto it = m_alias_to_pass_index.find(*alias);
                    it != m_alias_to_pass_index.end()) {
                    feedback_pass_indices.insert(it->second);
                    GOGGLES_LOG_DEBUG("Detected feedback texture '{}' -> pass {} (alias '{}')",
                                      tex.name, it->second, *alias);
                }
            }
            if (auto fb_idx = parse_pass_feedback_index(tex.name)) {
                if (*fb_idx < m_passes.size()) {
                    feedback_pass_indices.insert(*fb_idx);
                    GOGGLES_LOG_DEBUG("Detected PassFeedback{} texture", *fb_idx);
                }
            }
        }
    }
    if (m_required_history_depth > 0) {
        m_required_history_depth = std::min(m_required_history_depth, FrameHistory::MAX_HISTORY);
        GOGGLES_LOG_DEBUG("Detected OriginalHistory usage, depth={}", m_required_history_depth);
    }
    for (auto pass_idx : feedback_pass_indices) {
        m_feedback_framebuffers.try_emplace(pass_idx);
    }

    GOGGLES_TRY(load_preset_textures());

    GOGGLES_LOG_INFO(
        "FilterChain loaded preset: {} ({} passes, {} textures, {} aliases, {} params)",
        preset_path.filename().string(), m_passes.size(), m_texture_registry.size(),
        m_alias_to_pass_index.size(), m_preset.parameters.size());
    for (const auto& [alias, pass_idx] : m_alias_to_pass_index) {
        GOGGLES_LOG_DEBUG("  Alias '{}' -> pass {}", alias, pass_idx);
    }
    return {};
}

void FilterChain::bind_pass_textures(FilterPass& pass, size_t pass_index,
                                     vk::ImageView original_view, vk::Extent2D original_extent,
                                     vk::ImageView source_view) {
    pass.clear_alias_sizes();
    pass.clear_texture_bindings();

    pass.set_texture_binding("Source", source_view, nullptr);
    pass.set_texture_binding("Original", original_view, nullptr);

    pass.set_texture_binding("OriginalHistory0", original_view, nullptr);
    pass.set_alias_size("OriginalHistory0", original_extent.width, original_extent.height);

    for (uint32_t h = 0; h < m_required_history_depth; ++h) {
        auto name = std::format("OriginalHistory{}", h + 1);
        if (auto hist_view = m_frame_history.get(h)) {
            pass.set_texture_binding(name, hist_view, nullptr);
            auto ext = m_frame_history.get_extent(h);
            pass.set_alias_size(name, ext.width, ext.height);
        } else {
            // Fallback to original when history not yet available
            pass.set_texture_binding(name, original_view, nullptr);
            pass.set_alias_size(name, original_extent.width, original_extent.height);
        }
    }

    for (size_t p = 0; p < pass_index; ++p) {
        if (m_framebuffers[p]) {
            auto pass_name = std::format("PassOutput{}", p);
            auto pass_extent = m_framebuffers[p]->extent();
            pass.set_texture_binding(pass_name, m_framebuffers[p]->view(), nullptr);
            pass.set_alias_size(pass_name, pass_extent.width, pass_extent.height);
        }
    }

    for (const auto& [fb_idx, feedback_fb] : m_feedback_framebuffers) {
        auto feedback_name = std::format("PassFeedback{}", fb_idx);
        if (feedback_fb) {
            pass.set_texture_binding(feedback_name, feedback_fb->view(), nullptr);
            auto fb_extent = feedback_fb->extent();
            pass.set_alias_size(feedback_name, fb_extent.width, fb_extent.height);
        } else {
            // Fallback to source when feedback not yet available
            pass.set_texture_binding(feedback_name, source_view, nullptr);
        }
    }

    for (const auto& [alias, idx] : m_alias_to_pass_index) {
        if (idx < pass_index && m_framebuffers[idx]) {
            pass.set_texture_binding(alias, m_framebuffers[idx]->view(), nullptr);
            auto alias_extent = m_framebuffers[idx]->extent();
            pass.set_alias_size(alias, alias_extent.width, alias_extent.height);
        }
        if (auto fb_it = m_feedback_framebuffers.find(idx);
            fb_it != m_feedback_framebuffers.end() && fb_it->second) {
            auto feedback_name = alias + std::string(FEEDBACK_SUFFIX);
            pass.set_texture_binding(feedback_name, fb_it->second->view(), nullptr);
            auto fb_extent = fb_it->second->extent();
            pass.set_alias_size(feedback_name, fb_extent.width, fb_extent.height);
        }
    }

    for (const auto& [name, tex] : m_texture_registry) {
        pass.set_texture_binding(name, *tex.data.view, *tex.sampler);
    }
}

void FilterChain::copy_feedback_framebuffers(vk::CommandBuffer cmd) {
    for (auto& [pass_idx, feedback_fb] : m_feedback_framebuffers) {
        if (!feedback_fb || !m_framebuffers[pass_idx]) {
            continue;
        }
        auto extent = m_framebuffers[pass_idx]->extent();
        vk::ImageSubresourceLayers layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        vk::ImageCopy region{
            layers, {0, 0, 0}, layers, {0, 0, 0}, {extent.width, extent.height, 1}};

        std::array<vk::ImageMemoryBarrier, 2> pre{};
        pre[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre[0].dstAccessMask = vk::AccessFlagBits::eTransferRead;
        pre[0].oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        pre[0].newLayout = vk::ImageLayout::eTransferSrcOptimal;
        pre[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[0].image = m_framebuffers[pass_idx]->image();
        pre[0].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        pre[1].srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre[1].dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        pre[1].oldLayout = vk::ImageLayout::eUndefined;
        pre[1].newLayout = vk::ImageLayout::eTransferDstOptimal;
        pre[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[1].image = feedback_fb->image();
        pre[1].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, pre);

        cmd.copyImage(m_framebuffers[pass_idx]->image(), vk::ImageLayout::eTransferSrcOptimal,
                      feedback_fb->image(), vk::ImageLayout::eTransferDstOptimal, region);

        std::array<vk::ImageMemoryBarrier, 2> post{};
        post[0].srcAccessMask = vk::AccessFlagBits::eTransferRead;
        post[0].dstAccessMask = vk::AccessFlagBits::eShaderRead;
        post[0].oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        post[0].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        post[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[0].image = m_framebuffers[pass_idx]->image();
        post[0].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        post[1].srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        post[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
        post[1].oldLayout = vk::ImageLayout::eTransferDstOptimal;
        post[1].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        post[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[1].image = feedback_fb->image();
        post[1].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, post);
    }
}

auto FilterChain::record_prechain(vk::CommandBuffer cmd, vk::ImageView original_view,
                                  vk::Extent2D original_extent, uint32_t frame_index)
    -> PreChainResult {
    if (m_prechain_passes.empty() || m_prechain_framebuffers.empty()) {
        return {.view = original_view, .extent = original_extent};
    }

    vk::ImageView current_view = original_view;
    vk::Extent2D current_extent = original_extent;

    for (size_t i = 0; i < m_prechain_passes.size(); ++i) {
        auto& pass = m_prechain_passes[i];
        auto& framebuffer = m_prechain_framebuffers[i];
        auto output_extent = framebuffer->extent();

        vk::ImageMemoryBarrier pre_barrier{};
        pre_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        pre_barrier.oldLayout = vk::ImageLayout::eUndefined;
        pre_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_barrier.image = framebuffer->image();
        pre_barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                            vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                            pre_barrier);

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.source_extent = current_extent;
        ctx.output_extent = output_extent;
        ctx.target_image_view = framebuffer->view();
        ctx.target_format = framebuffer->format();
        ctx.source_texture = current_view;
        ctx.original_texture = original_view;
        ctx.scale_mode = ScaleMode::stretch;
        ctx.integer_scale = 0;

        pass->record(cmd, ctx);

        vk::ImageMemoryBarrier post_barrier{};
        post_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        post_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        post_barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        post_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_barrier.image = framebuffer->image();
        post_barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, post_barrier);

        GOGGLES_LOG_TRACE("Pre-chain pass {}: {}x{} -> {}x{}", i, current_extent.width,
                          current_extent.height, output_extent.width, output_extent.height);

        current_view = framebuffer->view();
        current_extent = output_extent;
    }

    return {.view = current_view, .extent = current_extent};
}

void FilterChain::record(vk::CommandBuffer cmd, vk::Image original_image,
                         vk::ImageView original_view, vk::Extent2D original_extent,
                         vk::ImageView swapchain_view, vk::Extent2D viewport_extent,
                         uint32_t frame_index, ScaleMode scale_mode, uint32_t integer_scale) {
    GOGGLES_PROFILE_FUNCTION();

    m_last_scale_mode = scale_mode;
    m_last_integer_scale = integer_scale;
    m_last_source_extent = original_extent;

    GOGGLES_MUST(ensure_prechain_passes(original_extent));

    auto [effective_original_view, effective_original_extent] =
        record_prechain(cmd, original_view, original_extent, frame_index);

    GOGGLES_MUST(ensure_frame_history(effective_original_extent));

    if (m_passes.empty() || m_bypass_enabled.load(std::memory_order_relaxed)) {
        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = viewport_extent;
        ctx.source_extent = effective_original_extent;
        ctx.target_image_view = swapchain_view;
        ctx.target_format = m_swapchain_format;
        ctx.source_texture = effective_original_view;
        ctx.original_texture = effective_original_view;
        ctx.scale_mode = scale_mode;
        ctx.integer_scale = integer_scale;

        m_output_pass->record(cmd, ctx);
        m_frame_count++;
        return;
    }

    auto vp = calculate_viewport(effective_original_extent.width, effective_original_extent.height,
                                 viewport_extent.width, viewport_extent.height, scale_mode,
                                 integer_scale);
    GOGGLES_MUST(ensure_framebuffers(
        {.viewport = viewport_extent, .source = effective_original_extent}, {vp.width, vp.height}));

    for (auto& [pass_idx, feedback_fb] : m_feedback_framebuffers) {
        if (feedback_fb && m_frame_count == 0) {
            vk::ImageMemoryBarrier init_barrier{};
            init_barrier.srcAccessMask = {};
            init_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            init_barrier.oldLayout = vk::ImageLayout::eUndefined;
            init_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            init_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            init_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            init_barrier.image = feedback_fb->image();
            init_barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
                                init_barrier);
        }
    }

    vk::ImageView source_view = effective_original_view;
    vk::Extent2D source_extent = effective_original_extent;

    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& pass = m_passes[i];

        vk::ImageView target_view = m_framebuffers[i]->view();
        vk::Extent2D target_extent = m_framebuffers[i]->extent();
        vk::Format target_format = m_framebuffers[i]->format();

        vk::ImageMemoryBarrier pre_barrier{};
        pre_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        pre_barrier.oldLayout = vk::ImageLayout::eUndefined;
        pre_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_barrier.image = m_framebuffers[i]->image();
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
        pass->set_original_size(effective_original_extent.width, effective_original_extent.height);
        pass->set_frame_count(m_frame_count, m_preset.passes[i].frame_count_mod);
        pass->set_final_viewport_size(vp.width, vp.height);
        pass->set_rotation(0);

        bind_pass_textures(*pass, i, effective_original_view, effective_original_extent,
                           source_view);

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = target_extent;
        ctx.source_extent = source_extent;
        ctx.target_image_view = target_view;
        ctx.target_format = target_format;
        ctx.source_texture = source_view;
        ctx.original_texture = effective_original_view;
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
        barrier.image = m_framebuffers[i]->image();
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        source_view = m_framebuffers[i]->view();
        source_extent = m_framebuffers[i]->extent();
    }

    PassContext output_ctx{};
    output_ctx.frame_index = frame_index;
    output_ctx.output_extent = viewport_extent;
    output_ctx.source_extent = effective_original_extent;
    output_ctx.target_image_view = swapchain_view;
    output_ctx.target_format = m_swapchain_format;
    output_ctx.source_texture = source_view;
    output_ctx.original_texture = effective_original_view;
    output_ctx.scale_mode = scale_mode;
    output_ctx.integer_scale = integer_scale;

    m_output_pass->record(cmd, output_ctx);

    if (m_frame_history.is_initialized()) {
        auto history_image = !m_prechain_framebuffers.empty()
                                 ? m_prechain_framebuffers.back()->image()
                                 : original_image;
        m_frame_history.push(cmd, history_image, effective_original_extent);
    }

    copy_feedback_framebuffers(cmd);
    m_frame_count++;
}

auto FilterChain::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    GOGGLES_LOG_DEBUG("FilterChain::handle_resize called: {}x{}", new_viewport_extent.width,
                      new_viewport_extent.height);

    if (m_preset.passes.empty() || m_framebuffers.empty()) {
        GOGGLES_LOG_DEBUG("handle_resize: no preset or framebuffers");
        return {};
    }

    if (m_last_source_extent.width == 0 || m_last_source_extent.height == 0) {
        GOGGLES_LOG_DEBUG("handle_resize: no source rendered yet, skipping");
        return {};
    }

    auto vp = calculate_viewport(m_last_source_extent.width, m_last_source_extent.height,
                                 new_viewport_extent.width, new_viewport_extent.height,
                                 m_last_scale_mode, m_last_integer_scale);

    for (size_t i = 0; i < m_framebuffers.size(); ++i) {
        const auto& pass_config = m_preset.passes[i];
        if (pass_config.scale_type_x == ScaleType::viewport ||
            pass_config.scale_type_y == ScaleType::viewport) {
            vk::Extent2D prev_extent =
                (i == 0) ? m_last_source_extent : m_framebuffers[i - 1]->extent();
            auto new_size =
                calculate_pass_output_size(pass_config, prev_extent, {vp.width, vp.height});

            GOGGLES_LOG_DEBUG("handle_resize: fb[{}] current={}x{}, new={}x{}", i,
                              m_framebuffers[i]->extent().width, m_framebuffers[i]->extent().height,
                              new_size.width, new_size.height);

            if (m_framebuffers[i]->extent() != new_size) {
                GOGGLES_TRY(m_framebuffers[i]->resize(new_size));
            }
        }
    }
    return {};
}

void FilterChain::shutdown() {
    m_passes.clear();
    m_framebuffers.clear();
    m_feedback_framebuffers.clear();
    m_texture_registry.clear();
    m_alias_to_pass_index.clear();
    m_frame_history.shutdown();
    m_output_pass->shutdown();
    m_preset = PresetConfig{};
    m_frame_count = 0;
    m_required_history_depth = 0;

    // Cleanup pre-chain resources
    for (auto& pass : m_prechain_passes) {
        pass->shutdown();
    }
    m_prechain_passes.clear();
    m_prechain_framebuffers.clear();
}

auto FilterChain::get_all_parameters() const -> std::vector<ParameterInfo> {
    std::unordered_map<std::string, ParameterInfo> param_map;

    for (const auto& pass : m_passes) {
        for (const auto& param : pass->parameters()) {
            if (param_map.contains(param.name)) {
                continue;
            }
            param_map[param.name] = {
                .name = param.name,
                .description = param.description,
                .current_value = pass->get_parameter_value(param.name),
                .default_value = param.default_value,
                .min_value = param.min_value,
                .max_value = param.max_value,
                .step = param.step,
            };
        }
    }

    std::vector<ParameterInfo> result;
    result.reserve(param_map.size());
    for (auto& [_, info] : param_map) {
        result.push_back(std::move(info));
    }
    return result;
}

void FilterChain::set_parameter(const std::string& name, float value) {
    bool found = false;
    for (auto& pass : m_passes) {
        for (const auto& param : pass->parameters()) {
            if (param.name == name) {
                found = true;
                break;
            }
        }
        pass->set_parameter_override(name, value);
        GOGGLES_MUST(pass->update_ubo_parameters());
    }
    if (!found) {
        GOGGLES_LOG_WARN("set_parameter: '{}' not found in any pass", name);
    }
}

void FilterChain::reset_parameter(const std::string& name) {
    for (auto& pass : m_passes) {
        for (const auto& param : pass->parameters()) {
            if (param.name == name) {
                pass->set_parameter_override(name, param.default_value);
                GOGGLES_MUST(pass->update_ubo_parameters());
                break;
            }
        }
    }
}

void FilterChain::clear_parameter_overrides() {
    for (auto& pass : m_passes) {
        pass->clear_parameter_overrides();
        GOGGLES_MUST(pass->update_ubo_parameters());
    }
}

auto FilterChain::ensure_framebuffers(const FramebufferExtents& extents,
                                      vk::Extent2D viewport_extent) -> Result<void> {
    GOGGLES_PROFILE_SCOPE("EnsureFramebuffers");

    if (m_preset.passes.empty()) {
        return {};
    }

    vk::Extent2D prev_extent = extents.source;

    for (size_t i = 0; i < m_framebuffers.size(); ++i) {
        const auto& pass_config = m_preset.passes[i];
        auto target_extent = calculate_pass_output_size(pass_config, prev_extent, viewport_extent);

        if (!m_framebuffers[i]) {
            m_framebuffers[i] =
                GOGGLES_TRY(Framebuffer::create(m_vk_ctx.device, m_vk_ctx.physical_device,
                                                pass_config.framebuffer_format, target_extent));
        } else if (m_framebuffers[i]->extent() != target_extent) {
            GOGGLES_TRY(m_framebuffers[i]->resize(target_extent));
        }

        if (auto it = m_feedback_framebuffers.find(i); it != m_feedback_framebuffers.end()) {
            if (!it->second) {
                it->second =
                    GOGGLES_TRY(Framebuffer::create(m_vk_ctx.device, m_vk_ctx.physical_device,
                                                    pass_config.framebuffer_format, target_extent));
                GOGGLES_LOG_DEBUG("Created feedback framebuffer for pass {}", i);
            } else if (it->second->extent() != target_extent) {
                GOGGLES_TRY(it->second->resize(target_extent));
            }
        }

        prev_extent = target_extent;
    }
    return {};
}

auto FilterChain::ensure_frame_history(vk::Extent2D extent) -> Result<void> {
    if (m_required_history_depth > 0 && !m_frame_history.is_initialized()) {
        GOGGLES_TRY(m_frame_history.init(m_vk_ctx.device, m_vk_ctx.physical_device,
                                         vk::Format::eR8G8B8A8Unorm, extent,
                                         m_required_history_depth));
    }
    return {};
}

auto FilterChain::ensure_prechain_passes(vk::Extent2D captured_extent) -> Result<void> {
    if (!m_prechain_passes.empty() && !m_prechain_framebuffers.empty()) {
        return {};
    }

    if (m_source_resolution.width == 0 && m_source_resolution.height == 0) {
        return {};
    }

    vk::Extent2D target_resolution = m_source_resolution;
    if (target_resolution.width == 0) {
        target_resolution.width =
            static_cast<uint32_t>(std::round(static_cast<float>(target_resolution.height) *
                                             static_cast<float>(captured_extent.width) /
                                             static_cast<float>(captured_extent.height)));
        target_resolution.width = std::max(1U, target_resolution.width);
    } else if (target_resolution.height == 0) {
        target_resolution.height =
            static_cast<uint32_t>(std::round(static_cast<float>(target_resolution.width) *
                                             static_cast<float>(captured_extent.height) /
                                             static_cast<float>(captured_extent.width)));
        target_resolution.height = std::max(1U, target_resolution.height);
    }

    m_source_resolution = target_resolution;

    DownsamplePassConfig downsample_config{
        .target_format = vk::Format::eR8G8B8A8Unorm,
        .num_sync_indices = m_num_sync_indices,
        .shader_dir = m_shader_dir,
    };
    m_prechain_passes.push_back(
        GOGGLES_TRY(DownsamplePass::create(m_vk_ctx, *m_shader_runtime, downsample_config)));

    m_prechain_framebuffers.push_back(GOGGLES_TRY(Framebuffer::create(
        m_vk_ctx.device, m_vk_ctx.physical_device, vk::Format::eR8G8B8A8Unorm, target_resolution)));

    GOGGLES_LOG_INFO("FilterChain pre-chain initialized (aspect-ratio): {}x{} (from {}x{})",
                     target_resolution.width, target_resolution.height, captured_extent.width,
                     captured_extent.height);
    return {};
}

auto FilterChain::calculate_pass_output_size(const ShaderPassConfig& pass_config,
                                             vk::Extent2D source_extent,
                                             vk::Extent2D viewport_extent) -> vk::Extent2D {
    uint32_t width = 0;
    uint32_t height = 0;

    switch (pass_config.scale_type_x) {
    case ScaleType::source:
        width = static_cast<uint32_t>(
            std::round(static_cast<float>(source_extent.width) * pass_config.scale_x));
        break;
    case ScaleType::viewport:
        width = static_cast<uint32_t>(
            std::round(static_cast<float>(viewport_extent.width) * pass_config.scale_x));
        break;
    case ScaleType::absolute:
        width = static_cast<uint32_t>(pass_config.scale_x);
        break;
    }

    switch (pass_config.scale_type_y) {
    case ScaleType::source:
        height = static_cast<uint32_t>(
            std::round(static_cast<float>(source_extent.height) * pass_config.scale_y));
        break;
    case ScaleType::viewport:
        height = static_cast<uint32_t>(
            std::round(static_cast<float>(viewport_extent.height) * pass_config.scale_y));
        break;
    case ScaleType::absolute:
        height = static_cast<uint32_t>(pass_config.scale_y);
        break;
    }

    return vk::Extent2D{std::max(1U, width), std::max(1U, height)};
}

auto FilterChain::load_preset_textures() -> Result<void> {
    GOGGLES_PROFILE_SCOPE("LoadPresetTextures");

    for (const auto& tex_config : m_preset.textures) {
        TextureLoadConfig load_cfg{.generate_mipmaps = tex_config.mipmap,
                                   .linear = tex_config.linear};

        auto tex_data_result = m_texture_loader->load_from_file(tex_config.path, load_cfg);
        if (!tex_data_result) {
            return nonstd::make_unexpected(tex_data_result.error());
        }

        auto sampler = GOGGLES_TRY(create_texture_sampler(tex_config));

        m_texture_registry[tex_config.name] = LoadedTexture{
            .data = std::move(tex_data_result.value()),
            .sampler = std::move(sampler),
        };

        GOGGLES_LOG_DEBUG("Loaded texture '{}' from {}", tex_config.name,
                          tex_config.path.filename().string());
    }
    return {};
}

auto FilterChain::create_texture_sampler(const TextureConfig& config) const
    -> Result<vk::UniqueSampler> {
    vk::Filter filter =
        (config.filter_mode == FilterMode::linear) ? vk::Filter::eLinear : vk::Filter::eNearest;

    vk::SamplerAddressMode address_mode;
    switch (config.wrap_mode) {
    case WrapMode::clamp_to_edge:
        address_mode = vk::SamplerAddressMode::eClampToEdge;
        break;
    case WrapMode::repeat:
        address_mode = vk::SamplerAddressMode::eRepeat;
        break;
    case WrapMode::mirrored_repeat:
        address_mode = vk::SamplerAddressMode::eMirroredRepeat;
        break;
    case WrapMode::clamp_to_border:
    default:
        address_mode = vk::SamplerAddressMode::eClampToBorder;
        break;
    }

    vk::SamplerMipmapMode mipmap_mode = (config.filter_mode == FilterMode::linear)
                                            ? vk::SamplerMipmapMode::eLinear
                                            : vk::SamplerMipmapMode::eNearest;

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = vk::BorderColor::eFloatTransparentBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = config.mipmap ? VK_LOD_CLAMP_NONE : 0.0f;

    auto [result, sampler] = m_vk_ctx.device.createSamplerUnique(sampler_info);
    if (result != vk::Result::eSuccess) {
        return make_error<vk::UniqueSampler>(ErrorCode::vulkan_init_failed,
                                             "Failed to create texture sampler: " +
                                                 vk::to_string(result));
    }
    return Result<vk::UniqueSampler>{std::move(sampler)};
}

} // namespace goggles::render
