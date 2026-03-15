#include "chain.hpp"

#include "api/abi_validation.hpp"
#include "chain/chain_builder.hpp"
#include "chain/chain_controls.hpp"
#include "chain/chain_executor.hpp"
#include "chain/chain_resources.hpp"
#include "device.hpp"
#include "diagnostics/diagnostic_policy.hpp"
#include "diagnostics/diagnostic_session.hpp"
#include "instance.hpp"
#include "program.hpp"
#include "shader/shader_runtime.hpp"
#include "support/logging.hpp"

#include <algorithm>
#include <cmath>
#include <goggles/filter_chain/scale_mode.hpp>
#include <new>

#define GOGGLES_LOG_TAG "render.runtime"

namespace goggles::filter_chain::runtime {

namespace {

auto map_fc_scale_mode(uint32_t fc_mode) -> std::optional<goggles::ScaleMode> {
    switch (fc_mode) {
    case GOGGLES_FC_SCALE_MODE_FIT:
        return goggles::ScaleMode::fit;
    case GOGGLES_FC_SCALE_MODE_INTEGER:
        return goggles::ScaleMode::integer;
    case GOGGLES_FC_SCALE_MODE_FILL:
        return goggles::ScaleMode::fill;
    case GOGGLES_FC_SCALE_MODE_DYNAMIC:
        return goggles::ScaleMode::dynamic;
    case GOGGLES_FC_SCALE_MODE_STRETCH:
        return goggles::ScaleMode::stretch;
    default:
        return std::nullopt;
    }
}

auto map_error_to_status(goggles::ErrorCode code) -> goggles_fc_status_t {
    switch (code) {
    case goggles::ErrorCode::file_not_found:
        return GOGGLES_FC_STATUS_NOT_FOUND;
    case goggles::ErrorCode::file_read_failed:
        return GOGGLES_FC_STATUS_IO_ERROR;
    case goggles::ErrorCode::invalid_config:
        return GOGGLES_FC_STATUS_VALIDATION_ERROR;
    case goggles::ErrorCode::invalid_data:
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    case goggles::ErrorCode::parse_error:
    case goggles::ErrorCode::shader_compile_failed:
    case goggles::ErrorCode::shader_load_failed:
        return GOGGLES_FC_STATUS_PRESET_ERROR;
    case goggles::ErrorCode::vulkan_init_failed:
    case goggles::ErrorCode::vulkan_device_lost:
        return GOGGLES_FC_STATUS_VULKAN_ERROR;
    default:
        return GOGGLES_FC_STATUS_RUNTIME_ERROR;
    }
}

} // namespace

auto Chain::create(Device* device, const Program* program,
                   const goggles_fc_chain_create_info_t* create_info,
                   goggles_fc_chain_t** out_chain) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        device != nullptr ? device->instance()->log_router() : nullptr);

    if (device == nullptr || program == nullptr || create_info == nullptr || out_chain == nullptr) {
        if (out_chain != nullptr) {
            *out_chain = nullptr;
        }
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    // Verify device-affine program ownership: program must belong to same device
    if (program->device() != device) {
        GOGGLES_LOG_ERROR("Chain::create: program is affine to a different device");
        *out_chain = nullptr;
        return GOGGLES_FC_STATUS_INVALID_DEPENDENCY;
    }

    // Build VulkanContext from device's borrowed handles
    goggles::render::VulkanContext vk_ctx{};
    vk_ctx.device = vk::Device(device->vk_device());
    vk_ctx.physical_device = vk::PhysicalDevice(device->physical_device());
    vk_ctx.command_pool = vk::CommandPool(device->setup_command_pool());
    vk_ctx.graphics_queue = vk::Queue(device->graphics_queue());
    vk_ctx.graphics_queue_family_index = device->graphics_queue_family_index();

    // Create shader runtime for this chain's internal passes (output/downsample)
    auto shader_runtime_result = goggles::render::ShaderRuntime::create(device->cache_dir());
    if (!shader_runtime_result) {
        GOGGLES_LOG_ERROR("Chain::create: shader runtime init failed: {}",
                          shader_runtime_result.error().message);
        *out_chain = nullptr;
        return GOGGLES_FC_STATUS_RUNTIME_ERROR;
    }

    // Determine initial source resolution from create info
    vk::Extent2D source_resolution{create_info->initial_prechain_resolution.width,
                                   create_info->initial_prechain_resolution.height};

    // ChainResources needs a shader_dir for built-in internal shaders.
    // With embedded assets, this can be empty -- the asset registry handles it.
    std::filesystem::path shader_dir;

    auto resources = goggles::render::ChainResources::create(
        vk_ctx, static_cast<vk::Format>(create_info->target_format), create_info->frames_in_flight,
        **shader_runtime_result, shader_dir, source_resolution);
    if (!resources) {
        GOGGLES_LOG_ERROR("Chain::create: resources init failed: {}", resources.error().message);
        *out_chain = nullptr;
        return GOGGLES_FC_STATUS_VULKAN_ERROR;
    }

    auto* chain = new (std::nothrow) Chain();
    if (chain == nullptr) {
        *out_chain = nullptr;
        return GOGGLES_FC_STATUS_OUT_OF_MEMORY;
    }

    chain->m_device = device;
    chain->m_program = program;
    chain->m_shader_runtime = std::move(*shader_runtime_result);
    chain->m_resources = std::move(*resources);
    chain->m_executor = std::make_unique<goggles::render::ChainExecutor>();
    chain->m_controls = std::make_unique<goggles::render::ChainControls>();
    chain->m_frames_in_flight = create_info->frames_in_flight;
    chain->m_stage_mask = create_info->initial_stage_mask;
    chain->m_last_error = goggles_fc_chain_error_info_init();

    std::optional<goggles::render::CompiledChain> compiled;
    if (program->resolved_source().bytes.empty()) {
        compiled.emplace();
    } else {
        SourceResolver resolver;
        auto build_result = goggles::render::ChainBuilder::build(
            vk_ctx, *chain->m_shader_runtime, create_info->frames_in_flight,
            *chain->m_resources->m_texture_loader, program->resolved_source(), resolver,
            program->import_callbacks(), chain->m_diagnostic_session.get());
        if (!build_result) {
            const auto status = map_error_to_status(build_result.error().code);
            GOGGLES_LOG_ERROR("Chain::create: chain install build failed: {}",
                              build_result.error().message);
            delete chain;
            *out_chain = nullptr;
            return status;
        }
        compiled = std::move(*build_result);
    }

    chain->m_resources->install(std::move(*compiled), chain->m_diagnostic_session.get());
    chain->m_controls_dirty = true;

    // Apply stage policy from create info
    bool prechain_enabled = (create_info->initial_stage_mask & GOGGLES_FC_STAGE_MASK_PRECHAIN) != 0;
    bool effect_enabled = (create_info->initial_stage_mask & GOGGLES_FC_STAGE_MASK_EFFECT) != 0;
    chain->m_resources->set_prechain_enabled(prechain_enabled);
    chain->m_resources->set_bypass(!effect_enabled);

    GOGGLES_LOG_INFO("Chain created for program '{}' ({} frames in flight)", program->source_name(),
                     create_info->frames_in_flight);

    *out_chain = chain->as_handle();
    return GOGGLES_FC_STATUS_OK;
}

Chain::~Chain() {
    // Destroy diagnostic session first — it may hold references to resource state
    m_diagnostic_session.reset();

    if (m_resources) {
        m_resources->shutdown();
        m_resources.reset();
    }
    if (m_shader_runtime) {
        m_shader_runtime->shutdown();
        m_shader_runtime.reset();
    }
    m_magic = 0;
}

auto Chain::bind_program(const Program* program) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (program == nullptr || m_device == nullptr || m_resources == nullptr ||
        m_shader_runtime == nullptr || m_controls == nullptr) {
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    // Verify device-affine ownership
    if (program->device() != m_device) {
        GOGGLES_LOG_ERROR("Chain::bind_program: program is affine to a different device");
        set_last_error(GOGGLES_FC_STATUS_INVALID_DEPENDENCY);
        return GOGGLES_FC_STATUS_INVALID_DEPENDENCY;
    }

    goggles::render::VulkanContext vk_ctx{};
    vk_ctx.device = vk::Device(m_device->vk_device());
    vk_ctx.physical_device = vk::PhysicalDevice(m_device->physical_device());
    vk_ctx.command_pool = vk::CommandPool(m_device->setup_command_pool());
    vk_ctx.graphics_queue = vk::Queue(m_device->graphics_queue());
    vk_ctx.graphics_queue_family_index = m_device->graphics_queue_family_index();

    std::optional<goggles::render::CompiledChain> compiled;
    if (program->resolved_source().bytes.empty()) {
        compiled.emplace();
    } else {
        SourceResolver resolver;
        auto build_result = goggles::render::ChainBuilder::build(
            vk_ctx, *m_shader_runtime, m_frames_in_flight, *m_resources->m_texture_loader,
            program->resolved_source(), resolver, program->import_callbacks(),
            m_diagnostic_session.get());
        if (!build_result) {
            const auto status = map_error_to_status(build_result.error().code);
            GOGGLES_LOG_ERROR("Chain::bind_program: rebuild failed: {}",
                              build_result.error().message);
            set_last_error(status);
            return status;
        }
        compiled = std::move(*build_result);
    }

    m_resources->install(std::move(*compiled), m_diagnostic_session.get());
    m_resources->set_prechain_enabled((m_stage_mask & GOGGLES_FC_STAGE_MASK_PRECHAIN) != 0);
    m_resources->set_bypass((m_stage_mask & GOGGLES_FC_STAGE_MASK_EFFECT) == 0);

    m_program = program;
    m_controls->replay_values(*m_resources);
    m_controls_dirty = true;
    m_frames_rendered = 0;

    GOGGLES_LOG_INFO("Chain bound to program '{}'", program->source_name());
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::clear() -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    // Reset frame count and control overrides
    m_frames_rendered = 0;
    m_controls->reset_controls(*m_resources);
    m_controls_dirty = true;

    return GOGGLES_FC_STATUS_OK;
}

auto Chain::resize(const goggles_fc_extent_2d_t* new_source_extent) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    vk::Extent2D extent{new_source_extent->width, new_source_extent->height};
    auto result = m_resources->handle_resize(extent);
    if (!result) {
        GOGGLES_LOG_ERROR("Chain::resize failed: {}", result.error().message);
        set_last_error(GOGGLES_FC_STATUS_VULKAN_ERROR);
        return GOGGLES_FC_STATUS_VULKAN_ERROR;
    }

    return GOGGLES_FC_STATUS_OK;
}

auto Chain::retarget(const goggles_fc_chain_target_info_t* target_info) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    auto result = m_resources->retarget_output(static_cast<vk::Format>(target_info->target_format));
    if (!result) {
        GOGGLES_LOG_ERROR("Chain::retarget failed: {}", result.error().message);
        set_last_error(GOGGLES_FC_STATUS_VULKAN_ERROR);
        return GOGGLES_FC_STATUS_VULKAN_ERROR;
    }

    return GOGGLES_FC_STATUS_OK;
}

auto Chain::record_vk(const goggles_fc_record_info_vk_t* record_info) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources || !m_executor) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    vk::CommandBuffer cmd(record_info->command_buffer);
    vk::Image source_image(record_info->source_image);
    vk::ImageView source_view(record_info->source_view);
    vk::Extent2D source_extent{record_info->source_extent.width, record_info->source_extent.height};
    vk::ImageView target_view(record_info->target_view);
    vk::Extent2D target_extent{record_info->target_extent.width, record_info->target_extent.height};

    auto scale_mode = map_fc_scale_mode(record_info->scale_mode);
    if (!scale_mode) {
        GOGGLES_LOG_ERROR("Chain::record_vk: unknown scale_mode value {}", record_info->scale_mode);
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    auto frame_status = detail::validate_frame_index(record_info->frame_index, m_frames_in_flight);
    if (frame_status != GOGGLES_FC_STATUS_OK) {
        set_last_error(frame_status);
        return frame_status;
    }

    if (m_diagnostic_session) {
        m_diagnostic_session->begin_frame(record_info->frame_index);
    }

    m_executor->record(*m_resources, cmd, source_image, source_view, source_extent, target_view,
                       target_extent, record_info->frame_index, *scale_mode,
                       record_info->integer_scale, m_diagnostic_session.get());

    if (m_diagnostic_session) {
        m_diagnostic_session->end_frame();
    }

    ++m_frames_rendered;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::get_report(goggles_fc_chain_report_t* out) const -> goggles_fc_status_t {
    out->struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_report_t);
    out->pass_count = m_resources ? static_cast<uint32_t>(m_resources->pass_count()) : 0;
    out->frames_rendered = m_frames_rendered;
    out->current_stage_mask = m_stage_mask;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::get_last_error(goggles_fc_chain_error_info_t* out) const -> goggles_fc_status_t {
    *out = m_last_error;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::get_control_count(uint32_t* out_count) const -> goggles_fc_status_t {
    if (!m_resources || !m_controls) {
        *out_count = 0;
        return GOGGLES_FC_STATUS_OK;
    }

    // NOTE: There is a TOCTOU window between get_control_count() and
    // get_control_info(i) — the control list could change between the two calls
    // if another thread calls bind_program() or clear(). This is a known
    // limitation of the index-based control query API.
    refresh_controls();
    *out_count = static_cast<uint32_t>(m_cached_controls.size());
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::get_control_info(uint32_t index, goggles_fc_control_info_t* out) const
    -> goggles_fc_status_t {
    refresh_controls();

    if (index >= m_cached_controls.size()) {
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    const auto& ctrl = m_cached_controls[index];
    out->struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_control_info_t);
    out->index = index;
    out->stage = (ctrl.stage == goggles::render::FilterControlStage::prechain)
                     ? GOGGLES_FC_STAGE_PRECHAIN
                     : GOGGLES_FC_STAGE_EFFECT;
    out->name.data = ctrl.name.c_str();
    out->name.size = ctrl.name.size();
    out->description.data = ctrl.description.has_value() ? ctrl.description->c_str() : nullptr;
    out->description.size = ctrl.description.has_value() ? ctrl.description->size() : 0;
    out->current_value = ctrl.current_value;
    out->default_value = ctrl.default_value;
    out->min_value = ctrl.min_value;
    out->max_value = ctrl.max_value;
    out->step = ctrl.step;

    return GOGGLES_FC_STATUS_OK;
}

auto Chain::find_control_index(uint32_t stage, std::string_view name, uint32_t* out_index) const
    -> goggles_fc_status_t {
    if (out_index == nullptr) {
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    refresh_controls();

    const auto index = find_control_index_impl(stage, name);
    if (!index.has_value()) {
        set_last_error(GOGGLES_FC_STATUS_NOT_FOUND);
        return GOGGLES_FC_STATUS_NOT_FOUND;
    }

    *out_index = *index;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::set_control_value_f32(uint32_t index, float value) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources || !m_controls) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    if (!std::isfinite(value)) {
        GOGGLES_LOG_ERROR("Chain::set_control_value_f32: non-finite value rejected");
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    refresh_controls();

    if (index >= m_cached_controls.size()) {
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    const auto& ctrl = m_cached_controls[index];
    bool success = m_controls->set_control_value(*m_resources, ctrl.control_id, value);
    if (!success) {
        set_last_error(GOGGLES_FC_STATUS_NOT_FOUND);
        return GOGGLES_FC_STATUS_NOT_FOUND;
    }

    m_controls_dirty = true;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::set_control_value_f32(uint32_t stage, std::string_view name, float value)
    -> goggles_fc_status_t {
    uint32_t index = 0;
    const auto status = find_control_index(stage, name, &index);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return set_control_value_f32(index, value);
}

auto Chain::reset_control_value(uint32_t index) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources || !m_controls) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    refresh_controls();

    if (index >= m_cached_controls.size()) {
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    const auto& ctrl = m_cached_controls[index];
    bool success = m_controls->reset_control_value(*m_resources, ctrl.control_id);
    if (!success) {
        set_last_error(GOGGLES_FC_STATUS_NOT_FOUND);
        return GOGGLES_FC_STATUS_NOT_FOUND;
    }

    m_controls_dirty = true;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::reset_all_controls() -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources || !m_controls) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    m_controls->reset_controls(*m_resources);
    m_controls_dirty = true;

    return GOGGLES_FC_STATUS_OK;
}

auto Chain::get_prechain_resolution(goggles_fc_extent_2d_t* out) const -> goggles_fc_status_t {
    if (!m_resources) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    auto extent = m_resources->get_prechain_resolution();
    out->width = extent.width;
    out->height = extent.height;
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::set_prechain_resolution(const goggles_fc_extent_2d_t* resolution)
    -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    m_resources->set_prechain_resolution(resolution->width, resolution->height);
    return GOGGLES_FC_STATUS_OK;
}

auto Chain::set_stage_mask(uint32_t mask) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    if (!m_resources) {
        set_last_error(GOGGLES_FC_STATUS_NOT_INITIALIZED);
        return GOGGLES_FC_STATUS_NOT_INITIALIZED;
    }

    bool prechain_enabled = (mask & GOGGLES_FC_STAGE_MASK_PRECHAIN) != 0;
    bool effect_enabled = (mask & GOGGLES_FC_STAGE_MASK_EFFECT) != 0;
    m_resources->set_prechain_enabled(prechain_enabled);
    m_resources->set_bypass(!effect_enabled);
    m_stage_mask = mask;

    return GOGGLES_FC_STATUS_OK;
}

void Chain::create_diagnostic_session(const goggles_fc_diagnostic_policy_t* policy) {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    // Destroy any existing session first
    m_diagnostic_session.reset();

    goggles::diagnostics::DiagnosticPolicy internal_policy{};
    if (policy != nullptr) {
        internal_policy.capture_mode =
            static_cast<goggles::diagnostics::CaptureMode>(policy->capture_mode);
        internal_policy.tier =
            static_cast<goggles::diagnostics::ActivationTier>(policy->activation_tier);
        internal_policy.capture_frame_limit = policy->capture_frame_limit;
    }

    m_diagnostic_session = goggles::diagnostics::DiagnosticSession::create(internal_policy);
    GOGGLES_LOG_INFO("Diagnostic session created for chain");
}

void Chain::destroy_diagnostic_session() {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        m_device != nullptr ? m_device->instance()->log_router() : nullptr);

    m_diagnostic_session.reset();
    GOGGLES_LOG_INFO("Diagnostic session destroyed for chain");
}

auto Chain::diagnostic_session() const -> goggles::diagnostics::DiagnosticSession* {
    return m_diagnostic_session.get();
}

auto Chain::get_diagnostic_summary(goggles_fc_diagnostic_summary_t* out) const
    -> goggles_fc_status_t {
    if (out == nullptr) {
        set_last_error(GOGGLES_FC_STATUS_INVALID_ARGUMENT);
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    out->struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_diagnostic_summary_t);

    if (!m_diagnostic_session) {
        out->session_active = false;
        out->debug_count = 0;
        out->info_count = 0;
        out->warning_count = 0;
        out->error_count = 0;
        out->current_frame = 0;
        out->total_events = 0;
        return GOGGLES_FC_STATUS_OK;
    }

    out->session_active = true;
    out->debug_count = m_diagnostic_session->event_count(goggles::diagnostics::Severity::debug);
    out->info_count = m_diagnostic_session->event_count(goggles::diagnostics::Severity::info);
    out->warning_count = m_diagnostic_session->event_count(goggles::diagnostics::Severity::warning);
    out->error_count = m_diagnostic_session->event_count(goggles::diagnostics::Severity::error);
    out->current_frame = m_diagnostic_session->current_frame();
    out->total_events = static_cast<uint32_t>(m_diagnostic_session->events().size());
    return GOGGLES_FC_STATUS_OK;
}

void Chain::set_last_error(goggles_fc_status_t status, int32_t vk_result,
                           uint32_t subsystem_code) const {
    m_last_error.struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_chain_error_info_t);
    m_last_error.status = status;
    m_last_error.vk_result = vk_result;
    m_last_error.subsystem_code = subsystem_code;
}

void Chain::refresh_controls() const {
    if (!m_controls_dirty || !m_resources || !m_controls) {
        return;
    }
    m_cached_controls = m_controls->list_controls(*m_resources);
    m_controls_dirty = false;
}

auto Chain::find_control_index_impl(uint32_t stage, std::string_view name) const
    -> std::optional<uint32_t> {
    if ((stage != GOGGLES_FC_STAGE_PRECHAIN && stage != GOGGLES_FC_STAGE_EFFECT) || name.empty()) {
        return std::nullopt;
    }

    for (uint32_t i = 0; i < m_cached_controls.size(); ++i) {
        const auto& control = m_cached_controls[i];
        const auto control_stage = control.stage == goggles::render::FilterControlStage::prechain
                                       ? GOGGLES_FC_STAGE_PRECHAIN
                                       : GOGGLES_FC_STAGE_EFFECT;
        if (control_stage != stage) {
            continue;
        }
        if (control.name == name) {
            return i;
        }
    }

    return std::nullopt;
}

auto Chain::as_handle() -> goggles_fc_chain_t* {
    return reinterpret_cast<goggles_fc_chain_t*>(this);
}

auto Chain::from_handle(goggles_fc_chain_t* handle) -> Chain* {
    return reinterpret_cast<Chain*>(handle);
}

auto Chain::from_handle(const goggles_fc_chain_t* handle) -> const Chain* {
    return reinterpret_cast<const Chain*>(handle);
}

} // namespace goggles::filter_chain::runtime
