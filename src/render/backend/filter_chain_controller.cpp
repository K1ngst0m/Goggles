#include "filter_chain_controller.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <goggles/profiling.hpp>
#include <limits>
#include <string>
#include <string_view>
#include <util/job_system.hpp>
#include <util/logging.hpp>
#include <vector>

namespace goggles::render::backend_internal {

namespace {

// ---------------------------------------------------------------------------
// Slot-level helper types
// ---------------------------------------------------------------------------

struct ControlSnapshot {
    goggles::fc::FilterControlId control_id = 0;
    float value = 0.0F;
};

// ---------------------------------------------------------------------------
// Path / encoding helpers
// ---------------------------------------------------------------------------

auto to_utf8_bytes(const std::filesystem::path& path) -> std::string {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.c_str()), utf8.size()};
}

// ---------------------------------------------------------------------------
// Stage / control translation helpers
// ---------------------------------------------------------------------------

auto to_filter_stage(uint32_t fc_stage) -> goggles::fc::FilterControlStage {
    switch (fc_stage) {
    case GOGGLES_FC_STAGE_PRECHAIN:
        return goggles::fc::FilterControlStage::prechain;
    case GOGGLES_FC_STAGE_EFFECT:
    default:
        return goggles::fc::FilterControlStage::effect;
    }
}

auto make_control_id_from_info(const goggles_fc_control_info_t& info)
    -> goggles::fc::FilterControlId {
    const std::string_view name{info.name.data != nullptr ? info.name.data : "", info.name.size};
    return goggles::fc::make_filter_control_id(to_filter_stage(info.stage), name);
}

auto stage_mask_from_policy(bool prechain_enabled, bool effect_stage_enabled) -> uint32_t {
    uint32_t mask = GOGGLES_FC_STAGE_MASK_POSTCHAIN;
    if (prechain_enabled) {
        mask |= GOGGLES_FC_STAGE_MASK_PRECHAIN;
    }
    if (effect_stage_enabled) {
        mask |= GOGGLES_FC_STAGE_MASK_EFFECT;
    }
    return mask;
}

auto to_filter_descriptor(const goggles_fc_control_info_t& info)
    -> goggles::fc::FilterControlDescriptor {
    const auto stage = to_filter_stage(info.stage);
    std::string name = info.name.data != nullptr ? std::string(info.name.data, info.name.size) : "";
    return goggles::fc::FilterControlDescriptor{
        .control_id = goggles::fc::make_filter_control_id(stage, name),
        .stage = stage,
        .name = std::move(name),
        .description = info.description.data != nullptr
                           ? std::optional<std::string>{std::string(info.description.data,
                                                                    info.description.size)}
                           : std::nullopt,
        .current_value = info.current_value,
        .default_value = info.default_value,
        .min_value = info.min_value,
        .max_value = info.max_value,
        .step = info.step,
    };
}

// ---------------------------------------------------------------------------
// Log callback (wired during slot initialization)
// ---------------------------------------------------------------------------

void log_callback(const goggles_fc_log_message_t* message, void* /* user_data */) {
    if (message == nullptr) {
        return;
    }

    const std::string_view domain{message->domain.data,
                                  message->domain.data != nullptr ? message->domain.size : 0};
    const std::string_view msg{message->message.data,
                               message->message.data != nullptr ? message->message.size : 0};

    switch (message->level) {
    case GOGGLES_FC_LOG_LEVEL_TRACE:
        GOGGLES_LOG_TRACE("[fc:{}] {}", domain, msg);
        break;
    case GOGGLES_FC_LOG_LEVEL_DEBUG:
        GOGGLES_LOG_DEBUG("[fc:{}] {}", domain, msg);
        break;
    case GOGGLES_FC_LOG_LEVEL_INFO:
        GOGGLES_LOG_INFO("[fc:{}] {}", domain, msg);
        break;
    case GOGGLES_FC_LOG_LEVEL_WARN:
        GOGGLES_LOG_WARN("[fc:{}] {}", domain, msg);
        break;
    case GOGGLES_FC_LOG_LEVEL_ERROR:
    case GOGGLES_FC_LOG_LEVEL_CRITICAL:
        GOGGLES_LOG_ERROR("[fc:{}] {}", domain, msg);
        break;
    default:
        GOGGLES_LOG_INFO("[fc:{}] {}", domain, msg);
        break;
    }
}

// ---------------------------------------------------------------------------
// FilterChainSlot helpers
// ---------------------------------------------------------------------------

auto make_chain_create_info(const FilterChainController::FilterChainSlot& slot)
    -> goggles_fc_chain_create_info_t {
    auto chain_info = goggles_fc_chain_create_info_init();
    chain_info.target_format = slot.target_format;
    chain_info.frames_in_flight = slot.frames_in_flight;
    chain_info.initial_stage_mask = slot.stage_mask;
    chain_info.initial_prechain_resolution.width = slot.prechain_width;
    chain_info.initial_prechain_resolution.height = slot.prechain_height;
    return chain_info;
}

auto snapshot_slot_controls(const FilterChainController::FilterChainSlot& slot)
    -> Result<std::vector<ControlSnapshot>> {
    if (!slot.chain) {
        return std::vector<ControlSnapshot>{};
    }

    auto count_result = slot.chain.get_control_count();
    if (!count_result) {
        return nonstd::make_unexpected(count_result.error());
    }

    std::vector<ControlSnapshot> controls;
    controls.reserve(count_result.value());
    for (uint32_t i = 0; i < count_result.value(); ++i) {
        auto info_result = slot.chain.get_control_info(i);
        if (!info_result) {
            return nonstd::make_unexpected(info_result.error());
        }
        controls.push_back(ControlSnapshot{
            .control_id = make_control_id_from_info(*info_result),
            .value = info_result->current_value,
        });
    }

    return controls;
}

auto resolve_slot_control_index(const FilterChainController::FilterChainSlot& slot,
                                goggles::fc::FilterControlId control_id) -> Result<uint32_t> {
    if (!slot.chain) {
        return make_error<uint32_t>(ErrorCode::vulkan_init_failed, "Chain not initialized");
    }

    auto count_result = slot.chain.get_control_count();
    if (!count_result) {
        return nonstd::make_unexpected(count_result.error());
    }

    for (uint32_t i = 0; i < count_result.value(); ++i) {
        auto info_result = slot.chain.get_control_info(i);
        if (!info_result) {
            return nonstd::make_unexpected(info_result.error());
        }
        if (make_control_id_from_info(*info_result) == control_id) {
            return i;
        }
    }

    return make_error<uint32_t>(ErrorCode::invalid_data, "Control id not found on active chain");
}

auto apply_slot_controls(FilterChainController::FilterChainSlot& slot,
                         const std::vector<ControlSnapshot>& controls) -> Result<void> {
    for (const auto& control : controls) {
        auto index_result = resolve_slot_control_index(slot, control.control_id);
        if (!index_result) {
            continue;
        }

        auto result = slot.chain.set_control_value_f32(*index_result, control.value);
        if (!result) {
            return nonstd::make_unexpected(result.error());
        }
    }

    return {};
}

auto rebuild_slot_chain(FilterChainController::FilterChainSlot& slot, bool preserve_controls)
    -> Result<void> {
    if (!slot.program) {
        return {};
    }

    auto controls_result =
        preserve_controls ? snapshot_slot_controls(slot)
                          : Result<std::vector<ControlSnapshot>>{std::vector<ControlSnapshot>{}};
    if (!controls_result) {
        return nonstd::make_unexpected(controls_result.error());
    }

    auto chain_info = make_chain_create_info(slot);
    auto chain_result =
        goggles::filter_chain::Chain::create(slot.device, slot.program, &chain_info);
    if (!chain_result) {
        return nonstd::make_unexpected(chain_result.error());
    }

    auto new_chain = std::move(chain_result.value());
    auto old_chain = std::move(slot.chain);
    slot.chain = std::move(new_chain);

    auto apply_result = apply_slot_controls(slot, *controls_result);
    if (!apply_result) {
        slot.chain = std::move(old_chain);
        return nonstd::make_unexpected(apply_result.error());
    }

    return {};
}

auto install_slot_program(FilterChainController::FilterChainSlot& slot,
                          goggles::filter_chain::Program program, bool preserve_controls)
    -> Result<void> {
    auto old_program = std::move(slot.program);
    slot.program = std::move(program);

    auto rebuild_result = rebuild_slot_chain(slot, preserve_controls);
    if (!rebuild_result) {
        slot.program = std::move(old_program);
        return nonstd::make_unexpected(rebuild_result.error());
    }

    return {};
}

auto initialize_slot(FilterChainController::FilterChainSlot& slot,
                     const FilterChainController::VulkanDeviceInfo& device_info) -> Result<void> {
    auto instance_info = goggles_fc_instance_create_info_init();
    instance_info.log_callback = &log_callback;
    instance_info.log_user_data = nullptr;

    auto instance_result = goggles::filter_chain::Instance::create(&instance_info);
    if (!instance_result) {
        return nonstd::make_unexpected(instance_result.error());
    }
    slot.instance = std::move(instance_result.value());

    auto dev_info = goggles_fc_vk_device_create_info_init();
    dev_info.physical_device = device_info.physical_device;
    dev_info.device = device_info.device;
    dev_info.graphics_queue = device_info.graphics_queue;
    dev_info.graphics_queue_family_index = device_info.graphics_queue_family_index;
    if (!device_info.cache_dir.empty()) {
        dev_info.cache_dir.data = device_info.cache_dir.c_str();
        dev_info.cache_dir.size = device_info.cache_dir.size();
    }

    auto device_result = goggles::filter_chain::Device::create(slot.instance, &dev_info);
    if (!device_result) {
        return nonstd::make_unexpected(device_result.error());
    }
    slot.device = std::move(device_result.value());

    return {};
}

auto load_preset_into_slot(FilterChainController::FilterChainSlot& slot,
                           const std::filesystem::path& preset_path,
                           const FilterChainController::ChainConfig& config) -> Result<void> {
    slot.target_format = config.target_format;
    slot.frames_in_flight = config.frames_in_flight;
    slot.stage_mask = config.initial_stage_mask;
    slot.prechain_width = config.initial_prechain_width;
    slot.prechain_height = config.initial_prechain_height;

    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;

    const auto path_utf8 = to_utf8_bytes(preset_path);
    source.path.data = path_utf8.c_str();
    source.path.size = path_utf8.size();

    auto program_result = goggles::filter_chain::Program::create(slot.device, &source);
    if (!program_result) {
        return nonstd::make_unexpected(program_result.error());
    }
    return install_slot_program(slot, std::move(program_result.value()),
                                /*preserve_controls=*/false);
}

auto load_passthrough_into_slot(FilterChainController::FilterChainSlot& slot,
                                const FilterChainController::ChainConfig& config) -> Result<void> {
    slot.target_format = config.target_format;
    slot.frames_in_flight = config.frames_in_flight;
    slot.stage_mask = config.initial_stage_mask;
    slot.prechain_width = config.initial_prechain_width;
    slot.prechain_height = config.initial_prechain_height;

    // Empty path: the runtime interprets this as passthrough.
    auto source = goggles_fc_preset_source_init();
    source.kind = GOGGLES_FC_PRESET_SOURCE_FILE;
    source.path.data = "";
    source.path.size = 0;

    auto program_result = goggles::filter_chain::Program::create(slot.device, &source);
    if (!program_result) {
        return nonstd::make_unexpected(program_result.error());
    }
    return install_slot_program(slot, std::move(program_result.value()),
                                /*preserve_controls=*/false);
}

void shutdown_slot(FilterChainController::FilterChainSlot& slot) {
    slot.chain = {};
    slot.program = {};
    slot.device = {};
    slot.instance = {};
}

auto record_slot(FilterChainController::FilterChainSlot& slot,
                 const FilterChainController::RecordParams& params) -> Result<void> {
    if (!slot.chain) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Chain not initialized");
    }

    auto info = goggles_fc_record_info_vk_init();
    info.command_buffer = params.command_buffer;
    info.source_image = params.source_image;
    info.source_view = params.source_view;
    info.source_extent.width = params.source_width;
    info.source_extent.height = params.source_height;
    info.target_view = params.target_view;
    info.target_extent.width = params.target_width;
    info.target_extent.height = params.target_height;
    info.frame_index = params.frame_index;
    info.scale_mode = params.scale_mode;
    info.integer_scale = params.integer_scale;

    auto result = slot.chain.record_vk(&info);
    if (!result) {
        return nonstd::make_unexpected(result.error());
    }
    return {};
}

// ---------------------------------------------------------------------------
// Controller-level helpers using FilterChainSlot
// ---------------------------------------------------------------------------

auto snapshot_adapter_controls(const FilterChainController::FilterChainSlot& slot)
    -> std::vector<FilterChainController::ControlOverride> {
    if (!slot.chain) {
        return {};
    }

    auto count_result = slot.chain.get_control_count();
    if (!count_result) {
        GOGGLES_LOG_WARN("Failed to snapshot filter controls: {}", count_result.error().message);
        return {};
    }

    std::vector<FilterChainController::ControlOverride> controls;
    const uint32_t count = count_result.value();
    controls.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        auto info_result = slot.chain.get_control_info(i);
        if (!info_result) {
            continue;
        }
        const auto descriptor = to_filter_descriptor(*info_result);
        controls.push_back(FilterChainController::ControlOverride{
            .control_id = descriptor.control_id,
            .value = descriptor.current_value,
        });
    }
    return controls;
}

auto snapshot_adapter_control_ids(const FilterChainController::FilterChainSlot& slot)
    -> std::vector<goggles::fc::FilterControlId> {
    if (!slot.chain) {
        return {};
    }

    auto count_result = slot.chain.get_control_count();
    if (!count_result) {
        GOGGLES_LOG_WARN("Failed to snapshot filter control ids: {}", count_result.error().message);
        return {};
    }

    std::vector<goggles::fc::FilterControlId> control_ids;
    const uint32_t count = count_result.value();
    control_ids.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        auto info_result = slot.chain.get_control_info(i);
        if (!info_result) {
            continue;
        }
        control_ids.push_back(to_filter_descriptor(*info_result).control_id);
    }
    return control_ids;
}

auto apply_adapter_controls(FilterChainController::FilterChainSlot& slot,
                            const std::vector<FilterChainController::ControlOverride>& controls,
                            const char* failure_prefix) -> Result<void> {
    const auto active_control_ids = snapshot_adapter_control_ids(slot);
    for (const auto& control : controls) {
        if (std::find(active_control_ids.begin(), active_control_ids.end(), control.control_id) ==
            active_control_ids.end()) {
            continue;
        }

        auto index_result = resolve_slot_control_index(slot, control.control_id);
        if (!index_result) {
            GOGGLES_LOG_WARN("{}: {}", failure_prefix, index_result.error().message);
            continue;
        }
        auto result = slot.chain.set_control_value_f32(*index_result, control.value);
        if (!result) {
            GOGGLES_LOG_WARN("{}: {}", failure_prefix, result.error().message);
        }
    }
    return {};
}

auto align_adapter_output(FilterChainController::FilterChainSlot& slot,
                          const FilterChainController::OutputTarget& output_target,
                          std::string_view operation_context) -> Result<void> {
    if (output_target.format == vk::Format::eUndefined) {
        return make_error<void>(ErrorCode::invalid_data,
                                "Authoritative output format is undefined");
    }
    if (output_target.extent.width == 0 || output_target.extent.height == 0) {
        return make_error<void>(ErrorCode::invalid_data, "Authoritative output extent is zero");
    }

    // retarget
    slot.target_format = static_cast<VkFormat>(output_target.format);
    if (slot.chain) {
        auto target_info = goggles_fc_chain_target_info_init();
        target_info.target_format = slot.target_format;
        auto retarget_result = slot.chain.retarget(&target_info);
        if (!retarget_result) {
            return make_error<void>(retarget_result.error().code,
                                    std::string("Failed to retarget ") +
                                        std::string(operation_context) + ": " +
                                        retarget_result.error().message);
        }
    }

    // resize
    if (slot.chain) {
        goggles_fc_extent_2d_t extent{.width = output_target.extent.width,
                                      .height = output_target.extent.height};
        auto resize_result = slot.chain.resize(&extent);
        if (!resize_result) {
            return make_error<void>(resize_result.error().code, std::string("Failed to resize ") +
                                                                    std::string(operation_context) +
                                                                    ": " +
                                                                    resize_result.error().message);
        }
    }

    return {};
}

auto create_and_load_slot(const FilterChainController::VulkanDeviceInfo& device_info,
                          const FilterChainController::ChainConfig& chain_config,
                          const std::filesystem::path& preset_path)
    -> Result<FilterChainController::FilterChainSlot> {
    FilterChainController::FilterChainSlot new_slot;
    GOGGLES_TRY(initialize_slot(new_slot, device_info));

    if (preset_path.empty()) {
        GOGGLES_TRY(load_passthrough_into_slot(new_slot, chain_config));
    } else {
        GOGGLES_TRY(load_preset_into_slot(new_slot, preset_path, chain_config));
    }

    return std::move(new_slot);
}

auto fallback_retire_after_frame(uint64_t frame_count) -> uint64_t {
    constexpr uint64_t MAX_FRAME = std::numeric_limits<uint64_t>::max();
    constexpr uint64_t RETIRE_DELAY =
        FilterChainController::RetiredAdapterTracker::FALLBACK_RETIRE_DELAY_FRAMES;
    return frame_count > (MAX_FRAME - RETIRE_DELAY) ? MAX_FRAME : frame_count + RETIRE_DELAY;
}

void retire_adapter_with_bounded_fallback(FilterChainController::RetiredAdapterTracker& retired,
                                          FilterChainController::FilterChainSlot retired_slot,
                                          uint64_t frame_count,
                                          const std::function<void()>& wait_all_frames) {
    if (!retired_slot.device) {
        return;
    }

    if (retired.retired_count < FilterChainController::RetiredAdapterTracker::MAX_RETIRED) {
        auto& entry = retired.retired[retired.retired_count++];
        entry.slot = std::move(retired_slot);
        entry.destroy_after_frame = fallback_retire_after_frame(frame_count);
        return;
    }

    GOGGLES_LOG_WARN("Retired adapter queue full, forcing immediate retirement");
    wait_all_frames();
    shutdown_slot(retired_slot);
}

void cleanup_retired_adapter_tracker(FilterChainController::RetiredAdapterTracker& retired,
                                     uint64_t frame_count) {
    size_t write_idx = 0;
    for (size_t i = 0; i < retired.retired_count; ++i) {
        if (frame_count >= retired.retired[i].destroy_after_frame) {
            GOGGLES_LOG_DEBUG("Destroying retired filter chain adapter");
            shutdown_slot(retired.retired[i].slot);
            retired.retired[i].destroy_after_frame = 0;
            continue;
        }

        if (write_idx != i) {
            retired.retired[write_idx].slot = std::move(retired.retired[i].slot);
            retired.retired[write_idx].destroy_after_frame = retired.retired[i].destroy_after_frame;
        }
        ++write_idx;
    }
    retired.retired_count = write_idx;
}

void shutdown_retired_adapter_tracker(FilterChainController::RetiredAdapterTracker& retired) {
    for (size_t i = 0; i < retired.retired_count; ++i) {
        shutdown_slot(retired.retired[i].slot);
        retired.retired[i].destroy_after_frame = 0;
    }
    retired.retired_count = 0;
}

} // namespace

auto FilterChainController::recreate_filter_chain(const VulkanDeviceInfo& device_info,
                                                  ChainConfig chain_config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    chain_config.initial_stage_mask =
        stage_mask_from_policy(prechain_policy_enabled, effect_stage_policy_enabled);
    chain_config.initial_prechain_width = source_resolution.width;
    chain_config.initial_prechain_height = source_resolution.height;

    authoritative_output_target = OutputTarget{
        .format = static_cast<vk::Format>(chain_config.target_format),
        .extent = vk::Extent2D{},
    };

    const auto control_state = authoritative_control_overrides.empty()
                                   ? snapshot_adapter_controls(active_slot)
                                   : authoritative_control_overrides;

    shutdown_slot(active_slot);

    auto slot_result = create_and_load_slot(device_info, chain_config, preset_path);
    if (!slot_result) {
        return nonstd::make_unexpected(slot_result.error());
    }
    active_slot = std::move(slot_result.value());

    auto controls_result = apply_adapter_controls(active_slot, control_state,
                                                  "Failed to restore filter control value");
    if (!controls_result) {
        shutdown_slot(active_slot);
        return nonstd::make_unexpected(controls_result.error());
    }

    authoritative_control_overrides = snapshot_adapter_controls(active_slot);

    return {};
}

auto FilterChainController::retarget_filter_chain(const OutputTarget& output_target)
    -> Result<void> {
    authoritative_output_target = output_target;

    if (!active_slot.chain) {
        return {};
    }

    auto retarget_result = align_adapter_output(active_slot, authoritative_output_target,
                                                "active filter chain after retarget");
    if (!retarget_result) {
        return nonstd::make_unexpected(retarget_result.error());
    }

    authoritative_control_overrides = snapshot_adapter_controls(active_slot);
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

    shutdown_slot(active_slot);
    shutdown_slot(pending_slot);
    shutdown_retired_adapter_tracker(retired_adapters);
}

void FilterChainController::load_shader_preset(const std::filesystem::path& new_preset_path,
                                               const std::function<void()>& wait_for_safe_rebuild) {
    GOGGLES_PROFILE_FUNCTION();

    preset_path = new_preset_path;

    if (!active_slot.chain) {
        GOGGLES_LOG_WARN("Filter chain adapter not initialized; preset load skipped");
        return;
    }

    // Drain in-flight GPU work before rebuilding. The slot's load path
    // destroys the old chain/program, so any frames still referencing those
    // resources must complete first.
    if (wait_for_safe_rebuild) {
        wait_for_safe_rebuild();
    }

    // For sync preset loads on the active slot, we keep using the
    // current slot's device and rebuild just the program/chain.
    auto config = ChainConfig{
        .target_format = static_cast<VkFormat>(authoritative_output_target.format),
        .frames_in_flight = 2,
        .initial_stage_mask =
            stage_mask_from_policy(prechain_policy_enabled, effect_stage_policy_enabled),
        .initial_prechain_width = source_resolution.width,
        .initial_prechain_height = source_resolution.height,
    };

    Result<void> load_result;
    if (new_preset_path.empty()) {
        load_result = load_passthrough_into_slot(active_slot, config);
    } else {
        load_result = load_preset_into_slot(active_slot, new_preset_path, config);
    }

    if (!load_result) {
        GOGGLES_LOG_WARN("Failed to load shader preset '{}': {} - falling back to passthrough",
                         new_preset_path.string(), load_result.error().message);
    } else if (new_preset_path.empty()) {
        GOGGLES_LOG_DEBUG("No shader preset specified, using passthrough mode");
    }

    // Re-apply output target alignment after preset load
    if (active_slot.chain && authoritative_output_target.format != vk::Format::eUndefined &&
        authoritative_output_target.extent.width > 0 &&
        authoritative_output_target.extent.height > 0) {
        auto align_result = align_adapter_output(active_slot, authoritative_output_target,
                                                 "filter chain after preset load");
        if (!align_result) {
            GOGGLES_LOG_WARN("Failed to align output after preset load: {}",
                             align_result.error().message);
        }
    }

    authoritative_control_overrides = snapshot_adapter_controls(active_slot);
}

auto FilterChainController::reload_shader_preset(std::filesystem::path new_preset_path,
                                                 const VulkanDeviceInfo& device_info,
                                                 ChainConfig chain_config) -> Result<void> {
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
    chain_config.initial_stage_mask =
        stage_mask_from_policy(prechain_policy_enabled, effect_stage_policy_enabled);
    chain_config.initial_prechain_width = source_resolution.width;
    chain_config.initial_prechain_height = source_resolution.height;
    const auto requested_output_target = authoritative_output_target;
    auto requested_controls = authoritative_control_overrides.empty()
                                  ? snapshot_adapter_controls(active_slot)
                                  : authoritative_control_overrides;

    pending_load_future = util::JobSystem::submit(
        [this, device_info, chain_config = std::move(chain_config),
         requested_preset_path = std::move(new_preset_path), requested_output_target,
         requested_controls = std::move(requested_controls)]() -> Result<void> {
            GOGGLES_PROFILE_SCOPE("AsyncShaderLoad");

            auto slot_result =
                create_and_load_slot(device_info, chain_config, requested_preset_path);
            if (!slot_result) {
                GOGGLES_LOG_ERROR("Failed to create filter chain adapter");
                return nonstd::make_unexpected(slot_result.error());
            }
            auto new_adapter = std::move(slot_result.value());

            GOGGLES_TRY(apply_adapter_controls(new_adapter, requested_controls,
                                               "Failed to restore filter control before swap"));

            if (requested_output_target.format != vk::Format::eUndefined &&
                requested_output_target.extent.width > 0 &&
                requested_output_target.extent.height > 0) {
                GOGGLES_TRY(align_adapter_output(new_adapter, requested_output_target,
                                                 "pending filter chain before swap"));
            }

            pending_slot = std::move(new_adapter);
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
            shutdown_slot(pending_slot);
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        } catch (...) {
            GOGGLES_LOG_ERROR("Async shader load threw unknown exception");
            shutdown_slot(pending_slot);
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }

        if (!result) {
            GOGGLES_LOG_ERROR("Async shader load failed: {}", result.error().message);
            shutdown_slot(pending_slot);
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }
    }

    // Re-apply output alignment and controls to the pending adapter
    auto controls_result =
        apply_adapter_controls(pending_slot, authoritative_control_overrides,
                               "Failed to restore filter control value before swap");
    if (!controls_result) {
        GOGGLES_LOG_ERROR("Failed to restore filter-chain runtime state before swap: {}",
                          controls_result.error().message);
        shutdown_slot(pending_slot);
        pending_chain_ready.store(false, std::memory_order_release);
        return;
    }

    if (authoritative_output_target.format != vk::Format::eUndefined &&
        authoritative_output_target.extent.width > 0 &&
        authoritative_output_target.extent.height > 0) {
        auto output_result = align_adapter_output(pending_slot, authoritative_output_target,
                                                  "pending filter chain before activation");
        if (!output_result) {
            GOGGLES_LOG_ERROR("Failed to align pending filter-chain output before swap: {}",
                              output_result.error().message);
            shutdown_slot(pending_slot);
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }
    }

    retire_adapter_with_bounded_fallback(retired_adapters, std::move(active_slot), frame_count,
                                         wait_all_frames);

    active_slot = std::move(pending_slot);

    // Re-apply authoritative prechain resolution to the swapped-in slot so it
    // matches what the controller promised via set_prechain_resolution().
    if (source_resolution.width > 0 && source_resolution.height > 0) {
        active_slot.prechain_width = source_resolution.width;
        active_slot.prechain_height = source_resolution.height;
        if (active_slot.chain) {
            goggles_fc_extent_2d_t resolution{.width = source_resolution.width,
                                              .height = source_resolution.height};
            auto prechain_result = active_slot.chain.set_prechain_resolution(&resolution);
            if (!prechain_result) {
                GOGGLES_LOG_WARN("Failed to restore prechain resolution after swap: {}",
                                 prechain_result.error().message);
            }
        }
    }

    authoritative_control_overrides = snapshot_adapter_controls(active_slot);
    preset_path = pending_preset_path;
    pending_chain_ready.store(false, std::memory_order_release);
    chain_swapped.store(true, std::memory_order_release);

    GOGGLES_LOG_INFO("Shader chain swapped: {}",
                     preset_path.empty() ? "(passthrough)" : preset_path.string());
}

void FilterChainController::cleanup_retired_adapters() {
    cleanup_retired_adapter_tracker(retired_adapters, frame_count);
}

void FilterChainController::set_stage_policy(
    bool prechain_enabled, bool effect_stage_enabled,
    const std::function<void()>& /*wait_for_safe_rebuild*/) {
    if (prechain_policy_enabled == prechain_enabled &&
        effect_stage_policy_enabled == effect_stage_enabled) {
        return;
    }

    prechain_policy_enabled = prechain_enabled;
    effect_stage_policy_enabled = effect_stage_enabled;

    if (!active_slot.chain) {
        return;
    }

    auto mask = stage_mask_from_policy(prechain_enabled, effect_stage_enabled);
    active_slot.stage_mask = mask;
    auto result = active_slot.chain.set_stage_mask(mask);
    if (!result) {
        GOGGLES_LOG_WARN("Failed to apply filter-chain stage policy: {}", result.error().message);
    }
}

void FilterChainController::set_prechain_resolution(
    vk::Extent2D resolution, const std::function<void()>& wait_for_safe_rebuild) {
    if (source_resolution == resolution) {
        return;
    }

    if (active_slot.chain && wait_for_safe_rebuild) {
        wait_for_safe_rebuild();
    }

    source_resolution = resolution;
    if (active_slot.chain) {
        active_slot.prechain_width = resolution.width;
        active_slot.prechain_height = resolution.height;
        goggles_fc_extent_2d_t fc_resolution{.width = resolution.width,
                                             .height = resolution.height};
        auto result = active_slot.chain.set_prechain_resolution(&fc_resolution);
        if (!result) {
            GOGGLES_LOG_WARN("Failed to set prechain resolution: {}", result.error().message);
        }
    }
}

auto FilterChainController::handle_resize(vk::Extent2D target_extent) -> Result<void> {
    if (!active_slot.chain) {
        return {};
    }

    goggles_fc_extent_2d_t extent{.width = target_extent.width, .height = target_extent.height};
    return active_slot.chain.resize(&extent);
}

auto FilterChainController::record(const RecordParams& record_params) -> Result<void> {
    if (!active_slot.chain) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    return record_slot(active_slot, record_params);
}

auto FilterChainController::current_prechain_resolution() const -> vk::Extent2D {
    return vk::Extent2D{active_slot.prechain_width, active_slot.prechain_height};
}

auto FilterChainController::get_chain_report() const -> goggles::Result<goggles_fc_chain_report_t> {
    if (!active_slot.chain) {
        return goggles::make_error<goggles_fc_chain_report_t>(goggles::ErrorCode::invalid_data,
                                                              "no active chain");
    }
    return active_slot.chain.get_report();
}

auto FilterChainController::list_filter_controls() const
    -> std::vector<goggles::fc::FilterControlDescriptor> {
    if (!active_slot.chain) {
        return {};
    }

    auto count_result = active_slot.chain.get_control_count();
    if (!count_result) {
        GOGGLES_LOG_WARN("Failed to list filter controls: {}", count_result.error().message);
        return {};
    }

    std::vector<goggles::fc::FilterControlDescriptor> controls;
    const uint32_t count = count_result.value();
    controls.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        auto info_result = active_slot.chain.get_control_info(i);
        if (!info_result) {
            continue;
        }
        controls.push_back(to_filter_descriptor(info_result.value()));
    }
    return controls;
}

auto FilterChainController::list_filter_controls(goggles::fc::FilterControlStage stage) const
    -> std::vector<goggles::fc::FilterControlDescriptor> {
    if (!active_slot.chain) {
        return {};
    }

    auto count_result = active_slot.chain.get_control_count();
    if (!count_result) {
        GOGGLES_LOG_WARN("Failed to list stage filter controls: {}", count_result.error().message);
        return {};
    }

    std::vector<goggles::fc::FilterControlDescriptor> controls;
    const uint32_t count = count_result.value();
    for (uint32_t i = 0; i < count; ++i) {
        auto info_result = active_slot.chain.get_control_info(i);
        if (!info_result) {
            continue;
        }
        auto descriptor = to_filter_descriptor(info_result.value());
        if (descriptor.stage == stage) {
            controls.push_back(std::move(descriptor));
        }
    }
    return controls;
}

auto FilterChainController::set_filter_control_value(goggles::fc::FilterControlId control_id,
                                                     float value) -> bool {
    if (!active_slot.chain) {
        return false;
    }

    auto index_result = resolve_slot_control_index(active_slot, control_id);
    if (!index_result) {
        GOGGLES_LOG_WARN("Failed to set filter control value: {}", index_result.error().message);
        return false;
    }
    auto result = active_slot.chain.set_control_value_f32(*index_result, value);
    if (!result) {
        GOGGLES_LOG_WARN("Failed to set filter control value: {}", result.error().message);
        return false;
    }
    authoritative_control_overrides = snapshot_adapter_controls(active_slot);
    return true;
}

auto FilterChainController::reset_filter_control_value(goggles::fc::FilterControlId control_id)
    -> bool {
    if (!active_slot.chain) {
        return false;
    }

    const auto controls = list_filter_controls();
    const auto it =
        std::find_if(controls.begin(), controls.end(), [control_id](const auto& control) {
            return control.control_id == control_id;
        });
    if (it == controls.end()) {
        GOGGLES_LOG_WARN("Failed to reset filter control value: control {} not found", control_id);
        return false;
    }

    auto index_result = resolve_slot_control_index(active_slot, control_id);
    if (!index_result) {
        GOGGLES_LOG_WARN("Failed to reset filter control value: {}", index_result.error().message);
        return false;
    }
    auto result = active_slot.chain.set_control_value_f32(*index_result, it->default_value);
    if (!result) {
        GOGGLES_LOG_WARN("Failed to reset filter control value: {}", result.error().message);
        return false;
    }
    authoritative_control_overrides = snapshot_adapter_controls(active_slot);
    return true;
}

void FilterChainController::reset_filter_controls() {
    if (!active_slot.chain) {
        return;
    }

    auto count_result = active_slot.chain.get_control_count();
    if (!count_result) {
        GOGGLES_LOG_WARN("Failed to reset filter controls: {}", count_result.error().message);
        return;
    }

    const uint32_t count = count_result.value();
    for (uint32_t i = 0; i < count; ++i) {
        auto info_result = active_slot.chain.get_control_info(i);
        if (!info_result) {
            continue;
        }
        auto descriptor = to_filter_descriptor(info_result.value());
        auto set_result = active_slot.chain.set_control_value_f32(i, info_result->default_value);
        if (!set_result) {
            GOGGLES_LOG_WARN("Failed to reset control {}: {}", i, set_result.error().message);
        }
    }

    authoritative_control_overrides = snapshot_adapter_controls(active_slot);
}

} // namespace goggles::render::backend_internal
