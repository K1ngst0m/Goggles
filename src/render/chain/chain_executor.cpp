#include "chain_executor.hpp"

#include "debug_label_scope.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <string>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

constexpr std::string_view FEEDBACK_SUFFIX = "Feedback";

auto now_ns() -> uint64_t {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

auto size_vec4_value(const SizeVec4& value) -> std::array<float, 4> {
    return {value.width, value.height, value.inv_width, value.inv_height};
}

auto semantic_classification(const FilterPass& pass, std::string_view name)
    -> diagnostics::SemanticClassification {
    for (const auto& parameter : pass.parameters()) {
        if (parameter.name == name) {
            return diagnostics::SemanticClassification::parameter;
        }
    }

    if (name == "MVP" || name == "SourceSize" || name == "OutputSize" || name == "OriginalSize" ||
        name == "FinalViewportSize" || name == "FrameCount") {
        return diagnostics::SemanticClassification::semantic;
    }

    if (name.size() > 4 && name.ends_with("Size")) {
        const auto alias_name = std::string{name.substr(0, name.size() - 4)};
        if (pass.alias_size(alias_name).has_value()) {
            return diagnostics::SemanticClassification::semantic;
        }
    }

    return diagnostics::SemanticClassification::unresolved;
}

auto semantic_value(const FilterPass& pass, std::string_view name)
    -> std::variant<float, std::array<float, 4>> {
    if (name == "SourceSize") {
        return size_vec4_value(pass.source_size());
    }
    if (name == "OutputSize") {
        return size_vec4_value(pass.output_size());
    }
    if (name == "OriginalSize") {
        return size_vec4_value(pass.original_size());
    }
    if (name == "FinalViewportSize") {
        return size_vec4_value(pass.final_viewport_size());
    }
    if (name == "FrameCount") {
        return static_cast<float>(pass.frame_count_value());
    }
    if (name.size() > 4 && name.ends_with("Size")) {
        const auto alias_name = std::string{name.substr(0, name.size() - 4)};
        if (auto alias = pass.alias_size(alias_name)) {
            return size_vec4_value(*alias);
        }
    }
    return 0.0F;
}

auto semantic_classification_name(diagnostics::SemanticClassification classification)
    -> std::string {
    switch (classification) {
    case diagnostics::SemanticClassification::parameter:
        return "parameter";
    case diagnostics::SemanticClassification::semantic:
        return "semantic";
    case diagnostics::SemanticClassification::static_value:
        return "static";
    case diagnostics::SemanticClassification::unresolved:
    default:
        return "unresolved";
    }
}

void record_timeline(diagnostics::DiagnosticSession* session, diagnostics::TimelineEventType type,
                     uint32_t pass_ordinal) {
    if (session == nullptr) {
        return;
    }

    session->record_timeline({
        .type = type,
        .pass_ordinal = pass_ordinal,
        .cpu_timestamp_ns = now_ns(),
        .gpu_duration_us = std::nullopt,
    });
}

auto timestamps_active(diagnostics::DiagnosticSession* session,
                       diagnostics::GpuTimestampPool* gpu_timestamp_pool) -> bool {
    return session != nullptr && gpu_timestamp_pool != nullptr &&
           gpu_timestamp_pool->is_available() &&
           session->policy().tier >= diagnostics::ActivationTier::tier1;
}

void flush_gpu_timestamps(diagnostics::DiagnosticSession* session,
                          diagnostics::GpuTimestampPool* gpu_timestamp_pool, vk::CommandBuffer cmd,
                          uint32_t frame_index) {
    if (!timestamps_active(session, gpu_timestamp_pool)) {
        return;
    }

    auto durations = gpu_timestamp_pool->read_results(frame_index);
    if (!durations) {
        diagnostics::DiagnosticEvent event{};
        event.severity = diagnostics::Severity::warning;
        event.original_severity = diagnostics::Severity::warning;
        event.category = diagnostics::Category::runtime;
        event.localization = {.pass_ordinal = diagnostics::LocalizationKey::CHAIN_LEVEL,
                              .stage = "timestamp",
                              .resource = {}};
        event.frame_index = frame_index;
        event.message = std::format("Failed to read GPU timestamp results for frame {}: {}; "
                                    "disabling GPU timestamps",
                                    frame_index, durations.error().message);
        session->emit(std::move(event));
        gpu_timestamp_pool->disable();
        return;
    }

    for (const auto& sample : *durations) {
        switch (sample.region) {
        case diagnostics::GpuTimestampRegion::pass:
            session->annotate_gpu_duration(diagnostics::TimelineEventType::pass_end,
                                           sample.pass_ordinal, sample.duration_us);
            break;
        case diagnostics::GpuTimestampRegion::prechain:
            session->annotate_gpu_duration(diagnostics::TimelineEventType::prechain_end,
                                           diagnostics::LocalizationKey::CHAIN_LEVEL,
                                           sample.duration_us);
            break;
        case diagnostics::GpuTimestampRegion::final_composition:
            session->annotate_gpu_duration(diagnostics::TimelineEventType::final_composition_end,
                                           diagnostics::LocalizationKey::CHAIN_LEVEL,
                                           sample.duration_us);
            break;
        }
    }
    gpu_timestamp_pool->reset_frame(cmd, frame_index);
}

void emit_strict_fallback_abort(diagnostics::DiagnosticSession* session, uint32_t pass_ordinal) {
    if (session == nullptr) {
        return;
    }

    diagnostics::DiagnosticEvent event{};
    event.severity = diagnostics::Severity::error;
    event.original_severity = diagnostics::Severity::error;
    event.category = diagnostics::Category::runtime;
    event.localization = {.pass_ordinal = pass_ordinal, .stage = "record", .resource = {}};
    event.frame_index = session->current_frame();
    event.message = std::format(
        "Strict diagnostics blocked pass {} after fallback substitution; skipping remaining "
        "effect passes",
        pass_ordinal);
    session->emit(std::move(event));
}

void initialize_feedback_images(ChainResources& resources, vk::CommandBuffer cmd) {
    for (auto& [pass_idx, feedback_fb] : resources.m_feedback_framebuffers) {
        if (!feedback_fb) {
            continue;
        }

        bool initialized = false;
        if (auto it = resources.m_feedback_initialized.find(pass_idx);
            it != resources.m_feedback_initialized.end()) {
            initialized = it->second;
        }
        if (initialized) {
            continue;
        }

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
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, init_barrier);
        resources.m_feedback_initialized[pass_idx] = true;
    }
}

template <typename Members>
void emit_semantic_events(diagnostics::DiagnosticSession& session, const FilterPass& pass,
                          uint32_t pass_ordinal, const Members& members) {
    for (const auto& member : members) {
        const auto classification = semantic_classification(pass, member.name);
        const auto value = semantic_value(pass, member.name);

        session.record_semantic({
            .pass_ordinal = pass_ordinal,
            .member_name = member.name,
            .classification = classification,
            .value = value,
            .offset = static_cast<uint32_t>(member.offset),
        });

        diagnostics::DiagnosticEvent event{};
        event.severity = classification == diagnostics::SemanticClassification::unresolved
                             ? diagnostics::Severity::warning
                             : diagnostics::Severity::info;
        event.original_severity = event.severity;
        event.category = diagnostics::Category::runtime;
        event.localization = {
            .pass_ordinal = pass_ordinal,
            .stage = "semantic",
            .resource = member.name,
        };
        event.frame_index = session.current_frame();
        event.message = std::format("Pass {} semantic '{}' classified as {}", pass_ordinal,
                                    member.name, semantic_classification_name(classification));
        event.evidence = diagnostics::SemanticEvidence{
            .member_name = member.name,
            .classification = semantic_classification_name(classification),
            .value = value,
            .offset = static_cast<uint32_t>(member.offset),
        };
        session.emit(std::move(event));

        if (classification == diagnostics::SemanticClassification::unresolved) {
            session.record_degradation({
                .pass_ordinal = pass_ordinal,
                .expected_resource = member.name,
                .substituted_resource = {},
                .frame_index = session.current_frame(),
                .type = diagnostics::DegradationType::semantic_unresolved,
            });
        }
    }
}

auto emit_binding_diagnostics(ChainResources& resources, FilterPass& pass, size_t pass_index,
                              vk::Extent2D original_extent, diagnostics::DiagnosticSession& session)
    -> bool {
    bool strict_fallback_forbidden = false;

    for (const auto& binding : pass.texture_bindings()) {
        auto status = diagnostics::BindingStatus::resolved;
        std::string resource_identity = binding.name;
        uint32_t width = original_extent.width;
        uint32_t height = original_extent.height;
        uint32_t format = 0;
        uint32_t producer_pass = diagnostics::LocalizationKey::CHAIN_LEVEL;
        std::string alias_name;

        if (binding.name.starts_with("OriginalHistory")) {
            const auto index =
                static_cast<uint32_t>(std::strtoul(binding.name.c_str() + 15, nullptr, 10));
            if (index > 0) {
                if (resources.m_frame_history.get(index - 1)) {
                    const auto extent = resources.m_frame_history.get_extent(index - 1);
                    width = extent.width;
                    height = extent.height;
                } else {
                    status = diagnostics::BindingStatus::substituted;
                    resource_identity = "Original";
                }
            } else {
                resource_identity = "Original";
            }
        } else if (binding.name.starts_with("PassOutput")) {
            const auto producer =
                static_cast<uint32_t>(std::strtoul(binding.name.c_str() + 10, nullptr, 10));
            if (producer < pass_index && resources.m_framebuffers[producer]) {
                producer_pass = producer;
                const auto extent = resources.m_framebuffers[producer]->extent();
                width = extent.width;
                height = extent.height;
                format = static_cast<uint32_t>(resources.m_framebuffers[producer]->format());
            } else {
                status = diagnostics::BindingStatus::substituted;
                resource_identity = "Source";
            }
        } else if (binding.name.starts_with("PassFeedback")) {
            const auto producer =
                static_cast<uint32_t>(std::strtoul(binding.name.c_str() + 12, nullptr, 10));
            if (auto it = resources.m_feedback_framebuffers.find(producer);
                it != resources.m_feedback_framebuffers.end() && it->second) {
                producer_pass = producer;
                const auto extent = it->second->extent();
                width = extent.width;
                height = extent.height;
                format = static_cast<uint32_t>(it->second->format());
            } else {
                status = diagnostics::BindingStatus::substituted;
                resource_identity = "Source";
            }
        } else if (auto alias_it = resources.m_alias_to_pass_index.find(binding.name);
                   alias_it != resources.m_alias_to_pass_index.end()) {
            alias_name = binding.name;
            if (alias_it->second < pass_index && resources.m_framebuffers[alias_it->second]) {
                producer_pass = static_cast<uint32_t>(alias_it->second);
                const auto extent = resources.m_framebuffers[alias_it->second]->extent();
                width = extent.width;
                height = extent.height;
                format =
                    static_cast<uint32_t>(resources.m_framebuffers[alias_it->second]->format());
            } else {
                status = diagnostics::BindingStatus::substituted;
                resource_identity = "Source";
            }
        } else if (auto texture_it = resources.m_texture_registry.find(binding.name);
                   texture_it != resources.m_texture_registry.end()) {
            width = texture_it->second.data.extent.width;
            height = texture_it->second.data.extent.height;
        } else if (!pass.has_texture_binding(binding.name) && binding.name != "Source") {
            status = diagnostics::BindingStatus::substituted;
            resource_identity = "Source";
        }

        session.record_binding({
            .pass_ordinal = static_cast<uint32_t>(pass_index),
            .binding_slot = binding.binding,
            .status = status,
            .resource_identity = resource_identity,
            .width = width,
            .height = height,
            .format = format,
            .producer_pass_ordinal = producer_pass,
            .alias_name = alias_name,
        });

        diagnostics::DiagnosticEvent event{};
        event.severity = status == diagnostics::BindingStatus::substituted
                             ? diagnostics::Severity::warning
                             : diagnostics::Severity::info;
        event.original_severity = event.severity;
        event.category = diagnostics::Category::runtime;
        event.localization = {.pass_ordinal = static_cast<uint32_t>(pass_index),
                              .stage = "bind",
                              .resource = binding.name};
        event.frame_index = session.current_frame();
        event.message = std::format("Pass {} binding '{}' resolved to {}", pass_index, binding.name,
                                    resource_identity);
        event.evidence = diagnostics::BindingEvidence{
            .resource_id = resource_identity,
            .is_fallback = status == diagnostics::BindingStatus::substituted,
            .width = width,
            .height = height,
            .format = format,
            .producer_pass = producer_pass,
            .alias_name = alias_name,
        };
        session.emit(std::move(event));

        if (status == diagnostics::BindingStatus::substituted) {
            session.record_degradation({
                .pass_ordinal = static_cast<uint32_t>(pass_index),
                .expected_resource = binding.name,
                .substituted_resource = resource_identity,
                .frame_index = session.current_frame(),
                .type = diagnostics::DegradationType::texture_fallback,
            });

            if (session.policy().mode == diagnostics::PolicyMode::strict) {
                strict_fallback_forbidden = true;
            }
        }
    }

    return strict_fallback_forbidden;
}

struct LayoutTransition {
    vk::ImageLayout from;
    vk::ImageLayout to;
};

void transition_image_layout(vk::CommandBuffer cmd, vk::Image image, LayoutTransition transition) {
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = transition.from;
    barrier.newLayout = transition.to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;
    if (transition.to == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        src_stage = vk::PipelineStageFlagBits::eFragmentShader;
        dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    cmd.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);
}

} // namespace

auto ChainExecutor::record_prechain(ChainResources& resources, vk::CommandBuffer cmd,
                                    vk::ImageView original_view, vk::Extent2D original_extent,
                                    uint32_t frame_index) -> ChainResult {
    if (resources.m_prechain_passes.empty() || resources.m_prechain_framebuffers.empty()) {
        return {.view = original_view, .extent = original_extent};
    }

    vk::ImageView current_view = original_view;
    vk::Extent2D current_extent = original_extent;

    for (size_t i = 0; i < resources.m_prechain_passes.size(); ++i) {
        auto& pass = resources.m_prechain_passes[i];
        auto& framebuffer = resources.m_prechain_framebuffers[i];
        auto output_extent = framebuffer->extent();

        transition_image_layout(
            cmd, framebuffer->image(),
            {.from = vk::ImageLayout::eUndefined, .to = vk::ImageLayout::eColorAttachmentOptimal});

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

        transition_image_layout(cmd, framebuffer->image(),
                                {.from = vk::ImageLayout::eColorAttachmentOptimal,
                                 .to = vk::ImageLayout::eShaderReadOnlyOptimal});

        GOGGLES_LOG_TRACE("Pre-chain pass {}: {}x{} -> {}x{}", i, current_extent.width,
                          current_extent.height, output_extent.width, output_extent.height);

        current_view = framebuffer->view();
        current_extent = output_extent;
    }

    return {.view = current_view, .extent = current_extent};
}

auto ChainExecutor::record_prechain_region(ChainResources& resources, vk::CommandBuffer cmd,
                                           vk::ImageView original_view,
                                           vk::Extent2D original_extent, uint32_t frame_index,
                                           diagnostics::DiagnosticSession* session,
                                           diagnostics::GpuTimestampPool* gpu_timestamp_pool)
    -> ChainResult {
    record_timeline(session, diagnostics::TimelineEventType::prechain_start,
                    diagnostics::LocalizationKey::CHAIN_LEVEL);
    if (timestamps_active(session, gpu_timestamp_pool)) {
        gpu_timestamp_pool->write_prechain_timestamp(cmd, frame_index, true);
    }
    auto result = record_prechain(resources, cmd, original_view, original_extent, frame_index);
    if (timestamps_active(session, gpu_timestamp_pool)) {
        gpu_timestamp_pool->write_prechain_timestamp(cmd, frame_index, false);
    }
    record_timeline(session, diagnostics::TimelineEventType::prechain_end,
                    diagnostics::LocalizationKey::CHAIN_LEVEL);
    return result;
}

void ChainExecutor::record_postchain(ChainResources& resources, vk::CommandBuffer cmd,
                                     vk::ImageView source_view, vk::Extent2D source_extent,
                                     vk::ImageView target_view, vk::Extent2D target_extent,
                                     uint32_t frame_index, ScaleMode scale_mode,
                                     uint32_t integer_scale) {
    if (resources.m_postchain_passes.empty()) {
        return;
    }

    vk::ImageView current_view = source_view;
    vk::Extent2D current_extent = source_extent;

    for (size_t i = 0; i < resources.m_postchain_passes.size(); ++i) {
        auto& pass = resources.m_postchain_passes[i];
        bool is_final = (i == resources.m_postchain_passes.size() - 1);

        vk::ImageView pass_target;
        vk::Extent2D pass_output_extent;
        vk::Format pass_format;

        if (is_final) {
            pass_target = target_view;
            pass_output_extent = target_extent;
            pass_format = resources.m_swapchain_format;
        } else {
            auto& framebuffer = resources.m_postchain_framebuffers[i];
            pass_target = framebuffer->view();
            pass_output_extent = framebuffer->extent();
            pass_format = framebuffer->format();

            transition_image_layout(cmd, framebuffer->image(),
                                    {.from = vk::ImageLayout::eUndefined,
                                     .to = vk::ImageLayout::eColorAttachmentOptimal});
        }

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.source_extent = current_extent;
        ctx.output_extent = pass_output_extent;
        ctx.target_image_view = pass_target;
        ctx.target_format = pass_format;
        ctx.source_texture = current_view;
        ctx.original_texture = source_view;
        ctx.scale_mode = scale_mode;
        ctx.integer_scale = integer_scale;

        pass->record(cmd, ctx);

        if (!is_final) {
            auto& framebuffer = resources.m_postchain_framebuffers[i];

            transition_image_layout(cmd, framebuffer->image(),
                                    {.from = vk::ImageLayout::eColorAttachmentOptimal,
                                     .to = vk::ImageLayout::eShaderReadOnlyOptimal});

            current_view = framebuffer->view();
            current_extent = pass_output_extent;
        }
    }
}

void ChainExecutor::record_final_composition(ChainResources& resources, vk::CommandBuffer cmd,
                                             vk::ImageView source_view, vk::Extent2D source_extent,
                                             vk::ImageView target_view, vk::Extent2D target_extent,
                                             uint32_t frame_index, ScaleMode scale_mode,
                                             uint32_t integer_scale,
                                             diagnostics::DiagnosticSession* session,
                                             diagnostics::GpuTimestampPool* gpu_timestamp_pool) {
    record_timeline(session, diagnostics::TimelineEventType::final_composition_start,
                    diagnostics::LocalizationKey::CHAIN_LEVEL);
    if (timestamps_active(session, gpu_timestamp_pool)) {
        gpu_timestamp_pool->write_final_composition_timestamp(cmd, frame_index, true);
    }
    record_postchain(resources, cmd, source_view, source_extent, target_view, target_extent,
                     frame_index, scale_mode, integer_scale);
    if (timestamps_active(session, gpu_timestamp_pool)) {
        gpu_timestamp_pool->write_final_composition_timestamp(cmd, frame_index, false);
    }
    record_timeline(session, diagnostics::TimelineEventType::final_composition_end,
                    diagnostics::LocalizationKey::CHAIN_LEVEL);
}

void ChainExecutor::record(ChainResources& resources, vk::CommandBuffer cmd,
                           vk::Image original_image, vk::ImageView original_view,
                           vk::Extent2D original_extent, vk::ImageView swapchain_view,
                           vk::Extent2D viewport_extent, uint32_t frame_index, ScaleMode scale_mode,
                           uint32_t integer_scale, diagnostics::DiagnosticSession* session,
                           diagnostics::GpuTimestampPool* gpu_timestamp_pool) {
    GOGGLES_PROFILE_FUNCTION();

    resources.m_last_scale_mode = scale_mode;
    resources.m_last_integer_scale = integer_scale;
    resources.m_last_source_extent = original_extent;

    flush_gpu_timestamps(session, gpu_timestamp_pool, cmd, frame_index);

    vk::Image effective_original_image = original_image;
    vk::ImageView effective_original_view = original_view;
    vk::Extent2D effective_original_extent = original_extent;
    if (resources.m_prechain_enabled.load(std::memory_order_relaxed)) {
        GOGGLES_MUST(resources.ensure_prechain_passes(original_extent));
        record_timeline(session, diagnostics::TimelineEventType::allocation,
                        diagnostics::LocalizationKey::CHAIN_LEVEL);
        auto prechain_result =
            record_prechain_region(resources, cmd, original_view, original_extent, frame_index,
                                   session, gpu_timestamp_pool);
        if (!resources.m_prechain_framebuffers.empty()) {
            effective_original_image = resources.m_prechain_framebuffers.back()->image();
        }
        effective_original_view = prechain_result.view;
        effective_original_extent = prechain_result.extent;
    }

    GOGGLES_MUST(resources.ensure_frame_history(effective_original_extent));

    if (resources.m_passes.empty() || resources.m_bypass_enabled.load(std::memory_order_relaxed)) {
        record_final_composition(resources, cmd, effective_original_view, effective_original_extent,
                                 swapchain_view, viewport_extent, frame_index, scale_mode,
                                 integer_scale, session, gpu_timestamp_pool);
        resources.m_frame_count++;
        return;
    }

    auto vp = calculate_viewport(effective_original_extent.width, effective_original_extent.height,
                                 viewport_extent.width, viewport_extent.height, scale_mode,
                                 integer_scale);
    GOGGLES_MUST(resources.ensure_framebuffers(
        {.viewport = viewport_extent, .source = effective_original_extent}, {vp.width, vp.height}));

    initialize_feedback_images(resources, cmd);

    vk::ImageView source_view = effective_original_view;
    vk::Extent2D source_extent = effective_original_extent;

    for (size_t i = 0; i < resources.m_passes.size(); ++i) {
        auto& pass = resources.m_passes[i];

        vk::ImageView target_view = resources.m_framebuffers[i]->view();
        vk::Extent2D target_extent = resources.m_framebuffers[i]->extent();
        vk::Format target_format = resources.m_framebuffers[i]->format();

        transition_image_layout(
            cmd, resources.m_framebuffers[i]->image(),
            {.from = vk::ImageLayout::eUndefined, .to = vk::ImageLayout::eColorAttachmentOptimal});

        pass->set_source_size(source_extent.width, source_extent.height);
        pass->set_output_size(target_extent.width, target_extent.height);
        pass->set_original_size(effective_original_extent.width, effective_original_extent.height);
        pass->set_frame_count(resources.m_frame_count,
                              resources.m_preset.passes[i].frame_count_mod);
        pass->set_final_viewport_size(vp.width, vp.height);
        pass->set_rotation(0);

        if (session != nullptr) {
            const auto& reflection = pass->reflection();
            if (reflection.ubo) {
                emit_semantic_events(*session, *pass, static_cast<uint32_t>(i),
                                     reflection.ubo->members);
            }
            if (reflection.push_constants) {
                emit_semantic_events(*session, *pass, static_cast<uint32_t>(i),
                                     reflection.push_constants->members);
            }
        }

        const auto bind_result =
            bind_pass_textures(resources, *pass, i, effective_original_view,
                               effective_original_extent, source_view, session);
        if (bind_result.strict_fallback_forbidden) {
            emit_strict_fallback_abort(session, static_cast<uint32_t>(i));

            record_final_composition(resources, cmd, source_view, source_extent, swapchain_view,
                                     viewport_extent, frame_index, scale_mode, integer_scale,
                                     session, gpu_timestamp_pool);
            resources.m_frame_count++;
            return;
        }

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

        record_timeline(session, diagnostics::TimelineEventType::pass_start,
                        static_cast<uint32_t>(i));
        if (timestamps_active(session, gpu_timestamp_pool)) {
            gpu_timestamp_pool->write_pass_timestamp(cmd, frame_index, static_cast<uint32_t>(i),
                                                     true);
        }
        {
            const ScopedDebugLabel debug_label(cmd,
                                               std::format("Pass {} {}", i, pass->shader_name()),
                                               {0.18F, 0.46F, 0.92F, 1.0F});
            pass->record(cmd, ctx);
        }
        if (timestamps_active(session, gpu_timestamp_pool)) {
            gpu_timestamp_pool->write_pass_timestamp(cmd, frame_index, static_cast<uint32_t>(i),
                                                     false);
        }
        record_timeline(session, diagnostics::TimelineEventType::pass_end,
                        static_cast<uint32_t>(i));

        transition_image_layout(cmd, resources.m_framebuffers[i]->image(),
                                {.from = vk::ImageLayout::eColorAttachmentOptimal,
                                 .to = vk::ImageLayout::eShaderReadOnlyOptimal});

        source_view = resources.m_framebuffers[i]->view();
        source_extent = resources.m_framebuffers[i]->extent();
    }

    record_final_composition(resources, cmd, source_view, source_extent, swapchain_view,
                             viewport_extent, frame_index, scale_mode, integer_scale, session,
                             gpu_timestamp_pool);

    if (resources.m_frame_history.is_initialized()) {
        {
            const ScopedDebugLabel debug_label(cmd, "History Push", {0.18F, 0.72F, 0.33F, 1.0F});
            resources.m_frame_history.push(cmd, effective_original_image,
                                           effective_original_extent);
        }
        record_timeline(session, diagnostics::TimelineEventType::history_push,
                        diagnostics::LocalizationKey::CHAIN_LEVEL);
    }

    {
        const ScopedDebugLabel debug_label(cmd, "Feedback Copy", {0.85F, 0.52F, 0.18F, 1.0F});
        copy_feedback_framebuffers(resources, cmd);
    }
    record_timeline(session, diagnostics::TimelineEventType::feedback_copy,
                    diagnostics::LocalizationKey::CHAIN_LEVEL);
    resources.m_frame_count++;
}

auto ChainExecutor::bind_pass_textures(ChainResources& resources, FilterPass& pass,
                                       size_t pass_index, vk::ImageView original_view,
                                       vk::Extent2D original_extent, vk::ImageView source_view,
                                       diagnostics::DiagnosticSession* session)
    -> ChainExecutor::BindPassResult {
    pass.clear_alias_sizes();
    pass.clear_texture_bindings();

    pass.set_texture_binding("Source", source_view, nullptr);
    pass.set_texture_binding("Original", original_view, nullptr);

    pass.set_texture_binding("OriginalHistory0", original_view, nullptr);
    pass.set_alias_size("OriginalHistory0", original_extent.width, original_extent.height);

    for (uint32_t h = 0; h < resources.m_required_history_depth; ++h) {
        auto name = std::format("OriginalHistory{}", h + 1);
        if (auto hist_view = resources.m_frame_history.get(h)) {
            pass.set_texture_binding(name, hist_view, nullptr);
            auto ext = resources.m_frame_history.get_extent(h);
            pass.set_alias_size(name, ext.width, ext.height);
        } else {
            pass.set_texture_binding(name, original_view, nullptr);
            pass.set_alias_size(name, original_extent.width, original_extent.height);
        }
    }

    for (size_t p = 0; p < pass_index; ++p) {
        if (resources.m_framebuffers[p]) {
            auto pass_name = std::format("PassOutput{}", p);
            auto pass_extent = resources.m_framebuffers[p]->extent();
            pass.set_texture_binding(pass_name, resources.m_framebuffers[p]->view(), nullptr);
            pass.set_alias_size(pass_name, pass_extent.width, pass_extent.height);
        }
    }

    for (const auto& [fb_idx, feedback_fb] : resources.m_feedback_framebuffers) {
        auto feedback_name = std::format("PassFeedback{}", fb_idx);
        if (feedback_fb) {
            pass.set_texture_binding(feedback_name, feedback_fb->view(), nullptr);
            auto fb_extent = feedback_fb->extent();
            pass.set_alias_size(feedback_name, fb_extent.width, fb_extent.height);
        } else {
            pass.set_texture_binding(feedback_name, source_view, nullptr);
        }
    }

    for (const auto& [alias, idx] : resources.m_alias_to_pass_index) {
        if (idx < pass_index && resources.m_framebuffers[idx]) {
            pass.set_texture_binding(alias, resources.m_framebuffers[idx]->view(), nullptr);
            auto alias_extent = resources.m_framebuffers[idx]->extent();
            pass.set_alias_size(alias, alias_extent.width, alias_extent.height);
        }
        if (auto fb_it = resources.m_feedback_framebuffers.find(idx);
            fb_it != resources.m_feedback_framebuffers.end() && fb_it->second) {
            auto feedback_name = alias + std::string(FEEDBACK_SUFFIX);
            pass.set_texture_binding(feedback_name, fb_it->second->view(), nullptr);
            auto fb_extent = fb_it->second->extent();
            pass.set_alias_size(feedback_name, fb_extent.width, fb_extent.height);
        }
    }

    for (const auto& [name, tex] : resources.m_texture_registry) {
        pass.set_texture_binding(name, tex.data.view, tex.sampler);
    }

    if (session == nullptr) {
        return {};
    }

    return ChainExecutor::BindPassResult{
        .strict_fallback_forbidden =
            emit_binding_diagnostics(resources, pass, pass_index, original_extent, *session)};
}

void ChainExecutor::copy_feedback_framebuffers(ChainResources& resources, vk::CommandBuffer cmd) {
    for (auto& [pass_idx, feedback_fb] : resources.m_feedback_framebuffers) {
        if (!feedback_fb || !resources.m_framebuffers[pass_idx]) {
            continue;
        }
        auto extent = resources.m_framebuffers[pass_idx]->extent();
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
        pre[0].image = resources.m_framebuffers[pass_idx]->image();
        pre[0].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        pre[1].srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre[1].dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        pre[1].oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        pre[1].newLayout = vk::ImageLayout::eTransferDstOptimal;
        pre[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[1].image = feedback_fb->image();
        pre[1].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, pre);

        cmd.copyImage(resources.m_framebuffers[pass_idx]->image(),
                      vk::ImageLayout::eTransferSrcOptimal, feedback_fb->image(),
                      vk::ImageLayout::eTransferDstOptimal, region);

        std::array<vk::ImageMemoryBarrier, 2> post{};
        post[0].srcAccessMask = vk::AccessFlagBits::eTransferRead;
        post[0].dstAccessMask = vk::AccessFlagBits::eShaderRead;
        post[0].oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        post[0].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        post[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[0].image = resources.m_framebuffers[pass_idx]->image();
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

} // namespace goggles::render
