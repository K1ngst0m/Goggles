#include "goggles_filter_chain.h"

#include "goggles_filter_chain.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <util/logging.hpp>
#include <vector>

namespace goggles::render {

namespace {

auto to_utf8_bytes(const std::filesystem::path& path) -> std::string {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.c_str()), utf8.size()};
}

auto chain_status_to_error_code(goggles_chain_status_t status) -> ErrorCode {
    switch (status) {
    case GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT:
    case GOGGLES_CHAIN_STATUS_NOT_FOUND:
    case GOGGLES_CHAIN_STATUS_NOT_SUPPORTED:
        return ErrorCode::invalid_data;
    case GOGGLES_CHAIN_STATUS_NOT_INITIALIZED:
        return ErrorCode::vulkan_init_failed;
    case GOGGLES_CHAIN_STATUS_PRESET_ERROR:
        return ErrorCode::shader_load_failed;
    case GOGGLES_CHAIN_STATUS_IO_ERROR:
        return ErrorCode::file_read_failed;
    case GOGGLES_CHAIN_STATUS_VULKAN_ERROR:
        return ErrorCode::vulkan_init_failed;
    case GOGGLES_CHAIN_STATUS_OUT_OF_MEMORY:
    case GOGGLES_CHAIN_STATUS_RUNTIME_ERROR:
    case GOGGLES_CHAIN_STATUS_OK:
    default:
        return ErrorCode::unknown_error;
    }
}

auto make_chain_error(const goggles_chain_t* chain, goggles_chain_status_t status,
                      std::string_view context) -> Error {
    ErrorCode error_code = chain_status_to_error_code(status);
    std::string message = std::format("{}: {}", context, goggles_chain_status_to_string(status));
    if (chain != nullptr) {
        auto last_error = goggles_chain_error_last_info_init();
        if (goggles_chain_error_last_info_get(chain, &last_error) == GOGGLES_CHAIN_STATUS_OK) {
            if (status == GOGGLES_CHAIN_STATUS_VULKAN_ERROR &&
                last_error.vk_result == VK_ERROR_DEVICE_LOST) {
                error_code = ErrorCode::vulkan_device_lost;
            }
            message += std::format(" (subsystem={}, vk_result={})", last_error.subsystem_code,
                                   last_error.vk_result);
        }
    }
    return Error{error_code, std::move(message)};
}

auto status_to_result(const goggles_chain_t* chain, goggles_chain_status_t status,
                      std::string_view context) -> Result<void> {
    if (status == GOGGLES_CHAIN_STATUS_OK) {
        return {};
    }
    return nonstd::make_unexpected(make_chain_error(chain, status, context));
}

auto to_chain_scale_mode(ChainScaleMode scale_mode) -> goggles_chain_scale_mode_t {
    switch (scale_mode) {
    case ChainScaleMode::fit:
        return GOGGLES_CHAIN_SCALE_MODE_FIT;
    case ChainScaleMode::integer:
        return GOGGLES_CHAIN_SCALE_MODE_INTEGER;
    case ChainScaleMode::stretch:
    default:
        return GOGGLES_CHAIN_SCALE_MODE_STRETCH;
    }
}

auto to_api_stage(ChainControlStage stage) -> goggles_chain_stage_t {
    switch (stage) {
    case ChainControlStage::prechain:
        return GOGGLES_CHAIN_STAGE_PRECHAIN;
    case ChainControlStage::postchain:
        return GOGGLES_CHAIN_STAGE_POSTCHAIN;
    case ChainControlStage::effect:
    default:
        return GOGGLES_CHAIN_STAGE_EFFECT;
    }
}

auto to_control_stage(goggles_chain_stage_t stage) -> ChainControlStage {
    switch (stage) {
    case GOGGLES_CHAIN_STAGE_PRECHAIN:
        return ChainControlStage::prechain;
    case GOGGLES_CHAIN_STAGE_POSTCHAIN:
        return ChainControlStage::postchain;
    case GOGGLES_CHAIN_STAGE_EFFECT:
    default:
        return ChainControlStage::effect;
    }
}

auto to_api_stage_mask(const ChainStagePolicy& policy) -> goggles_chain_stage_mask_t {
    return static_cast<goggles_chain_stage_mask_t>(to_underlying(stage_policy_mask(policy)));
}

auto snapshot_to_controls(const goggles_chain_control_snapshot_t* snapshot)
    -> std::vector<ChainControlDescriptor> {
    std::vector<ChainControlDescriptor> controls;
    const auto* descriptors = goggles_chain_control_snapshot_get_data(snapshot);
    const size_t count = goggles_chain_control_snapshot_get_count(snapshot);
    controls.reserve(count);

    for (size_t index = 0; index < count; ++index) {
        const auto& descriptor = descriptors[index];
        controls.push_back(ChainControlDescriptor{
            .control_id = descriptor.control_id,
            .stage = to_control_stage(descriptor.stage),
            .name = descriptor.name_utf8 != nullptr ? descriptor.name_utf8 : "",
            .description = descriptor.description_utf8 != nullptr
                               ? std::optional<std::string>{descriptor.description_utf8}
                               : std::nullopt,
            .current_value = descriptor.current_value,
            .default_value = descriptor.default_value,
            .min_value = descriptor.min_value,
            .max_value = descriptor.max_value,
            .step = descriptor.step,
        });
    }

    return controls;
}

} // namespace

FilterChainRuntime::~FilterChainRuntime() {
    auto result = destroy();
    if (!result) {
        GOGGLES_LOG_WARN("FilterChainRuntime destructor cleanup failed: {}",
                         result.error().message);
    }
}

FilterChainRuntime::FilterChainRuntime(FilterChainRuntime&& other) noexcept
    : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

auto FilterChainRuntime::operator=(FilterChainRuntime&& other) noexcept -> FilterChainRuntime& {
    if (this == &other) {
        return *this;
    }
    auto result = destroy();
    if (!result) {
        GOGGLES_LOG_WARN("FilterChainRuntime move-assignment cleanup failed: {}",
                         result.error().message);
        return *this;
    }
    m_handle = other.m_handle;
    other.m_handle = nullptr;
    return *this;
}

auto FilterChainRuntime::create(const ChainCreateInfo& create_info) -> Result<FilterChainRuntime> {
    auto info = goggles_chain_vk_create_info_ex_init();
    const auto shader_dir_utf8 = to_utf8_bytes(create_info.shader_dir);
    const auto cache_dir_utf8 = to_utf8_bytes(create_info.cache_dir);

    info.target_format = static_cast<VkFormat>(create_info.target_format);
    info.num_sync_indices = create_info.num_sync_indices;
    info.shader_dir_utf8 = shader_dir_utf8.c_str();
    info.shader_dir_len = shader_dir_utf8.size();
    info.cache_dir_utf8 = cache_dir_utf8.empty() ? nullptr : cache_dir_utf8.c_str();
    info.cache_dir_len = cache_dir_utf8.size();
    info.initial_prechain_resolution.width = create_info.initial_prechain_resolution.width;
    info.initial_prechain_resolution.height = create_info.initial_prechain_resolution.height;

    const goggles_chain_vk_context_t vk_context{
        .device = static_cast<VkDevice>(create_info.device),
        .physical_device = static_cast<VkPhysicalDevice>(create_info.physical_device),
        .graphics_queue = static_cast<VkQueue>(create_info.graphics_queue),
        .graphics_queue_family_index = create_info.graphics_queue_family_index,
    };

    goggles_chain_t* handle = nullptr;
    const auto status = goggles_chain_create_vk_ex(&vk_context, &info, &handle);
    if (status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(handle, status, "Failed to create filter chain"));
    }

    return FilterChainRuntime{handle};
}

auto FilterChainRuntime::destroy() -> Result<void> {
    if (m_handle == nullptr) {
        return {};
    }

    auto* chain = m_handle;
    const auto status = goggles_chain_destroy(&m_handle);
    if (status != GOGGLES_CHAIN_STATUS_OK) {
        m_handle = nullptr;
    }
    return status_to_result(chain, status, "Failed to destroy filter chain");
}

auto FilterChainRuntime::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    if (m_handle == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto preset_path_utf8 = to_utf8_bytes(preset_path);
    const auto status =
        goggles_chain_preset_load_ex(m_handle, preset_path_utf8.c_str(), preset_path_utf8.size());
    return status_to_result(m_handle, status, "Failed to load shader preset");
}

auto FilterChainRuntime::handle_resize(vk::Extent2D new_target_extent) -> Result<void> {
    if (m_handle == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto status = goggles_chain_handle_resize(
        m_handle, goggles_chain_extent2d_t{.width = new_target_extent.width,
                                           .height = new_target_extent.height});
    return status_to_result(m_handle, status, "Failed to resize filter chain");
}

auto FilterChainRuntime::set_stage_policy(const ChainStagePolicy& policy) -> Result<void> {
    if (m_handle == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    auto stage_policy = goggles_chain_stage_policy_init();
    stage_policy.enabled_stage_mask = to_api_stage_mask(policy);
    const auto status = goggles_chain_stage_policy_set(m_handle, &stage_policy);
    return status_to_result(m_handle, status, "Failed to apply chain policy");
}

auto FilterChainRuntime::get_stage_policy() const -> Result<ChainStagePolicy> {
    if (m_handle == nullptr) {
        return make_error<ChainStagePolicy>(ErrorCode::vulkan_init_failed,
                                            "Filter chain not initialized");
    }

    auto stage_policy = goggles_chain_stage_policy_init();
    const auto status = goggles_chain_stage_policy_get(m_handle, &stage_policy);
    if (status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(m_handle, status, "Failed to read chain policy"));
    }

    return ChainStagePolicy{
        .prechain_enabled =
            (stage_policy.enabled_stage_mask & GOGGLES_CHAIN_STAGE_MASK_PRECHAIN) != 0u,
        .effect_stage_enabled =
            (stage_policy.enabled_stage_mask & GOGGLES_CHAIN_STAGE_MASK_EFFECT) != 0u,
    };
}

auto FilterChainRuntime::set_prechain_resolution(vk::Extent2D resolution) -> Result<void> {
    if (m_handle == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto status = goggles_chain_prechain_resolution_set(
        m_handle, goggles_chain_extent2d_t{.width = resolution.width, .height = resolution.height});
    return status_to_result(m_handle, status, "Failed to set prechain resolution");
}

auto FilterChainRuntime::get_prechain_resolution() const -> Result<vk::Extent2D> {
    if (m_handle == nullptr) {
        return make_error<vk::Extent2D>(ErrorCode::vulkan_init_failed,
                                        "Filter chain not initialized");
    }

    goggles_chain_extent2d_t resolution{};
    const auto status = goggles_chain_prechain_resolution_get(m_handle, &resolution);
    if (status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(m_handle, status, "Failed to get prechain resolution"));
    }
    return vk::Extent2D{resolution.width, resolution.height};
}

auto FilterChainRuntime::record(const ChainRecordInfo& record_info) -> Result<void> {
    if (m_handle == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    auto info = goggles_chain_vk_record_info_init();
    info.command_buffer = static_cast<VkCommandBuffer>(record_info.command_buffer);
    info.source_image = static_cast<VkImage>(record_info.source_image);
    info.source_view = static_cast<VkImageView>(record_info.source_view);
    info.source_extent.width = record_info.source_extent.width;
    info.source_extent.height = record_info.source_extent.height;
    info.target_view = static_cast<VkImageView>(record_info.target_view);
    info.target_extent.width = record_info.target_extent.width;
    info.target_extent.height = record_info.target_extent.height;
    info.frame_index = record_info.frame_index;
    info.scale_mode = to_chain_scale_mode(record_info.scale_mode);
    info.integer_scale = record_info.integer_scale;

    const auto status = goggles_chain_record_vk(m_handle, &info);
    return status_to_result(m_handle, status, "Failed to record filter chain");
}

auto FilterChainRuntime::list_controls() const -> Result<std::vector<ChainControlDescriptor>> {
    if (m_handle == nullptr) {
        return make_error<std::vector<ChainControlDescriptor>>(ErrorCode::vulkan_init_failed,
                                                               "Filter chain not initialized");
    }

    goggles_chain_control_snapshot_t* snapshot = nullptr;
    const auto list_status = goggles_chain_control_list(m_handle, &snapshot);
    if (list_status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(m_handle, list_status, "Failed to list filter controls"));
    }

    auto controls = snapshot_to_controls(snapshot);
    const auto destroy_status = goggles_chain_control_snapshot_destroy(&snapshot);
    if (destroy_status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(m_handle, destroy_status, "Failed to destroy control snapshot"));
    }

    return controls;
}

auto FilterChainRuntime::list_controls(ChainControlStage stage) const
    -> Result<std::vector<ChainControlDescriptor>> {
    if (m_handle == nullptr) {
        return make_error<std::vector<ChainControlDescriptor>>(ErrorCode::vulkan_init_failed,
                                                               "Filter chain not initialized");
    }

    goggles_chain_control_snapshot_t* snapshot = nullptr;
    const auto list_status =
        goggles_chain_control_list_stage(m_handle, to_api_stage(stage), &snapshot);
    if (list_status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(m_handle, list_status, "Failed to list stage filter controls"));
    }

    auto controls = snapshot_to_controls(snapshot);
    const auto destroy_status = goggles_chain_control_snapshot_destroy(&snapshot);
    if (destroy_status != GOGGLES_CHAIN_STATUS_OK) {
        return nonstd::make_unexpected(
            make_chain_error(m_handle, destroy_status, "Failed to destroy stage control snapshot"));
    }

    return controls;
}

auto FilterChainRuntime::set_control_value(std::uint64_t control_id, float value) -> Result<bool> {
    if (m_handle == nullptr) {
        return make_error<bool>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto status = goggles_chain_control_set_value(m_handle, control_id, value);
    if (status == GOGGLES_CHAIN_STATUS_OK) {
        return true;
    }
    if (status == GOGGLES_CHAIN_STATUS_NOT_FOUND) {
        return false;
    }
    return nonstd::make_unexpected(
        make_chain_error(m_handle, status, "Failed to set filter control value"));
}

auto FilterChainRuntime::reset_control_value(std::uint64_t control_id) -> Result<bool> {
    if (m_handle == nullptr) {
        return make_error<bool>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto status = goggles_chain_control_reset_value(m_handle, control_id);
    if (status == GOGGLES_CHAIN_STATUS_OK) {
        return true;
    }
    if (status == GOGGLES_CHAIN_STATUS_NOT_FOUND) {
        return false;
    }
    return nonstd::make_unexpected(
        make_chain_error(m_handle, status, "Failed to reset filter control value"));
}

auto FilterChainRuntime::reset_all_controls() -> Result<void> {
    if (m_handle == nullptr) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    const auto status = goggles_chain_control_reset_all(m_handle);
    return status_to_result(m_handle, status, "Failed to reset filter controls");
}

} // namespace goggles::render
