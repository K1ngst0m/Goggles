#include "chain_builder.hpp"

#include "diagnostics/compile_report.hpp"
#include "diagnostics/source_provenance.hpp"
#include "runtime/source_resolver.hpp"
#include "shader/retroarch_preprocessor.hpp"
#include "support/logging.hpp"
#include "support/profiling.hpp"
#include "vulkan_result.hpp"

#include <charconv>
#include <cstdint>
#include <functional>
#include <sstream>
#include <unordered_set>

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

auto scale_type_string(ScaleType type) -> std::string_view {
    switch (type) {
    case ScaleType::source:
        return "source";
    case ScaleType::viewport:
        return "viewport";
    case ScaleType::absolute:
        return "absolute";
    }
    return "source";
}

auto wrap_mode_string(WrapMode mode) -> std::string_view {
    switch (mode) {
    case WrapMode::clamp_to_edge:
        return "clamp_to_edge";
    case WrapMode::repeat:
        return "repeat";
    case WrapMode::mirrored_repeat:
        return "mirrored_repeat";
    case WrapMode::clamp_to_border:
        return "clamp_to_border";
    }
    return "clamp_to_border";
}

auto filter_mode_string(FilterMode mode) -> std::string_view {
    switch (mode) {
    case FilterMode::linear:
        return "linear";
    case FilterMode::nearest:
        return "nearest";
    }
    return "linear";
}

auto compile_stage_string(diagnostics::CompileStage stage) -> std::string_view {
    switch (stage) {
    case diagnostics::CompileStage::vertex:
        return "vertex";
    case diagnostics::CompileStage::fragment:
        return "fragment";
    }
    return "unknown";
}

auto fnv1a_hash(std::string_view value) -> std::string {
    constexpr uint64_t OFFSET_BASIS = 14695981039346656037ull;
    constexpr uint64_t PRIME = 1099511628211ull;

    uint64_t hash = OFFSET_BASIS;
    for (const auto ch : value) {
        hash ^= static_cast<uint8_t>(ch);
        hash *= PRIME;
    }

    std::ostringstream stream;
    stream << std::hex << hash;
    return stream.str();
}

void append_reflection_signature(std::string* output, const ReflectionData& reflection) {
    if (reflection.ubo.has_value()) {
        output->append("ubo|");
        output->append(std::to_string(reflection.ubo->binding));
        output->push_back('|');
        output->append(std::to_string(reflection.ubo->set));
        output->push_back('|');
        output->append(std::to_string(reflection.ubo->total_size));
        output->push_back('|');
        for (const auto& member : reflection.ubo->members) {
            output->append(member.name);
            output->push_back(':');
            output->append(std::to_string(member.offset));
            output->push_back(':');
            output->append(std::to_string(member.size));
            output->push_back('|');
        }
    }

    if (reflection.push_constants.has_value()) {
        output->append("push|");
        output->append(std::to_string(reflection.push_constants->total_size));
        output->push_back('|');
        for (const auto& member : reflection.push_constants->members) {
            output->append(member.name);
            output->push_back(':');
            output->append(std::to_string(member.offset));
            output->push_back(':');
            output->append(std::to_string(member.size));
            output->push_back('|');
        }
    }

    for (const auto& texture : reflection.textures) {
        output->append("tex|");
        output->append(texture.name);
        output->push_back(':');
        output->append(std::to_string(texture.binding));
        output->push_back(':');
        output->append(std::to_string(texture.set));
        output->push_back('|');
    }

    for (const auto& input : reflection.vertex_inputs) {
        output->append("vin|");
        output->append(input.name);
        output->push_back(':');
        output->append(std::to_string(input.location));
        output->push_back(':');
        output->append(std::to_string(input.offset));
        output->push_back('|');
    }
}

auto serialize_preset(const PresetConfig& preset) -> std::string {
    std::string output;
    for (const auto& pass : preset.passes) {
        output.append(pass.shader_path.string());
        output.push_back('|');
        output.append(std::string(scale_type_string(pass.scale_type_x)));
        output.push_back('|');
        output.append(std::string(scale_type_string(pass.scale_type_y)));
        output.push_back('|');
        output.append(std::to_string(pass.scale_x));
        output.push_back('|');
        output.append(std::to_string(pass.scale_y));
        output.push_back('|');
        output.append(std::string(filter_mode_string(pass.filter_mode)));
        output.push_back('|');
        output.append(std::string(wrap_mode_string(pass.wrap_mode)));
        output.push_back('|');
        output.append(std::to_string(pass.frame_count_mod));
        output.push_back('|');
        if (pass.alias.has_value()) {
            output.append(*pass.alias);
        }
        output.push_back('\n');
    }

    for (const auto& texture : preset.textures) {
        output.append(texture.name);
        output.push_back('|');
        output.append(texture.path.string());
        output.push_back('|');
        output.append(std::string(filter_mode_string(texture.filter_mode)));
        output.push_back('|');
        output.append(texture.mipmap ? "1" : "0");
        output.push_back('|');
        output.append(std::string(wrap_mode_string(texture.wrap_mode)));
        output.push_back('\n');
    }

    for (const auto& parameter : preset.parameters) {
        output.append(parameter.name);
        output.push_back('=');
        output.append(std::to_string(parameter.value));
        output.push_back('\n');
    }

    return output;
}

void update_identity_field(diagnostics::DiagnosticSession& session,
                           const std::function<void(diagnostics::SessionIdentity*)>& apply) {
    auto identity = session.identity();
    apply(&identity);
    session.update_identity(std::move(identity));
}

void emit_preset_parse_event(diagnostics::DiagnosticSession& session,
                             const std::filesystem::path& preset_path, const PresetConfig& preset) {
    session.emit({.severity = diagnostics::Severity::info,
                  .original_severity = diagnostics::Severity::info,
                  .category = diagnostics::Category::authoring,
                  .localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                                   .stage = "preset_parse",
                                   .resource = {}},
                  .frame_index = 0,
                  .timestamp_ns = 0,
                  .message = "Parsed preset '" + preset_path.filename().string() + "' (" +
                             std::to_string(preset.passes.size()) + " passes, " +
                             std::to_string(preset.textures.size()) + " textures)",
                  .evidence = {},
                  .session_identity = std::nullopt});
}

void emit_include_graph_event(diagnostics::DiagnosticSession& session, uint32_t pass_ordinal,
                              const std::filesystem::path& shader_path,
                              const diagnostics::SourceProvenanceMap& provenance) {
    session.emit(
        {.severity = diagnostics::Severity::info,
         .original_severity = diagnostics::Severity::info,
         .category = diagnostics::Category::authoring,
         .localization = {.pass_ordinal = pass_ordinal, .stage = "include_graph", .resource = {}},
         .frame_index = 0,
         .timestamp_ns = 0,
         .message = "Include expansion succeeded for '" + shader_path.filename().string() + "' (" +
                    std::to_string(provenance.size()) + " mapped lines)",
         .evidence = diagnostics::ProvenanceEvidence{.original_file = shader_path.string(),
                                                     .original_line = 0,
                                                     .rewrite_applied = false,
                                                     .rewrite_description = {}},
         .session_identity = std::nullopt});
}

auto summarize_reflection(const ReflectionData& reflection) -> std::vector<std::string> {
    std::vector<std::string> summary;
    if (reflection.ubo.has_value()) {
        summary.push_back("ubo@binding" + std::to_string(reflection.ubo->binding));
    }
    if (reflection.push_constants.has_value()) {
        summary.push_back("push_constants:" +
                          std::to_string(reflection.push_constants->total_size));
    }
    for (const auto& texture : reflection.textures) {
        summary.push_back("texture:" + texture.name + "@" + std::to_string(texture.binding));
    }
    for (const auto& input : reflection.vertex_inputs) {
        summary.push_back("vertex:" + input.name + "@" + std::to_string(input.location));
    }
    return summary;
}

void emit_reflection_event(diagnostics::DiagnosticSession& session, uint32_t pass_ordinal,
                           const FilterPass& pass) {
    session.emit(
        {.severity = diagnostics::Severity::info,
         .original_severity = diagnostics::Severity::info,
         .category = diagnostics::Category::authoring,
         .localization = {.pass_ordinal = pass_ordinal, .stage = "reflection", .resource = {}},
         .frame_index = 0,
         .timestamp_ns = 0,
         .message = "Merged reflection contract for pass " + std::to_string(pass_ordinal),
         .evidence = diagnostics::ReflectionEvidence{.stage = "vertex+fragment",
                                                     .resource_summary =
                                                         summarize_reflection(pass.reflection()),
                                                     .merge_conflicts = {}},
         .session_identity = std::nullopt});
}

void emit_chain_manifest(diagnostics::DiagnosticSession& session, const PresetConfig& preset) {
    auto manifest = std::make_unique<diagnostics::ChainManifest>();
    for (size_t i = 0; i < preset.passes.size(); ++i) {
        const auto& pc = preset.passes[i];
        diagnostics::ManifestPassEntry entry;
        entry.ordinal = static_cast<uint32_t>(i);
        entry.shader_path = pc.shader_path.string();
        entry.scale_type_x = scale_type_string(pc.scale_type_x);
        entry.scale_type_y = scale_type_string(pc.scale_type_y);
        entry.scale_x = pc.scale_x;
        entry.scale_y = pc.scale_y;
        entry.wrap_mode = wrap_mode_string(pc.wrap_mode);
        if (pc.alias.has_value()) {
            entry.alias = *pc.alias;
        }
        manifest->add_pass(std::move(entry));
    }
    for (const auto& tex : preset.textures) {
        diagnostics::ManifestTextureEntry tex_entry;
        tex_entry.name = tex.name;
        tex_entry.path = tex.path.string();
        tex_entry.mipmap = tex.mipmap;
        manifest->add_texture(std::move(tex_entry));
    }

    session.emit({.severity = diagnostics::Severity::info,
                  .original_severity = diagnostics::Severity::info,
                  .category = diagnostics::Category::authoring,
                  .localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                                   .stage = "manifest",
                                   .resource = {}},
                  .frame_index = 0,
                  .timestamp_ns = 0,
                  .message = "Chain manifest generated",
                  .evidence = {},
                  .session_identity = std::nullopt});
    session.set_chain_manifest(std::move(manifest));
}

void emit_pass_provenance(diagnostics::DiagnosticSession& session, uint32_t pass_ordinal,
                          const std::filesystem::path& shader_path,
                          const diagnostics::SourceProvenanceMap& provenance) {
    session.emit(
        {.severity = diagnostics::Severity::debug,
         .original_severity = diagnostics::Severity::debug,
         .category = diagnostics::Category::authoring,
         .localization = {.pass_ordinal = pass_ordinal, .stage = "provenance", .resource = {}},
         .frame_index = 0,
         .timestamp_ns = 0,
         .message = "Source provenance tracked (" + std::to_string(provenance.size()) + " entries)",
         .evidence = diagnostics::ProvenanceEvidence{.original_file = shader_path.string(),
                                                     .original_line = 0,
                                                     .rewrite_applied = false,
                                                     .rewrite_description = {}},
         .session_identity = std::nullopt});
}

void emit_compile_diagnostics(diagnostics::DiagnosticSession& session, uint32_t pass_ordinal,
                              const diagnostics::CompileReport& report,
                              diagnostics::AuthoringVerdict& verdict) {
    for (const auto& stage : report.stages()) {
        const auto severity =
            stage.success ? diagnostics::Severity::info : diagnostics::Severity::error;
        session.emit(
            {.severity = severity,
             .original_severity = severity,
             .category = diagnostics::Category::authoring,
             .localization = {.pass_ordinal = pass_ordinal, .stage = "compile", .resource = {}},
             .frame_index = 0,
             .timestamp_ns = 0,
             .message = std::string(compile_stage_string(stage.stage)) +
                        (stage.success ? " stage compiled" : " stage failed to compile"),
             .evidence = diagnostics::CompileEvidence{.stage = std::string(
                                                          compile_stage_string(stage.stage)),
                                                      .success = stage.success,
                                                      .messages = stage.messages,
                                                      .timing_us = stage.timing_us,
                                                      .cache_hit = stage.cache_hit},
             .session_identity = std::nullopt});
    }

    if (!report.all_succeeded()) {
        verdict.result = diagnostics::VerdictResult::fail;
        for (const auto& stage : report.stages()) {
            if (!stage.success) {
                for (const auto& msg : stage.messages) {
                    verdict.findings.push_back({.severity = diagnostics::Severity::error,
                                                .localization = {.pass_ordinal = pass_ordinal,
                                                                 .stage = "compile",
                                                                 .resource = {}},
                                                .message = msg});
                }
            }
        }
    }
}

// Returns an error message if reflection loss is fatal and the build should abort.
auto evaluate_reflection_gate(diagnostics::DiagnosticSession& session, uint32_t pass_ordinal,
                              const FilterPass& pass, diagnostics::AuthoringVerdict& verdict)
    -> std::optional<std::string> {
    bool reflection_empty = pass.texture_bindings().empty() && pass.parameters().empty();
    if (!reflection_empty) {
        return std::nullopt;
    }

    if (session.policy().mode == diagnostics::PolicyMode::strict) {
        session.emit(
            {.severity = diagnostics::Severity::error,
             .original_severity = diagnostics::Severity::error,
             .category = diagnostics::Category::authoring,
             .localization = {.pass_ordinal = pass_ordinal, .stage = "reflection", .resource = {}},
             .frame_index = 0,
             .timestamp_ns = 0,
             .message = "Empty reflection contract (strict mode)",
             .evidence = diagnostics::ReflectionEvidence{.stage = "vertex+fragment",
                                                         .resource_summary = {},
                                                         .merge_conflicts = {}},
             .session_identity = std::nullopt});
        verdict.result = diagnostics::VerdictResult::fail;
        verdict.findings.push_back(
            {.severity = diagnostics::Severity::error,
             .localization = {.pass_ordinal = pass_ordinal, .stage = "reflection", .resource = {}},
             .message = "Empty reflection contract rejected in strict mode"});

        if (session.policy().reflection_loss_is_fatal) {
            session.set_authoring_verdict(verdict);
            return "Pass " + std::to_string(pass_ordinal) +
                   ": empty reflection contract rejected in strict mode";
        }
    } else {
        session.emit(
            {.severity = diagnostics::Severity::warning,
             .original_severity = diagnostics::Severity::warning,
             .category = diagnostics::Category::authoring,
             .localization = {.pass_ordinal = pass_ordinal, .stage = "reflection", .resource = {}},
             .frame_index = 0,
             .timestamp_ns = 0,
             .message = "Empty reflection contract (compatibility mode: degraded)",
             .evidence = diagnostics::ReflectionEvidence{.stage = "vertex+fragment",
                                                         .resource_summary = {},
                                                         .merge_conflicts = {}},
             .session_identity = std::nullopt});
        if (verdict.result == diagnostics::VerdictResult::pass) {
            verdict.result = diagnostics::VerdictResult::degraded;
        }
        verdict.findings.push_back(
            {.severity = diagnostics::Severity::warning,
             .localization = {.pass_ordinal = pass_ordinal, .stage = "reflection", .resource = {}},
             .message = "Empty reflection contract — pass marked as degraded"});
    }
    return std::nullopt;
}

void emit_authoring_verdict(diagnostics::DiagnosticSession& session,
                            const diagnostics::AuthoringVerdict& verdict) {
    session.set_authoring_verdict(verdict);
    auto sev = verdict.result == diagnostics::VerdictResult::pass ? diagnostics::Severity::info
               : verdict.result == diagnostics::VerdictResult::degraded
                   ? diagnostics::Severity::warning
                   : diagnostics::Severity::error;
    auto label = verdict.result == diagnostics::VerdictResult::pass       ? "pass"
                 : verdict.result == diagnostics::VerdictResult::degraded ? "degraded"
                                                                          : "fail";
    session.emit({.severity = sev,
                  .original_severity = sev,
                  .category = diagnostics::Category::authoring,
                  .localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                                   .stage = "verdict",
                                   .resource = {}},
                  .frame_index = 0,
                  .timestamp_ns = 0,
                  .message = "Authoring verdict: " + std::string(label),
                  .evidence = {},
                  .session_identity = std::nullopt});
}

struct BuiltPassArtifacts {
    std::unique_ptr<FilterPass> pass;
    diagnostics::CompileReport compile_report;
};

auto build_filter_pass(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                       uint32_t num_sync_indices, RetroArchPreprocessor& preprocessor,
                       const ShaderPassConfig& pass_config, uint32_t pass_ordinal,
                       diagnostics::DiagnosticSession* session,
                       diagnostics::AuthoringVerdict* verdict,
                       std::string* expanded_source_material,
                       std::string* compiled_contract_material) -> Result<BuiltPassArtifacts> {
    std::unique_ptr<diagnostics::SourceProvenanceMap> provenance;
    if (session != nullptr) {
        provenance = std::make_unique<diagnostics::SourceProvenanceMap>();
    }

    auto preprocessed = preprocessor.preprocess(pass_config.shader_path, provenance.get());
    if (!preprocessed) {
        if (session != nullptr) {
            session->emit({.severity = diagnostics::Severity::error,
                           .original_severity = diagnostics::Severity::error,
                           .category = diagnostics::Category::authoring,
                           .localization = {.pass_ordinal = pass_ordinal,
                                            .stage = "preprocess",
                                            .resource = {}},
                           .frame_index = 0,
                           .timestamp_ns = 0,
                           .message = preprocessed.error().message,
                           .evidence = {},
                           .session_identity = std::nullopt});
        }
        return make_error<BuiltPassArtifacts>(preprocessed.error().code,
                                              preprocessed.error().message);
    }

    if (session != nullptr && provenance && provenance->size() > 0) {
        emit_include_graph_event(*session, pass_ordinal, pass_config.shader_path, *provenance);
        emit_pass_provenance(*session, pass_ordinal, pass_config.shader_path, *provenance);
    }

    expanded_source_material->append(pass_config.shader_path.string());
    expanded_source_material->push_back('\n');
    expanded_source_material->append(preprocessed->vertex_source);
    expanded_source_material->push_back('\n');
    expanded_source_material->append(preprocessed->fragment_source);
    expanded_source_material->push_back('\n');

    auto compile_report = std::make_unique<diagnostics::CompileReport>();

    FilterPassConfig config{
        .target_format = pass_config.framebuffer_format,
        .num_sync_indices = num_sync_indices,
        .vertex_source = preprocessed->vertex_source,
        .fragment_source = preprocessed->fragment_source,
        .shader_name = pass_config.shader_path.stem().string(),
        .filter_mode = pass_config.filter_mode,
        .mipmap = pass_config.mipmap,
        .wrap_mode = pass_config.wrap_mode,
        .parameters = preprocessed->parameters,
    };
    auto pass = GOGGLES_TRY(FilterPass::create(
        vk_ctx, shader_runtime, config, session != nullptr ? compile_report.get() : nullptr));

    if (session != nullptr) {
        emit_compile_diagnostics(*session, pass_ordinal, *compile_report, *verdict);
    }

    compiled_contract_material->append(pass_config.shader_path.string());
    compiled_contract_material->push_back('\n');
    append_reflection_signature(compiled_contract_material, pass->reflection());
    compiled_contract_material->push_back('\n');

    if (session != nullptr) {
        emit_reflection_event(*session, pass_ordinal, *pass);
        auto fatal = evaluate_reflection_gate(*session, pass_ordinal, *pass, *verdict);
        if (fatal.has_value()) {
            return make_error<BuiltPassArtifacts>(ErrorCode::shader_compile_failed, *fatal);
        }
    }

    return BuiltPassArtifacts{.pass = std::move(pass),
                              .compile_report = std::move(*compile_report)};
}

auto build_filter_pass_resolved(
    const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime, uint32_t num_sync_indices,
    RetroArchPreprocessor& preprocessor, const ShaderPassConfig& pass_config, uint32_t pass_ordinal,
    filter_chain::runtime::SourceResolver& resolver,
    const goggles_fc_import_callbacks_t* import_callbacks, diagnostics::DiagnosticSession* session,
    diagnostics::AuthoringVerdict* verdict, std::string* expanded_source_material,
    std::string* compiled_contract_material) -> Result<BuiltPassArtifacts> {
    // Resolve shader source through the resolver instead of filesystem access.
    auto shader_bytes_result =
        resolver.resolve_relative(pass_config.shader_path.parent_path(),
                                  pass_config.shader_path.filename().string(), import_callbacks);
    if (!shader_bytes_result) {
        if (session != nullptr) {
            session->emit(
                {.severity = diagnostics::Severity::error,
                 .original_severity = diagnostics::Severity::error,
                 .category = diagnostics::Category::authoring,
                 .localization = {.pass_ordinal = pass_ordinal, .stage = "resolve", .resource = {}},
                 .frame_index = 0,
                 .timestamp_ns = 0,
                 .message = shader_bytes_result.error().message,
                 .evidence = {},
                 .session_identity = std::nullopt});
        }
        return make_error<BuiltPassArtifacts>(shader_bytes_result.error().code,
                                              shader_bytes_result.error().message);
    }

    std::string shader_source(shader_bytes_result->begin(), shader_bytes_result->end());

    std::unique_ptr<diagnostics::SourceProvenanceMap> provenance;
    if (session != nullptr) {
        provenance = std::make_unique<diagnostics::SourceProvenanceMap>();
    }

    auto preprocessed = preprocessor.preprocess_source(
        shader_source, pass_config.shader_path.parent_path(), provenance.get());
    if (!preprocessed) {
        if (session != nullptr) {
            session->emit({.severity = diagnostics::Severity::error,
                           .original_severity = diagnostics::Severity::error,
                           .category = diagnostics::Category::authoring,
                           .localization = {.pass_ordinal = pass_ordinal,
                                            .stage = "preprocess",
                                            .resource = {}},
                           .frame_index = 0,
                           .timestamp_ns = 0,
                           .message = preprocessed.error().message,
                           .evidence = {},
                           .session_identity = std::nullopt});
        }
        return make_error<BuiltPassArtifacts>(preprocessed.error().code,
                                              preprocessed.error().message);
    }

    if (session != nullptr && provenance && provenance->size() > 0) {
        emit_include_graph_event(*session, pass_ordinal, pass_config.shader_path, *provenance);
        emit_pass_provenance(*session, pass_ordinal, pass_config.shader_path, *provenance);
    }

    expanded_source_material->append(pass_config.shader_path.string());
    expanded_source_material->push_back('\n');
    expanded_source_material->append(preprocessed->vertex_source);
    expanded_source_material->push_back('\n');
    expanded_source_material->append(preprocessed->fragment_source);
    expanded_source_material->push_back('\n');

    auto compile_report = std::make_unique<diagnostics::CompileReport>();

    FilterPassConfig config{
        .target_format = pass_config.framebuffer_format,
        .num_sync_indices = num_sync_indices,
        .vertex_source = preprocessed->vertex_source,
        .fragment_source = preprocessed->fragment_source,
        .shader_name = pass_config.shader_path.stem().string(),
        .filter_mode = pass_config.filter_mode,
        .mipmap = pass_config.mipmap,
        .wrap_mode = pass_config.wrap_mode,
        .parameters = preprocessed->parameters,
    };
    auto pass = GOGGLES_TRY(FilterPass::create(
        vk_ctx, shader_runtime, config, session != nullptr ? compile_report.get() : nullptr));

    if (session != nullptr) {
        emit_compile_diagnostics(*session, pass_ordinal, *compile_report, *verdict);
    }

    compiled_contract_material->append(pass_config.shader_path.string());
    compiled_contract_material->push_back('\n');
    append_reflection_signature(compiled_contract_material, pass->reflection());
    compiled_contract_material->push_back('\n');

    if (session != nullptr) {
        emit_reflection_event(*session, pass_ordinal, *pass);
        auto fatal = evaluate_reflection_gate(*session, pass_ordinal, *pass, *verdict);
        if (fatal.has_value()) {
            return make_error<BuiltPassArtifacts>(ErrorCode::shader_compile_failed, *fatal);
        }
    }

    return BuiltPassArtifacts{.pass = std::move(pass),
                              .compile_report = std::move(*compile_report)};
}

} // namespace

auto ChainBuilder::build(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                         uint32_t num_sync_indices, TextureLoader& texture_loader,
                         const std::filesystem::path& preset_path,
                         diagnostics::DiagnosticSession* session) -> Result<CompiledChain> {
    GOGGLES_PROFILE_FUNCTION();

    PresetParser parser;
    auto preset_result = parser.load(preset_path);
    if (!preset_result) {
        if (session != nullptr) {
            session->emit(
                {.severity = diagnostics::Severity::error,
                 .original_severity = diagnostics::Severity::error,
                 .category = diagnostics::Category::authoring,
                 .localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                                  .stage = "preset_parse",
                                  .resource = {}},
                 .frame_index = 0,
                 .timestamp_ns = 0,
                 .message = preset_result.error().message,
                 .evidence = {},
                 .session_identity = std::nullopt});
        }
        return make_error<CompiledChain>(preset_result.error().code, preset_result.error().message);
    }

    if (session != nullptr) {
        update_identity_field(*session, [&](diagnostics::SessionIdentity* identity) {
            identity->preset_hash = fnv1a_hash(serialize_preset(*preset_result));
        });
        emit_preset_parse_event(*session, preset_path, *preset_result);
        emit_chain_manifest(*session, *preset_result);
    }

    std::vector<std::unique_ptr<FilterPass>> new_passes;
    std::vector<diagnostics::CompileReport> compile_reports;
    std::unordered_map<std::string, size_t> new_alias_map;
    RetroArchPreprocessor preprocessor;

    diagnostics::AuthoringVerdict verdict;
    verdict.result = diagnostics::VerdictResult::pass;
    std::string expanded_source_material;
    std::string compiled_contract_material;

    for (size_t i = 0; i < preset_result->passes.size(); ++i) {
        const auto& pass_config = preset_result->passes[i];
        auto pass_ordinal = static_cast<uint32_t>(i);

        auto artifacts = GOGGLES_TRY(build_filter_pass(
            vk_ctx, shader_runtime, num_sync_indices, preprocessor, pass_config, pass_ordinal,
            session, &verdict, &expanded_source_material, &compiled_contract_material));
        compile_reports.push_back(std::move(artifacts.compile_report));

        for (const auto& override : preset_result->parameters) {
            artifacts.pass->set_parameter_override(override.name, override.value);
        }
        auto ubo_result = artifacts.pass->update_ubo_parameters();
        if (!ubo_result) {
            return make_error<CompiledChain>(ubo_result.error().code, ubo_result.error().message);
        }

        new_passes.push_back(std::move(artifacts.pass));

        if (pass_config.alias.has_value()) {
            new_alias_map[*pass_config.alias] = i;
        }
    }

    uint32_t required_history_depth = 0;
    std::unordered_set<size_t> feedback_pass_indices;
    for (const auto& pass : new_passes) {
        for (const auto& tex : pass->texture_bindings()) {
            if (auto idx = parse_original_history_index(tex.name)) {
                required_history_depth = std::max(required_history_depth, *idx + 1);
            }
            if (auto alias = parse_feedback_alias(tex.name)) {
                if (auto it = new_alias_map.find(*alias); it != new_alias_map.end()) {
                    feedback_pass_indices.insert(it->second);
                    GOGGLES_LOG_DEBUG("Detected feedback texture '{}' -> pass {} (alias '{}')",
                                      tex.name, it->second, *alias);
                }
            }
            if (auto fb_idx = parse_pass_feedback_index(tex.name)) {
                if (*fb_idx < new_passes.size()) {
                    feedback_pass_indices.insert(*fb_idx);
                    GOGGLES_LOG_DEBUG("Detected PassFeedback{} texture", *fb_idx);
                }
            }
        }
    }
    if (required_history_depth > 0) {
        required_history_depth = std::min(required_history_depth, FrameHistory::MAX_HISTORY);
        GOGGLES_LOG_DEBUG("Detected OriginalHistory usage, depth={}", required_history_depth);
    }

    auto texture_registry =
        GOGGLES_TRY(load_preset_textures(vk_ctx, texture_loader, *preset_result));

    if (session != nullptr) {
        update_identity_field(*session, [&](diagnostics::SessionIdentity* identity) {
            identity->expanded_source_hash = fnv1a_hash(expanded_source_material);
            identity->compiled_contract_hash = fnv1a_hash(compiled_contract_material);
        });
        emit_authoring_verdict(*session, verdict);
    }

    GOGGLES_LOG_INFO(
        "FilterChain loaded preset: {} ({} passes, {} textures, {} aliases, {} params)",
        preset_path.filename().string(), new_passes.size(), texture_registry.size(),
        new_alias_map.size(), preset_result->parameters.size());
    for (const auto& [alias, pass_idx] : new_alias_map) {
        GOGGLES_LOG_DEBUG("  Alias '{}' -> pass {}", alias, pass_idx);
    }

    return CompiledChain{
        .preset = std::move(*preset_result),
        .passes = std::move(new_passes),
        .compile_reports = std::move(compile_reports),
        .alias_to_pass_index = std::move(new_alias_map),
        .required_history_depth = required_history_depth,
        .texture_registry = std::move(texture_registry),
        .feedback_pass_indices = std::move(feedback_pass_indices),
    };
}

auto ChainBuilder::build(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                         uint32_t num_sync_indices, TextureLoader& texture_loader,
                         const filter_chain::runtime::ResolvedSource& resolved,
                         filter_chain::runtime::SourceResolver& resolver,
                         const goggles_fc_import_callbacks_t* import_callbacks,
                         diagnostics::DiagnosticSession* session) -> Result<CompiledChain> {
    GOGGLES_PROFILE_FUNCTION();

    PresetParser parser;
    auto preset_result = parser.load(resolved, resolver, import_callbacks);
    if (!preset_result) {
        if (session != nullptr) {
            session->emit(
                {.severity = diagnostics::Severity::error,
                 .original_severity = diagnostics::Severity::error,
                 .category = diagnostics::Category::authoring,
                 .localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                                  .stage = "preset_parse",
                                  .resource = {}},
                 .frame_index = 0,
                 .timestamp_ns = 0,
                 .message = preset_result.error().message,
                 .evidence = {},
                 .session_identity = std::nullopt});
        }
        return make_error<CompiledChain>(preset_result.error().code, preset_result.error().message);
    }

    if (session != nullptr) {
        update_identity_field(*session, [&](diagnostics::SessionIdentity* identity) {
            identity->preset_hash = fnv1a_hash(serialize_preset(*preset_result));
        });
        // Use provenance source_name for the diagnostic event rather than a filesystem path
        std::filesystem::path source_label(
            resolved.provenance.source_name.empty() ? "<memory>" : resolved.provenance.source_name);
        emit_preset_parse_event(*session, source_label, *preset_result);
        emit_chain_manifest(*session, *preset_result);
    }

    std::vector<std::unique_ptr<FilterPass>> new_passes;
    std::vector<diagnostics::CompileReport> compile_reports;
    std::unordered_map<std::string, size_t> new_alias_map;
    RetroArchPreprocessor preprocessor;

    diagnostics::AuthoringVerdict verdict;
    verdict.result = diagnostics::VerdictResult::pass;
    std::string expanded_source_material;
    std::string compiled_contract_material;

    for (size_t i = 0; i < preset_result->passes.size(); ++i) {
        const auto& pass_config = preset_result->passes[i];
        auto pass_ordinal = static_cast<uint32_t>(i);

        auto artifacts = GOGGLES_TRY(build_filter_pass_resolved(
            vk_ctx, shader_runtime, num_sync_indices, preprocessor, pass_config, pass_ordinal,
            resolver, import_callbacks, session, &verdict, &expanded_source_material,
            &compiled_contract_material));
        compile_reports.push_back(std::move(artifacts.compile_report));

        for (const auto& override : preset_result->parameters) {
            artifacts.pass->set_parameter_override(override.name, override.value);
        }
        auto ubo_result = artifacts.pass->update_ubo_parameters();
        if (!ubo_result) {
            return make_error<CompiledChain>(ubo_result.error().code, ubo_result.error().message);
        }

        new_passes.push_back(std::move(artifacts.pass));

        if (pass_config.alias.has_value()) {
            new_alias_map[*pass_config.alias] = i;
        }
    }

    uint32_t required_history_depth = 0;
    std::unordered_set<size_t> feedback_pass_indices;
    for (const auto& pass : new_passes) {
        for (const auto& tex : pass->texture_bindings()) {
            if (auto idx = parse_original_history_index(tex.name)) {
                required_history_depth = std::max(required_history_depth, *idx + 1);
            }
            if (auto alias = parse_feedback_alias(tex.name)) {
                if (auto it = new_alias_map.find(*alias); it != new_alias_map.end()) {
                    feedback_pass_indices.insert(it->second);
                    GOGGLES_LOG_DEBUG("Detected feedback texture '{}' -> pass {} (alias '{}')",
                                      tex.name, it->second, *alias);
                }
            }
            if (auto fb_idx = parse_pass_feedback_index(tex.name)) {
                if (*fb_idx < new_passes.size()) {
                    feedback_pass_indices.insert(*fb_idx);
                    GOGGLES_LOG_DEBUG("Detected PassFeedback{} texture", *fb_idx);
                }
            }
        }
    }
    if (required_history_depth > 0) {
        required_history_depth = std::min(required_history_depth, FrameHistory::MAX_HISTORY);
        GOGGLES_LOG_DEBUG("Detected OriginalHistory usage, depth={}", required_history_depth);
    }

    auto texture_registry = GOGGLES_TRY(
        load_preset_textures(vk_ctx, texture_loader, *preset_result, resolver, import_callbacks));

    if (session != nullptr) {
        update_identity_field(*session, [&](diagnostics::SessionIdentity* identity) {
            identity->expanded_source_hash = fnv1a_hash(expanded_source_material);
            identity->compiled_contract_hash = fnv1a_hash(compiled_contract_material);
        });
        emit_authoring_verdict(*session, verdict);
    }

    std::string source_display = resolved.provenance.source_name.empty()
                                     ? std::string("<memory>")
                                     : resolved.provenance.source_name;
    GOGGLES_LOG_INFO(
        "FilterChain loaded preset: {} ({} passes, {} textures, {} aliases, {} params)",
        source_display, new_passes.size(), texture_registry.size(), new_alias_map.size(),
        preset_result->parameters.size());
    for (const auto& [alias, pass_idx] : new_alias_map) {
        GOGGLES_LOG_DEBUG("  Alias '{}' -> pass {}", alias, pass_idx);
    }

    return CompiledChain{
        .preset = std::move(*preset_result),
        .passes = std::move(new_passes),
        .compile_reports = std::move(compile_reports),
        .alias_to_pass_index = std::move(new_alias_map),
        .required_history_depth = required_history_depth,
        .texture_registry = std::move(texture_registry),
        .feedback_pass_indices = std::move(feedback_pass_indices),
    };
}

auto ChainBuilder::load_preset_textures(const VulkanContext& vk_ctx, TextureLoader& texture_loader,
                                        const PresetConfig& preset)
    -> Result<std::unordered_map<std::string, LoadedTexture>> {
    GOGGLES_PROFILE_SCOPE("LoadPresetTextures");

    std::unordered_map<std::string, LoadedTexture> registry;

    for (const auto& tex_config : preset.textures) {
        TextureLoadConfig load_cfg{.generate_mipmaps = tex_config.mipmap,
                                   .linear = tex_config.linear};

        auto tex_data_result = texture_loader.load_from_file(tex_config.path, load_cfg);
        if (!tex_data_result) {
            return nonstd::make_unexpected(tex_data_result.error());
        }

        auto sampler_result = create_texture_sampler(vk_ctx, tex_config);
        if (!sampler_result) {
            auto& loaded = tex_data_result.value();
            if (loaded.view) {
                vk_ctx.device.destroyImageView(loaded.view);
            }
            if (loaded.memory) {
                vk_ctx.device.freeMemory(loaded.memory);
            }
            if (loaded.image) {
                vk_ctx.device.destroyImage(loaded.image);
            }
            return nonstd::make_unexpected(sampler_result.error());
        }
        auto sampler = sampler_result.value();

        auto texture_data = tex_data_result.value();
        registry[tex_config.name] = LoadedTexture{.data = texture_data, .sampler = sampler};

        GOGGLES_LOG_DEBUG("Loaded texture '{}' from {}", tex_config.name,
                          tex_config.path.filename().string());
    }
    return registry;
}

auto ChainBuilder::load_preset_textures(const VulkanContext& vk_ctx, TextureLoader& texture_loader,
                                        const PresetConfig& preset,
                                        filter_chain::runtime::SourceResolver& resolver,
                                        const goggles_fc_import_callbacks_t* import_callbacks)
    -> Result<std::unordered_map<std::string, LoadedTexture>> {
    GOGGLES_PROFILE_SCOPE("LoadPresetTextures");

    std::unordered_map<std::string, LoadedTexture> registry;

    for (const auto& tex_config : preset.textures) {
        TextureLoadConfig load_cfg{.generate_mipmaps = tex_config.mipmap,
                                   .linear = tex_config.linear};

        // Resolve texture data through the resolver instead of filesystem access.
        auto tex_bytes_result = resolver.resolve_relative(
            tex_config.path.parent_path(), tex_config.path.filename().string(), import_callbacks);
        if (!tex_bytes_result) {
            return nonstd::make_unexpected(tex_bytes_result.error());
        }

        auto tex_data_result = texture_loader.load_from_bytes(
            tex_bytes_result->data(), tex_bytes_result->size(), tex_config.name, load_cfg);
        if (!tex_data_result) {
            return nonstd::make_unexpected(tex_data_result.error());
        }

        auto sampler_result = create_texture_sampler(vk_ctx, tex_config);
        if (!sampler_result) {
            auto& loaded = tex_data_result.value();
            if (loaded.view) {
                vk_ctx.device.destroyImageView(loaded.view);
            }
            if (loaded.memory) {
                vk_ctx.device.freeMemory(loaded.memory);
            }
            if (loaded.image) {
                vk_ctx.device.destroyImage(loaded.image);
            }
            return nonstd::make_unexpected(sampler_result.error());
        }
        auto sampler = sampler_result.value();

        auto texture_data = tex_data_result.value();
        registry[tex_config.name] = LoadedTexture{.data = texture_data, .sampler = sampler};

        GOGGLES_LOG_DEBUG("Loaded texture '{}' via resolver", tex_config.name);
    }
    return registry;
}

auto ChainBuilder::create_texture_sampler(const VulkanContext& vk_ctx, const TextureConfig& config)
    -> Result<vk::Sampler> {
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

    auto [result, sampler] = vk_ctx.device.createSampler(sampler_info);
    if (result != vk::Result::eSuccess) {
        return make_error<vk::Sampler>(ErrorCode::vulkan_init_failed,
                                       "Failed to create texture sampler: " +
                                           vk::to_string(result));
    }
    return Result<vk::Sampler>{sampler};
}

} // namespace goggles::render
