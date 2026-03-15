#include "program.hpp"

#include "chain/preset_parser.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "source_resolver.hpp"
#include "support/logging.hpp"

#include <new>

#define GOGGLES_LOG_TAG "render.runtime"

namespace goggles::filter_chain::runtime {

namespace {

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

auto Program::create(Device* device, const goggles_fc_preset_source_t* source,
                     goggles_fc_program_t** out_program) -> goggles_fc_status_t {
    goggles::filter_chain::detail::ScopedLogRouter log_scope(
        device != nullptr ? device->instance()->log_router() : nullptr);

    if (device == nullptr || source == nullptr || out_program == nullptr) {
        if (out_program != nullptr) {
            *out_program = nullptr;
        }
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }

    // Resolve the preset source into bytes + provenance
    SourceResolver resolver;
    auto resolved = resolver.resolve(source);
    if (!resolved) {
        GOGGLES_LOG_ERROR("Program::create: source resolution failed: {}",
                          resolved.error().message);
        *out_program = nullptr;
        return map_error_to_status(resolved.error().code);
    }

    // Parse the preset config (CPU-only) to cache pass/texture/shader counts.
    // Passthrough: empty bytes means no custom passes — zero counts.
    uint32_t pass_count = 0;
    uint32_t texture_count = 0;

    if (!resolved->bytes.empty()) {
        goggles::render::PresetParser parser;
        auto preset_result = parser.load(*resolved, resolver, source->import_callbacks);
        if (!preset_result) {
            GOGGLES_LOG_ERROR("Program::create: preset parsing failed: {}",
                              preset_result.error().message);
            *out_program = nullptr;
            return map_error_to_status(preset_result.error().code);
        }
        pass_count = static_cast<uint32_t>(preset_result->passes.size());
        texture_count = static_cast<uint32_t>(preset_result->textures.size());
    }

    auto* program = new (std::nothrow) Program();
    if (program == nullptr) {
        *out_program = nullptr;
        return GOGGLES_FC_STATUS_OUT_OF_MEMORY;
    }

    program->m_device = device;
    program->m_resolved_source = *resolved;
    program->m_provenance = program->m_resolved_source.provenance;
    if (source->import_callbacks != nullptr) {
        program->m_import_callbacks = *source->import_callbacks;
    }
    program->m_pass_count = pass_count;
    program->m_shader_count = pass_count * 2; // vertex + fragment per pass
    program->m_texture_count = texture_count;

    GOGGLES_LOG_INFO("Program created: {} ({} passes, {} textures)", program->source_name(),
                     program->pass_count(), program->texture_count());

    *out_program = program->as_handle();
    return GOGGLES_FC_STATUS_OK;
}

Program::~Program() {
    m_magic = 0;
}

auto Program::get_source_info(goggles_fc_program_source_info_t* out) const -> goggles_fc_status_t {
    out->struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_program_source_info_t);
    out->provenance = m_provenance.kind;
    out->source_name.data = m_provenance.source_name.c_str();
    out->source_name.size = m_provenance.source_name.size();
    out->source_path.data = m_provenance.source_path.c_str();
    out->source_path.size = m_provenance.source_path.size();
    out->pass_count = pass_count();
    return GOGGLES_FC_STATUS_OK;
}

auto Program::get_report(goggles_fc_program_report_t* out) const -> goggles_fc_status_t {
    out->struct_size = GOGGLES_FC_STRUCT_SIZE(goggles_fc_program_report_t);
    out->shader_count = shader_count();
    out->pass_count = pass_count();
    out->texture_count = texture_count();
    return GOGGLES_FC_STATUS_OK;
}

auto Program::as_handle() -> goggles_fc_program_t* {
    return reinterpret_cast<goggles_fc_program_t*>(this);
}

auto Program::from_handle(goggles_fc_program_t* handle) -> Program* {
    return reinterpret_cast<Program*>(handle);
}

auto Program::from_handle(const goggles_fc_program_t* handle) -> const Program* {
    return reinterpret_cast<const Program*>(handle);
}

} // namespace goggles::filter_chain::runtime
