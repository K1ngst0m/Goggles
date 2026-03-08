#include "filter_chain_controller.hpp"

#include <chrono>
#include <limits>
#include <util/job_system.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render::backend_internal {

namespace {

auto to_chain_stage(FilterControlStage stage) -> ChainControlStage {
    switch (stage) {
    case FilterControlStage::prechain:
        return ChainControlStage::prechain;
    case FilterControlStage::effect:
    default:
        return ChainControlStage::effect;
    }
}

auto to_filter_stage(ChainControlStage stage) -> FilterControlStage {
    switch (stage) {
    case ChainControlStage::prechain:
        return FilterControlStage::prechain;
    case ChainControlStage::effect:
    case ChainControlStage::postchain:
    default:
        return FilterControlStage::effect;
    }
}

auto to_filter_descriptor(const ChainControlDescriptor& descriptor) -> FilterControlDescriptor {
    return FilterControlDescriptor{
        .control_id = descriptor.control_id,
        .stage = to_filter_stage(descriptor.stage),
        .name = descriptor.name,
        .description = descriptor.description,
        .current_value = descriptor.current_value,
        .default_value = descriptor.default_value,
        .min_value = descriptor.min_value,
        .max_value = descriptor.max_value,
        .step = descriptor.step,
    };
}

void destroy_filter_chain(FilterChainRuntime& chain, const char* failure_message) {
    if (!chain) {
        return;
    }

    auto destroy_result = chain.destroy();
    if (!destroy_result) {
        GOGGLES_LOG_WARN("{}: {}", failure_message, destroy_result.error().message);
    }
}

auto create_filter_chain_runtime(const FilterChainController::RuntimeBuildConfig& config)
    -> Result<FilterChainRuntime> {
    auto filter_chain_result = FilterChainRuntime::create(ChainCreateInfo{
        .device = config.vulkan_context.device,
        .physical_device = config.vulkan_context.physical_device,
        .graphics_queue = config.vulkan_context.graphics_queue,
        .graphics_queue_family_index = config.graphics_queue_family_index,
        .target_format = config.target_format,
        .num_sync_indices = config.num_sync_indices,
        .shader_dir = config.shader_dir,
        .cache_dir = config.cache_dir,
        .initial_prechain_resolution = config.initial_prechain_resolution,
    });
    if (!filter_chain_result) {
        return nonstd::make_unexpected(filter_chain_result.error());
    }

    return std::move(filter_chain_result.value());
}

} // namespace

auto FilterChainController::recreate_filter_chain(const RuntimeBuildConfig& config)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (filter_chain) {
        auto destroy_result = filter_chain.destroy();
        if (!destroy_result) {
            return make_error<void>(ErrorCode::unknown_error,
                                    "Failed to destroy existing filter chain: " +
                                        destroy_result.error().message);
        }
    }

    auto filter_chain_result = create_filter_chain_runtime(config);
    if (!filter_chain_result) {
        return nonstd::make_unexpected(filter_chain_result.error());
    }
    filter_chain = std::move(filter_chain_result.value());

    set_stage_policy(ChainStagePolicy{
        .prechain_enabled = prechain_policy_enabled,
        .effect_stage_enabled = effect_stage_policy_enabled,
    });

    if (!preset_path.empty()) {
        auto load_result = filter_chain.load_preset(preset_path);
        if (!load_result) {
            GOGGLES_LOG_WARN("Failed to reload shader preset after chain rebuild: {}",
                             load_result.error().message);
        }
    }

    return {};
}

void FilterChainController::shutdown(const std::function<void()>& wait_for_gpu_idle) {
    if (pending_load_future.valid()) {
        auto status = pending_load_future.wait_for(std::chrono::seconds(3));
        if (status == std::future_status::timeout) {
            GOGGLES_LOG_WARN(
                "Shader load task still running during shutdown, waiting for completion");
        }

        try {
            auto pending_result = pending_load_future.get();
            if (!pending_result) {
                GOGGLES_LOG_WARN("Pending shader load finished with error during shutdown: {}",
                                 pending_result.error().message);
            }
        } catch (const std::exception& ex) {
            GOGGLES_LOG_WARN("Pending shader load threw exception during shutdown: {}", ex.what());
        } catch (...) {
            GOGGLES_LOG_WARN("Pending shader load threw unknown exception during shutdown");
        }
    }

    pending_chain_ready.store(false, std::memory_order_release);

    wait_for_gpu_idle();

    destroy_filter_chain(filter_chain, "Filter chain destroy during shutdown failed");
    destroy_filter_chain(pending_filter_chain,
                         "Pending filter chain destroy during shutdown failed");

    for (size_t i = 0; i < deferred_count; ++i) {
        destroy_filter_chain(deferred_destroys[i].filter_chain,
                             "Deferred filter chain destroy during shutdown failed");
        deferred_destroys[i].destroy_after_frame = 0;
    }
    deferred_count = 0;
}

void FilterChainController::load_shader_preset(const std::filesystem::path& new_preset_path) {
    GOGGLES_PROFILE_FUNCTION();

    preset_path = new_preset_path;

    if (new_preset_path.empty()) {
        GOGGLES_LOG_DEBUG("No shader preset specified, using passthrough mode");
        return;
    }

    if (!filter_chain) {
        GOGGLES_LOG_WARN("Filter chain not initialized; preset load skipped");
        return;
    }

    auto load_result = filter_chain.load_preset(new_preset_path);
    if (!load_result) {
        GOGGLES_LOG_WARN("Failed to load shader preset '{}': {} - falling back to passthrough",
                         new_preset_path.string(), load_result.error().message);
    }
}

auto FilterChainController::reload_shader_preset(std::filesystem::path new_preset_path,
                                                 RuntimeBuildConfig config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (pending_chain_ready.load(std::memory_order_acquire)) {
        GOGGLES_LOG_WARN("Shader reload already pending, ignoring request");
        return {};
    }

    if (pending_load_future.valid() &&
        pending_load_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        GOGGLES_LOG_WARN("Shader compilation in progress, ignoring request");
        return {};
    }

    pending_preset_path = new_preset_path;

    pending_load_future = util::JobSystem::submit(
        [this, build_config = std::move(config), requested_preset_path = std::move(new_preset_path),
         requested_prechain_policy_enabled = prechain_policy_enabled,
         requested_effect_policy_enabled = effect_stage_policy_enabled]() -> Result<void> {
            GOGGLES_PROFILE_SCOPE("AsyncShaderLoad");

            auto pending_chain_result = create_filter_chain_runtime(build_config);
            if (!pending_chain_result) {
                GOGGLES_LOG_ERROR("Failed to create filter chain");
                return nonstd::make_unexpected(pending_chain_result.error());
            }
            auto pending_chain = std::move(pending_chain_result.value());

            auto policy_result = pending_chain.set_stage_policy(ChainStagePolicy{
                .prechain_enabled = requested_prechain_policy_enabled,
                .effect_stage_enabled = requested_effect_policy_enabled,
            });
            if (!policy_result) {
                return policy_result;
            }

            if (!requested_preset_path.empty()) {
                auto load_result = pending_chain.load_preset(requested_preset_path);
                if (!load_result) {
                    GOGGLES_LOG_ERROR("Failed to load preset '{}'", requested_preset_path.string());
                    return load_result;
                }
            }

            pending_filter_chain = std::move(pending_chain);
            pending_chain_ready.store(true, std::memory_order_release);

            GOGGLES_LOG_INFO("Shader preset compiled: {}", requested_preset_path.empty()
                                                               ? "(passthrough)"
                                                               : requested_preset_path.string());
            return {};
        });

    return {};
}

void FilterChainController::advance_frame() {
    ++frame_count;
}

void FilterChainController::check_pending_chain_swap(const std::function<void()>& wait_all_frames) {
    if (!pending_chain_ready.load(std::memory_order_acquire)) {
        return;
    }

    if (pending_load_future.valid()) {
        Result<void> result;
        try {
            result = pending_load_future.get();
        } catch (const std::exception& ex) {
            GOGGLES_LOG_ERROR("Async shader load threw exception: {}", ex.what());
            destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        } catch (...) {
            GOGGLES_LOG_ERROR("Async shader load threw unknown exception");
            destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }

        if (!result) {
            GOGGLES_LOG_ERROR("Async shader load failed: {}", result.error().message);
            destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }
    }

    const uint64_t retire_delay = 3u;
    const uint64_t retire_after_frame =
        frame_count > (std::numeric_limits<uint64_t>::max() - retire_delay)
            ? std::numeric_limits<uint64_t>::max()
            : frame_count + retire_delay;

    if (deferred_count < MAX_DEFERRED_DESTROYS) {
        auto& deferred = deferred_destroys[deferred_count++];
        deferred.filter_chain = std::move(filter_chain);
        deferred.destroy_after_frame = retire_after_frame;
    } else {
        GOGGLES_LOG_WARN("Deferred destroy queue full, destroying immediately");
        wait_all_frames();
        destroy_filter_chain(filter_chain, "Failed to destroy active filter chain");
    }

    filter_chain = std::move(pending_filter_chain);
    set_stage_policy(ChainStagePolicy{
        .prechain_enabled = prechain_policy_enabled,
        .effect_stage_enabled = effect_stage_policy_enabled,
    });
    if (filter_chain && source_resolution.width > 0 && source_resolution.height > 0) {
        auto set_result = filter_chain.set_prechain_resolution(source_resolution);
        if (!set_result) {
            GOGGLES_LOG_WARN("Failed to reapply prechain resolution after swap: {}",
                             set_result.error().message);
        }
    }
    preset_path = pending_preset_path;
    pending_chain_ready.store(false, std::memory_order_release);
    chain_swapped.store(true, std::memory_order_release);

    GOGGLES_LOG_INFO("Shader chain swapped: {}",
                     preset_path.empty() ? "(passthrough)" : preset_path.string());
}

void FilterChainController::cleanup_deferred_destroys() {
    size_t write_idx = 0;
    for (size_t i = 0; i < deferred_count; ++i) {
        if (frame_count >= deferred_destroys[i].destroy_after_frame) {
            GOGGLES_LOG_DEBUG("Destroying deferred filter chain");
            destroy_filter_chain(deferred_destroys[i].filter_chain,
                                 "Failed to destroy deferred filter chain");
            deferred_destroys[i].destroy_after_frame = 0;
        } else {
            if (write_idx != i) {
                deferred_destroys[write_idx].filter_chain =
                    std::move(deferred_destroys[i].filter_chain);
                deferred_destroys[write_idx].destroy_after_frame =
                    deferred_destroys[i].destroy_after_frame;
            }
            ++write_idx;
        }
    }
    deferred_count = write_idx;
}

void FilterChainController::set_stage_policy(const ChainStagePolicy& policy) {
    prechain_policy_enabled = policy.prechain_enabled;
    effect_stage_policy_enabled = policy.effect_stage_enabled;

    if (!filter_chain) {
        return;
    }

    auto policy_result = filter_chain.set_stage_policy(policy);
    if (!policy_result) {
        GOGGLES_LOG_WARN("Failed to apply filter-chain stage policy: {}",
                         policy_result.error().message);
    }
}

void FilterChainController::set_prechain_resolution(const PrechainResolutionConfig& config) {
    auto effective_resolution = config.requested_resolution;
    if ((effective_resolution.width == 0u) != (effective_resolution.height == 0u)) {
        const auto reference_resolution =
            resolve_initial_prechain_resolution(config.fallback_resolution);
        if (effective_resolution.width == 0u) {
            const auto scaled_width = (static_cast<uint64_t>(reference_resolution.width) *
                                           static_cast<uint64_t>(effective_resolution.height) +
                                       static_cast<uint64_t>(reference_resolution.height / 2u)) /
                                      static_cast<uint64_t>(reference_resolution.height);
            effective_resolution.width =
                std::max<uint32_t>(1u, static_cast<uint32_t>(scaled_width));
        } else {
            const auto scaled_height = (static_cast<uint64_t>(reference_resolution.height) *
                                            static_cast<uint64_t>(effective_resolution.width) +
                                        static_cast<uint64_t>(reference_resolution.width / 2u)) /
                                       static_cast<uint64_t>(reference_resolution.width);
            effective_resolution.height =
                std::max<uint32_t>(1u, static_cast<uint32_t>(scaled_height));
        }
    }

    source_resolution = effective_resolution;
    if (filter_chain && effective_resolution.width > 0u && effective_resolution.height > 0u) {
        auto set_result = filter_chain.set_prechain_resolution(effective_resolution);
        if (!set_result) {
            GOGGLES_LOG_WARN("Failed to set prechain resolution: {}", set_result.error().message);
        }
    }
}

auto FilterChainController::handle_resize(vk::Extent2D target_extent) -> Result<void> {
    if (!filter_chain) {
        return {};
    }

    return filter_chain.handle_resize(target_extent);
}

auto FilterChainController::resolve_initial_prechain_resolution(
    vk::Extent2D fallback_resolution) const -> vk::Extent2D {
    if (source_resolution.width > 0u && source_resolution.height > 0u) {
        return source_resolution;
    }
    if (fallback_resolution.width > 0u && fallback_resolution.height > 0u) {
        return fallback_resolution;
    }
    return vk::Extent2D{1u, 1u};
}

auto FilterChainController::current_prechain_resolution() const -> vk::Extent2D {
    if (filter_chain) {
        auto resolution_result = filter_chain.get_prechain_resolution();
        if (resolution_result) {
            return resolution_result.value();
        }
    }
    return source_resolution;
}

auto FilterChainController::list_filter_controls() const -> std::vector<FilterControlDescriptor> {
    if (!filter_chain) {
        return {};
    }

    auto controls_result = filter_chain.list_controls();
    if (!controls_result) {
        GOGGLES_LOG_WARN("Failed to list filter controls: {}", controls_result.error().message);
        return {};
    }

    std::vector<FilterControlDescriptor> controls;
    controls.reserve(controls_result->size());
    for (const auto& descriptor : controls_result.value()) {
        controls.push_back(to_filter_descriptor(descriptor));
    }
    return controls;
}

auto FilterChainController::list_filter_controls(FilterControlStage stage) const
    -> std::vector<FilterControlDescriptor> {
    if (!filter_chain) {
        return {};
    }

    auto controls_result = filter_chain.list_controls(to_chain_stage(stage));
    if (!controls_result) {
        GOGGLES_LOG_WARN("Failed to list stage filter controls: {}",
                         controls_result.error().message);
        return {};
    }

    std::vector<FilterControlDescriptor> controls;
    controls.reserve(controls_result->size());
    for (const auto& descriptor : controls_result.value()) {
        controls.push_back(to_filter_descriptor(descriptor));
    }
    return controls;
}

auto FilterChainController::set_filter_control_value(FilterControlId control_id, float value)
    -> bool {
    if (!filter_chain) {
        return false;
    }

    auto set_result = filter_chain.set_control_value(control_id, value);
    if (!set_result) {
        GOGGLES_LOG_WARN("Failed to set filter control value: {}", set_result.error().message);
        return false;
    }
    return set_result.value();
}

auto FilterChainController::reset_filter_control_value(FilterControlId control_id) -> bool {
    if (!filter_chain) {
        return false;
    }

    auto reset_result = filter_chain.reset_control_value(control_id);
    if (!reset_result) {
        GOGGLES_LOG_WARN("Failed to reset filter control value: {}", reset_result.error().message);
        return false;
    }
    return reset_result.value();
}

void FilterChainController::reset_filter_controls() {
    if (!filter_chain) {
        return;
    }

    auto reset_result = filter_chain.reset_all_controls();
    if (!reset_result) {
        GOGGLES_LOG_WARN("Failed to reset filter controls: {}", reset_result.error().message);
    }
}

} // namespace goggles::render::backend_internal
