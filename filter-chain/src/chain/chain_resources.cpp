#include "chain_resources.hpp"

#include "diagnostics/diagnostic_event.hpp"
#include "support/logging.hpp"
#include "support/profiling.hpp"
#include "vulkan_result.hpp"

#include <cmath>
#include <format>

namespace goggles::render {

namespace {

auto capture_mode_name(diagnostics::CaptureMode mode) -> std::string {
    switch (mode) {
    case diagnostics::CaptureMode::minimal:
        return "minimal";
    case diagnostics::CaptureMode::investigate:
        return "investigate";
    case diagnostics::CaptureMode::forensic:
        return "forensic";
    case diagnostics::CaptureMode::standard:
    default:
        return "standard";
    }
}

auto build_environment_fingerprint(const VulkanContext& vk_ctx) -> std::string {
    const auto properties = vk_ctx.physical_device.getProperties();
    return std::format("linux|device={}|vendor={:#x}|device_id={:#x}|api={:#x}|driver={:#x}",
                       properties.deviceName.data(), properties.vendorID, properties.deviceID,
                       properties.apiVersion, properties.driverVersion);
}

void shutdown_output_state(ChainResources::OutputState& output_state) {
    for (auto& pass : output_state.postchain_passes) {
        pass->shutdown();
    }
    output_state.postchain_passes.clear();
    output_state.postchain_framebuffers.clear();
    output_state.swapchain_format = vk::Format::eUndefined;
}

auto build_output_state(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                        const std::filesystem::path& shader_dir, vk::Format swapchain_format,
                        uint32_t num_sync_indices) -> Result<ChainResources::OutputState> {
    ChainResources::OutputState output_state{};
    output_state.swapchain_format = swapchain_format;

    OutputPassConfig output_config{
        .target_format = swapchain_format,
        .num_sync_indices = num_sync_indices,
        .shader_dir = shader_dir,
    };
    output_state.postchain_passes.push_back(
        GOGGLES_TRY(OutputPass::create(vk_ctx, shader_runtime, output_config)));

    return output_state;
}

} // namespace

ChainResources::~ChainResources() {
    shutdown();
}

auto ChainResources::create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                            uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                            const std::filesystem::path& shader_dir, vk::Extent2D source_resolution)
    -> ResultPtr<ChainResources> {
    GOGGLES_PROFILE_FUNCTION();

    auto resources = std::unique_ptr<ChainResources>(new ChainResources());

    resources->m_vk_ctx = vk_ctx;
    resources->m_num_sync_indices = num_sync_indices;
    resources->m_shader_runtime = &shader_runtime;
    resources->m_shader_dir = shader_dir;
    resources->m_prechain_requested_resolution = source_resolution;
    resources->m_prechain_parameters = DownsamplePass::shader_parameters(0.0F);
    resources->m_output_state = GOGGLES_TRY(
        build_output_state(vk_ctx, shader_runtime, shader_dir, swapchain_format, num_sync_indices));

    resources->m_texture_loader = std::make_unique<TextureLoader>(
        vk_ctx.device, vk_ctx.physical_device, vk_ctx.command_pool, vk_ctx.graphics_queue);

    if (source_resolution.width > 0 && source_resolution.height > 0) {
        DownsamplePassConfig downsample_config{
            .target_format = vk::Format::eR8G8B8A8Unorm,
            .num_sync_indices = num_sync_indices,
            .shader_dir = shader_dir,
        };
        resources->m_prechain_passes.push_back(
            GOGGLES_TRY(DownsamplePass::create(vk_ctx, shader_runtime, downsample_config)));
        resources->apply_prechain_parameters();

        resources->m_prechain_framebuffers.push_back(GOGGLES_TRY(Framebuffer::create(
            vk_ctx.device, vk_ctx.physical_device, vk::Format::eR8G8B8A8Unorm, source_resolution)));
        resources->m_prechain_resolved_resolution = source_resolution;
        resources->m_prechain_last_captured_extent = source_resolution;

        GOGGLES_LOG_INFO("FilterChain pre-chain enabled: {}x{}", source_resolution.width,
                         source_resolution.height);
    } else if (source_resolution.width > 0 || source_resolution.height > 0) {
        GOGGLES_LOG_INFO("FilterChain pre-chain pending: width={}, height={}",
                         source_resolution.width, source_resolution.height);
    }

    GOGGLES_LOG_DEBUG("FilterChain initialized (passthrough mode)");
    return make_result_ptr(std::move(resources));
}

void ChainResources::install(CompiledChain&& compiled, diagnostics::DiagnosticSession* session) {
    m_preset = std::move(compiled.preset);
    m_passes = std::move(compiled.passes);
    m_alias_to_pass_index = std::move(compiled.alias_to_pass_index);
    m_framebuffers.clear();
    m_framebuffers.resize(m_passes.size());
    cleanup_texture_registry();
    m_texture_registry = std::move(compiled.texture_registry);

    m_frame_history.shutdown();

    m_required_history_depth = compiled.required_history_depth;
    m_feedback_framebuffers.clear();
    m_feedback_initialized.clear();
    for (auto pass_idx : compiled.feedback_pass_indices) {
        m_feedback_framebuffers.try_emplace(pass_idx);
        m_feedback_initialized.try_emplace(pass_idx, false);
    }

    ++m_generation_id;

    if (session == nullptr) {
        return;
    }

    auto identity = session->identity();
    identity.generation_id = m_generation_id;
    identity.capture_mode = capture_mode_name(session->policy().capture_mode);
    identity.environment_fingerprint = build_environment_fingerprint(m_vk_ctx);
    session->update_identity(std::move(identity));

    diagnostics::DiagnosticEvent event{};
    event.severity = diagnostics::Severity::info;
    event.original_severity = diagnostics::Severity::info;
    event.category = diagnostics::Category::runtime;
    event.localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                          .stage = "install",
                          .resource = {}};
    event.message = std::format("Installed compiled chain generation {} with {} pass(es)",
                                m_generation_id, m_passes.size());
    session->emit(std::move(event));
}

void ChainResources::shutdown() {
    m_passes.clear();
    m_framebuffers.clear();
    m_feedback_framebuffers.clear();
    m_feedback_initialized.clear();
    cleanup_texture_registry();
    m_alias_to_pass_index.clear();
    m_frame_history.shutdown();
    m_preset = PresetConfig{};
    m_frame_count = 0;
    m_required_history_depth = 0;

    for (auto& pass : m_prechain_passes) {
        pass->shutdown();
    }
    m_prechain_passes.clear();
    m_prechain_framebuffers.clear();

    shutdown_output_state(m_output_state);
}

auto ChainResources::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
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

    GOGGLES_TRY(ensure_framebuffers(
        {.viewport = new_viewport_extent, .source = m_last_source_extent}, {vp.width, vp.height}));
    return {};
}

auto ChainResources::retarget_output(vk::Format swapchain_format) -> Result<void> {
    if (swapchain_format == vk::Format::eUndefined) {
        return make_error<void>(ErrorCode::invalid_data, "Output retarget format is undefined");
    }

    if (m_output_state.swapchain_format == swapchain_format) {
        return {};
    }

    auto candidate = GOGGLES_TRY(build_output_state(m_vk_ctx, *m_shader_runtime, m_shader_dir,
                                                    swapchain_format, m_num_sync_indices));
    shutdown_output_state(m_output_state);
    m_output_state = std::move(candidate);
    return {};
}

auto ChainResources::ensure_framebuffers(const FramebufferExtents& extents,
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
                m_feedback_initialized[i] = false;
                GOGGLES_LOG_DEBUG("Created feedback framebuffer for pass {}", i);
            } else if (it->second->extent() != target_extent) {
                GOGGLES_TRY(it->second->resize(target_extent));
                m_feedback_initialized[i] = false;
            }
        }

        prev_extent = target_extent;
    }
    return {};
}

auto ChainResources::ensure_frame_history(vk::Extent2D extent) -> Result<void> {
    if (m_required_history_depth > 0 && !m_frame_history.is_initialized()) {
        GOGGLES_TRY(m_frame_history.init(m_vk_ctx.device, m_vk_ctx.physical_device,
                                         vk::Format::eR8G8B8A8Unorm, extent,
                                         m_required_history_depth));
    }
    return {};
}

auto ChainResources::ensure_prechain_passes(vk::Extent2D captured_extent) -> Result<void> {
    if (m_prechain_requested_resolution.width == 0 && m_prechain_requested_resolution.height == 0) {
        return {};
    }

    if (captured_extent.width == 0 || captured_extent.height == 0) {
        return {};
    }

    vk::Extent2D target_resolution = m_prechain_requested_resolution;
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

    const bool has_resources = !m_prechain_passes.empty() && !m_prechain_framebuffers.empty();
    const bool aspect_dependent =
        m_prechain_requested_resolution.width == 0 || m_prechain_requested_resolution.height == 0;
    const bool captured_extent_changed =
        aspect_dependent && captured_extent != m_prechain_last_captured_extent;
    const bool target_resolution_changed = target_resolution != m_prechain_resolved_resolution;
    if (has_resources && !captured_extent_changed && !target_resolution_changed) {
        return {};
    }

    for (auto& pass : m_prechain_passes) {
        pass->shutdown();
    }
    m_prechain_passes.clear();
    m_prechain_framebuffers.clear();

    DownsamplePassConfig downsample_config{
        .target_format = vk::Format::eR8G8B8A8Unorm,
        .num_sync_indices = m_num_sync_indices,
        .shader_dir = m_shader_dir,
    };
    m_prechain_passes.push_back(
        GOGGLES_TRY(DownsamplePass::create(m_vk_ctx, *m_shader_runtime, downsample_config)));
    apply_prechain_parameters();

    m_prechain_framebuffers.push_back(GOGGLES_TRY(Framebuffer::create(
        m_vk_ctx.device, m_vk_ctx.physical_device, vk::Format::eR8G8B8A8Unorm, target_resolution)));

    m_prechain_resolved_resolution = target_resolution;
    m_prechain_last_captured_extent = captured_extent;

    GOGGLES_LOG_INFO("FilterChain pre-chain initialized (aspect-ratio): {}x{} (from {}x{})",
                     target_resolution.width, target_resolution.height, captured_extent.width,
                     captured_extent.height);
    return {};
}

void ChainResources::apply_prechain_parameters() {
    for (const auto& parameter : m_prechain_parameters) {
        for (auto& pass : m_prechain_passes) {
            pass->set_shader_parameter(parameter.name, parameter.current_value);
        }
    }
}

void ChainResources::cleanup_texture_registry() {
    for (auto& [_, tex] : m_texture_registry) {
        if (tex.sampler) {
            m_vk_ctx.device.destroySampler(tex.sampler);
            tex.sampler = nullptr;
        }
        if (tex.data.view) {
            m_vk_ctx.device.destroyImageView(tex.data.view);
            tex.data.view = nullptr;
        }
        if (tex.data.image) {
            m_vk_ctx.device.destroyImage(tex.data.image);
            tex.data.image = nullptr;
        }
        if (tex.data.memory) {
            m_vk_ctx.device.freeMemory(tex.data.memory);
            tex.data.memory = nullptr;
        }
    }
    m_texture_registry.clear();
}

void ChainResources::set_prechain_resolution(uint32_t width, uint32_t height) {
    m_prechain_requested_resolution = vk::Extent2D{width, height};
    m_prechain_resolved_resolution = vk::Extent2D{};
    m_prechain_last_captured_extent = vk::Extent2D{};

    for (auto& pass : m_prechain_passes) {
        pass->shutdown();
    }
    m_prechain_passes.clear();
    m_prechain_framebuffers.clear();
}

auto ChainResources::get_prechain_resolution() const -> vk::Extent2D {
    return m_prechain_requested_resolution;
}

auto ChainResources::get_all_parameters() const -> std::vector<ParameterInfo> {
    std::unordered_set<std::string> seen_names;
    std::vector<ParameterInfo> result;
    for (const auto& pass : m_passes) {
        for (const auto& param : pass->parameters()) {
            if (!seen_names.insert(param.name).second) {
                continue;
            }

            result.push_back({
                .name = param.name,
                .description = param.description,
                .current_value = pass->get_parameter_value(param.name),
                .default_value = param.default_value,
                .min_value = param.min_value,
                .max_value = param.max_value,
                .step = param.step,
            });
        }
    }
    return result;
}

void ChainResources::set_parameter(const std::string& name, float value) {
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

void ChainResources::reset_parameter(const std::string& name) {
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

void ChainResources::clear_parameter_overrides() {
    for (auto& pass : m_passes) {
        pass->clear_parameter_overrides();
        GOGGLES_MUST(pass->update_ubo_parameters());
    }
}

auto ChainResources::get_prechain_parameters() const -> std::vector<ShaderParameter> {
    if (m_prechain_passes.empty()) {
        return m_prechain_parameters;
    }

    std::vector<ShaderParameter> result;
    for (const auto& pass : m_prechain_passes) {
        auto params = pass->get_shader_parameters();
        result.insert(result.end(), params.begin(), params.end());
    }
    return result;
}

void ChainResources::set_prechain_parameter(const std::string& name, float value) {
    const float sanitized = DownsamplePass::sanitize_parameter_value(name, value);
    for (auto& parameter : m_prechain_parameters) {
        if (parameter.name == name) {
            parameter.current_value = sanitized;
        }
    }

    for (auto& pass : m_prechain_passes) {
        pass->set_shader_parameter(name, sanitized);
    }
}

auto ChainResources::calculate_pass_output_size(const ShaderPassConfig& pass_config,
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

} // namespace goggles::render
