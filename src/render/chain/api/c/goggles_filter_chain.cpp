#include "goggles_filter_chain.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <render/chain/chain_runtime.hpp>
#include <render/chain/filter_controls.hpp>
#include <render/chain/vulkan_context.hpp>
#include <string>
#include <string_view>
#include <util/config.hpp>
#include <util/diagnostics/diagnostic_policy.hpp>
#include <util/diagnostics/diagnostic_sink.hpp>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace {

using goggles::ErrorCode;
using goggles::ScaleMode;
using goggles::diagnostics::ActivationTier;
using goggles::diagnostics::CaptureMode;
using goggles::diagnostics::DiagnosticEvent;
using goggles::diagnostics::DiagnosticPolicy;
using goggles::diagnostics::PolicyMode;
using goggles::diagnostics::Severity;
using goggles::render::ChainRuntime;
using goggles::render::FilterChainPaths;
using goggles::render::FilterControlDescriptor;
using goggles::render::FilterControlStage;
using goggles::render::VulkanContext;

constexpr uint64_t CHAIN_MAGIC = 0x474f47434841494eULL;
constexpr uint64_t SNAPSHOT_MAGIC = 0x474f47534e415054ULL;
constexpr uint32_t MAX_SYNC_INDICES = 8u;

enum class ChainState : uint8_t { created, ready, dead };
enum class ErrorSubsystem : uint8_t {
    none = 0u,
    validation = 1u,
    runtime = 2u,
    control = 3u,
    vulkan = 4u,
};

struct LastErrorInfo {
    goggles_chain_status_t status = GOGGLES_CHAIN_STATUS_OK;
    int32_t vk_result = 0;
    uint32_t subsystem_code = static_cast<uint32_t>(ErrorSubsystem::none);
};

} // namespace

struct goggles_chain {
    uint64_t magic = CHAIN_MAGIC;
    std::unique_ptr<ChainRuntime> runtime;
    vk::Device device;
    vk::CommandPool command_pool;
    uint32_t num_sync_indices = 0;
    goggles_chain_stage_mask_t stage_mask = GOGGLES_CHAIN_STAGE_MASK_ALL;
    ChainState state = ChainState::created;
    LastErrorInfo last_error;
};

struct goggles_chain_control_snapshot {
    uint64_t magic = SNAPSHOT_MAGIC;
    std::vector<goggles_chain_control_desc_t> descriptors;
    std::vector<std::string> names;
    std::vector<std::optional<std::string>> descriptions;
};

namespace {

auto is_valid_chain_handle(const goggles_chain_t* chain) -> bool {
    return chain != nullptr && chain->magic == CHAIN_MAGIC && chain->state != ChainState::dead &&
           static_cast<bool>(chain->runtime);
}

auto is_valid_snapshot_handle(const goggles_chain_control_snapshot_t* snapshot) -> bool {
    return snapshot != nullptr && snapshot->magic == SNAPSHOT_MAGIC;
}

auto set_last_error(goggles_chain_t* chain, goggles_chain_status_t status, ErrorSubsystem subsystem,
                    int32_t vk_result = 0) -> goggles_chain_status_t {
    if (is_valid_chain_handle(chain) && status != GOGGLES_CHAIN_STATUS_OK) {
        chain->last_error.status = status;
        chain->last_error.vk_result = vk_result;
        chain->last_error.subsystem_code = static_cast<uint32_t>(subsystem);
    }
    return status;
}

auto fail_chain(goggles_chain_t* chain, goggles_chain_status_t status, ErrorSubsystem subsystem,
                int32_t vk_result = 0) -> goggles_chain_status_t {
    return set_last_error(chain, status, subsystem, vk_result);
}

auto fail_chain(const goggles_chain_t* chain, goggles_chain_status_t status,
                ErrorSubsystem subsystem, int32_t vk_result = 0) -> goggles_chain_status_t {
    return fail_chain(const_cast<goggles_chain_t*>(chain), status, subsystem, vk_result);
}

auto fail_exception(goggles_chain_t* chain, bool out_of_memory) -> goggles_chain_status_t {
    const auto status =
        out_of_memory ? GOGGLES_CHAIN_STATUS_OUT_OF_MEMORY : GOGGLES_CHAIN_STATUS_RUNTIME_ERROR;
    return fail_chain(chain, status, ErrorSubsystem::runtime);
}

auto fail_exception(const goggles_chain_t* chain, bool out_of_memory) -> goggles_chain_status_t {
    return fail_exception(const_cast<goggles_chain_t*>(chain), out_of_memory);
}

auto contains_nul(std::string_view value) -> bool {
    return value.find('\0') != std::string_view::npos;
}

auto is_continuation_byte(unsigned char value) -> bool {
    return (value & 0xc0u) == 0x80u;
}

auto is_valid_utf8(std::string_view value) -> bool {
    size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7fu) {
            ++index;
            continue;
        }

        if ((first & 0xe0u) == 0xc0u) {
            if (index + 1 >= value.size()) {
                return false;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            if (first < 0xc2u || !is_continuation_byte(second)) {
                return false;
            }
            index += 2;
            continue;
        }

        if ((first & 0xf0u) == 0xe0u) {
            if (index + 2 >= value.size()) {
                return false;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            const auto third = static_cast<unsigned char>(value[index + 2]);
            if (!is_continuation_byte(second) || !is_continuation_byte(third)) {
                return false;
            }
            if (first == 0xe0u && second < 0xa0u) {
                return false;
            }
            if (first == 0xedu && second >= 0xa0u) {
                return false;
            }
            index += 3;
            continue;
        }

        if ((first & 0xf8u) == 0xf0u) {
            if (index + 3 >= value.size()) {
                return false;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            const auto third = static_cast<unsigned char>(value[index + 2]);
            const auto fourth = static_cast<unsigned char>(value[index + 3]);
            if (!is_continuation_byte(second) || !is_continuation_byte(third) ||
                !is_continuation_byte(fourth)) {
                return false;
            }
            if (first == 0xf0u && second < 0x90u) {
                return false;
            }
            if (first > 0xf4u) {
                return false;
            }
            if (first == 0xf4u && second > 0x8fu) {
                return false;
            }
            index += 4;
            continue;
        }

        return false;
    }

    return true;
}

auto copy_required_utf8_span(const char* bytes, size_t length, std::string* out_text) -> bool {
    if (out_text == nullptr || bytes == nullptr || length == 0u) {
        return false;
    }

    const std::string_view text{bytes, length};
    if (contains_nul(text) || !is_valid_utf8(text)) {
        return false;
    }

    *out_text = std::string{text};
    return true;
}

auto copy_optional_utf8_span(const char* bytes, size_t length, std::string* out_text) -> bool {
    if (out_text == nullptr) {
        return false;
    }

    if (length == 0u) {
        out_text->clear();
        return true;
    }

    return copy_required_utf8_span(bytes, length, out_text);
}

auto copy_required_utf8_cstr(const char* text, std::string* out_text) -> bool {
    if (text == nullptr) {
        return false;
    }
    return copy_required_utf8_span(text, std::strlen(text), out_text);
}

auto copy_optional_utf8_cstr(const char* text, std::string* out_text) -> bool {
    if (text == nullptr) {
        out_text->clear();
        return true;
    }
    return copy_optional_utf8_span(text, std::strlen(text), out_text);
}

auto is_valid_extent(goggles_chain_extent2d_t extent) -> bool {
    return extent.width > 0u && extent.height > 0u;
}

auto to_vulkan_extent(goggles_chain_extent2d_t extent) -> vk::Extent2D {
    return vk::Extent2D{extent.width, extent.height};
}

auto to_api_extent(vk::Extent2D extent) -> goggles_chain_extent2d_t {
    return goggles_chain_extent2d_t{.width = extent.width, .height = extent.height};
}

auto is_valid_stage_mask(goggles_chain_stage_mask_t stage_mask) -> bool {
    if (stage_mask == 0u) {
        return false;
    }
    return (stage_mask & ~GOGGLES_CHAIN_STAGE_MASK_ALL) == 0u;
}

auto to_scale_mode(goggles_chain_scale_mode_t scale_mode, ScaleMode* out_mode) -> bool {
    if (out_mode == nullptr) {
        return false;
    }

    switch (scale_mode) {
    case GOGGLES_CHAIN_SCALE_MODE_STRETCH:
        *out_mode = ScaleMode::stretch;
        return true;
    case GOGGLES_CHAIN_SCALE_MODE_FIT:
        *out_mode = ScaleMode::fit;
        return true;
    case GOGGLES_CHAIN_SCALE_MODE_INTEGER:
        *out_mode = ScaleMode::integer;
        return true;
    default:
        return false;
    }
}

auto to_api_stage(FilterControlStage stage) -> goggles_chain_stage_t {
    switch (stage) {
    case FilterControlStage::prechain:
        return GOGGLES_CHAIN_STAGE_PRECHAIN;
    case FilterControlStage::effect:
        return GOGGLES_CHAIN_STAGE_EFFECT;
    }
    return GOGGLES_CHAIN_STAGE_EFFECT;
}

auto to_filter_control_stage(goggles_chain_stage_t stage, FilterControlStage* out_stage) -> bool {
    if (out_stage == nullptr) {
        return false;
    }

    switch (stage) {
    case GOGGLES_CHAIN_STAGE_PRECHAIN:
        *out_stage = FilterControlStage::prechain;
        return true;
    case GOGGLES_CHAIN_STAGE_EFFECT:
        *out_stage = FilterControlStage::effect;
        return true;
    default:
        return false;
    }
}

auto map_error_code(ErrorCode code) -> goggles_chain_status_t {
    switch (code) {
    case ErrorCode::file_not_found:
    case ErrorCode::file_read_failed:
    case ErrorCode::file_write_failed:
        return GOGGLES_CHAIN_STATUS_IO_ERROR;
    case ErrorCode::parse_error:
    case ErrorCode::shader_compile_failed:
    case ErrorCode::shader_load_failed:
        return GOGGLES_CHAIN_STATUS_PRESET_ERROR;
    case ErrorCode::invalid_config:
    case ErrorCode::invalid_data:
        return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
    case ErrorCode::vulkan_init_failed:
    case ErrorCode::vulkan_device_lost:
        return GOGGLES_CHAIN_STATUS_VULKAN_ERROR;
    case ErrorCode::ok:
    case ErrorCode::input_init_failed:
    case ErrorCode::unknown_error:
    default:
        return GOGGLES_CHAIN_STATUS_RUNTIME_ERROR;
    }
}

void apply_stage_policy(goggles_chain_t* chain) {
    if (!is_valid_chain_handle(chain)) {
        return;
    }

    const bool prechain_enabled = (chain->stage_mask & GOGGLES_CHAIN_STAGE_MASK_PRECHAIN) != 0u;
    const bool effect_enabled = (chain->stage_mask & GOGGLES_CHAIN_STAGE_MASK_EFFECT) != 0u;
    chain->runtime->set_stage_policy(prechain_enabled, effect_enabled);
}

auto ensure_chain_state(goggles_chain_t* chain, bool require_ready) -> goggles_chain_status_t {
    if (!is_valid_chain_handle(chain)) {
        return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
    }

    if (require_ready && chain->state != ChainState::ready) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_NOT_INITIALIZED, ErrorSubsystem::validation);
    }

    return GOGGLES_CHAIN_STATUS_OK;
}

auto ensure_chain_state(const goggles_chain_t* chain, bool require_ready)
    -> goggles_chain_status_t {
    return ensure_chain_state(const_cast<goggles_chain_t*>(chain), require_ready);
}

void destroy_runtime(goggles_chain_t* chain) {
    if (chain == nullptr) {
        return;
    }

    if (chain->runtime) {
        chain->runtime->shutdown();
        chain->runtime.reset();
    }

    if (chain->device && chain->command_pool) {
        chain->device.destroyCommandPool(chain->command_pool);
    }
    chain->command_pool = nullptr;
    chain->state = ChainState::dead;
}

struct CreateInput {
    vk::Format target_format;
    uint32_t num_sync_indices = 0u;
    std::string shader_dir;
    std::string cache_dir;
    goggles_chain_extent2d_t initial_prechain_resolution{};
};

auto create_runtime(const goggles_chain_vk_context_t* vk_context, const CreateInput& input,
                    goggles_chain_t** out_chain) -> goggles_chain_status_t {
    const vk::Device device{vk_context->device};

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = vk_context->graphics_queue_family_index;

    const auto [pool_result, command_pool] = device.createCommandPool(pool_info);
    if (pool_result != vk::Result::eSuccess) {
        return GOGGLES_CHAIN_STATUS_VULKAN_ERROR;
    }

    VulkanContext context{
        .device = device,
        .physical_device = vk::PhysicalDevice{vk_context->physical_device},
        .command_pool = command_pool,
        .graphics_queue = vk::Queue{vk_context->graphics_queue},
        .graphics_queue_family_index = vk_context->graphics_queue_family_index,
    };

    FilterChainPaths paths{
        .shader_dir = std::filesystem::path(input.shader_dir),
        .cache_dir = std::filesystem::path(input.cache_dir),
    };

    auto runtime_result =
        ChainRuntime::create(context, input.target_format, input.num_sync_indices, paths,
                             to_vulkan_extent(input.initial_prechain_resolution));
    if (!runtime_result) {
        device.destroyCommandPool(command_pool);
        return map_error_code(runtime_result.error().code);
    }

    auto chain = std::make_unique<goggles_chain_t>();
    chain->runtime = std::move(runtime_result.value());
    chain->device = device;
    chain->command_pool = command_pool;
    chain->num_sync_indices = input.num_sync_indices;
    chain->stage_mask = GOGGLES_CHAIN_STAGE_MASK_ALL;
    chain->state = ChainState::created;
    apply_stage_policy(chain.get());

    *out_chain = chain.release();
    return GOGGLES_CHAIN_STATUS_OK;
}

auto build_snapshot(const std::vector<FilterControlDescriptor>& controls,
                    goggles_chain_control_snapshot_t** out_snapshot) -> goggles_chain_status_t {
    auto snapshot = std::make_unique<goggles_chain_control_snapshot_t>();
    snapshot->descriptors.resize(controls.size());
    snapshot->names.reserve(controls.size());
    snapshot->descriptions.reserve(controls.size());

    for (const auto& control : controls) {
        snapshot->names.push_back(control.name);
        snapshot->descriptions.push_back(control.description);
    }

    for (size_t index = 0; index < controls.size(); ++index) {
        const auto& control = controls[index];
        auto& descriptor = snapshot->descriptors[index];
        descriptor.control_id = control.control_id;
        descriptor.stage = to_api_stage(control.stage);
        descriptor.name_utf8 = snapshot->names[index].c_str();
        const auto& description = snapshot->descriptions[index];
        descriptor.description_utf8 = description.has_value() ? description->c_str() : nullptr;
        descriptor.current_value = control.current_value;
        descriptor.default_value = control.default_value;
        descriptor.min_value = control.min_value;
        descriptor.max_value = control.max_value;
        descriptor.step = control.step;
    }

    *out_snapshot = snapshot.release();
    return GOGGLES_CHAIN_STATUS_OK;
}

class CallbackSink final : public goggles::diagnostics::DiagnosticSink {
public:
    CallbackSink(goggles_chain_diagnostic_event_cb callback, void* user_data)
        : m_callback(callback), m_user_data(user_data) {}

    void receive(const DiagnosticEvent& event) override {
        if (m_callback == nullptr) {
            return;
        }
        m_callback(static_cast<uint32_t>(event.severity), static_cast<uint32_t>(event.category),
                   event.localization.pass_ordinal, event.message.c_str(), m_user_data);
    }

private:
    goggles_chain_diagnostic_event_cb m_callback = nullptr;
    void* m_user_data = nullptr;
};

auto to_capture_mode(uint32_t value) -> std::optional<CaptureMode> {
    switch (value) {
    case GOGGLES_CHAIN_DIAG_MODE_MINIMAL:
        return CaptureMode::minimal;
    case GOGGLES_CHAIN_DIAG_MODE_STANDARD:
        return CaptureMode::standard;
    case GOGGLES_CHAIN_DIAG_MODE_INVESTIGATE:
        return CaptureMode::investigate;
    case GOGGLES_CHAIN_DIAG_MODE_FORENSIC:
        return CaptureMode::forensic;
    default:
        return std::nullopt;
    }
}

auto to_policy_mode(uint32_t value) -> std::optional<PolicyMode> {
    switch (value) {
    case GOGGLES_CHAIN_DIAG_POLICY_COMPATIBILITY:
        return PolicyMode::compatibility;
    case GOGGLES_CHAIN_DIAG_POLICY_STRICT:
        return PolicyMode::strict;
    default:
        return std::nullopt;
    }
}

auto to_activation_tier(uint32_t value) -> std::optional<ActivationTier> {
    switch (value) {
    case 0u:
        return ActivationTier::tier0;
    case 1u:
        return ActivationTier::tier1;
    case 2u:
        return ActivationTier::tier2;
    default:
        return std::nullopt;
    }
}

auto diagnostics_not_active(goggles_chain_t* chain) -> goggles_chain_status_t {
    return fail_chain(chain, GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE, ErrorSubsystem::runtime);
}

} // namespace

extern "C" {

auto goggles_chain_api_version() -> uint32_t {
    return GOGGLES_CHAIN_API_VERSION;
}

auto goggles_chain_abi_version() -> uint32_t {
    return GOGGLES_CHAIN_ABI_VERSION;
}

auto goggles_chain_capabilities_get(goggles_chain_capabilities_t* out_caps)
    -> goggles_chain_status_t {
    if (out_caps == nullptr) {
        return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
    }
    if (out_caps->struct_size < sizeof(goggles_chain_capabilities_t)) {
        return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
    }

    out_caps->capability_flags = GOGGLES_CHAIN_CAPABILITY_LAST_ERROR_INFO;
    out_caps->max_sync_indices = MAX_SYNC_INDICES;
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_status_to_string(goggles_chain_status_t status) -> const char* {
    switch (status) {
    case GOGGLES_CHAIN_STATUS_OK:
        return "OK";
    case GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case GOGGLES_CHAIN_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case GOGGLES_CHAIN_STATUS_NOT_FOUND:
        return "NOT_FOUND";
    case GOGGLES_CHAIN_STATUS_PRESET_ERROR:
        return "PRESET_ERROR";
    case GOGGLES_CHAIN_STATUS_IO_ERROR:
        return "IO_ERROR";
    case GOGGLES_CHAIN_STATUS_VULKAN_ERROR:
        return "VULKAN_ERROR";
    case GOGGLES_CHAIN_STATUS_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case GOGGLES_CHAIN_STATUS_NOT_SUPPORTED:
        return "NOT_SUPPORTED";
    case GOGGLES_CHAIN_STATUS_RUNTIME_ERROR:
        return "RUNTIME_ERROR";
    case GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE:
        return "DIAGNOSTICS_NOT_ACTIVE";
    default:
        return "UNKNOWN_STATUS";
    }
}

auto goggles_chain_create_vk(const goggles_chain_vk_context_t* vk_context,
                             const goggles_chain_vk_create_info_t* create_info,
                             goggles_chain_t** out_chain) -> goggles_chain_status_t {
    try {
        if (out_chain != nullptr) {
            *out_chain = nullptr;
        }

        if (vk_context == nullptr || create_info == nullptr || out_chain == nullptr) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }

        if (create_info->struct_size < sizeof(goggles_chain_vk_create_info_t)) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        if (vk_context->device == VK_NULL_HANDLE || vk_context->physical_device == VK_NULL_HANDLE ||
            vk_context->graphics_queue == VK_NULL_HANDLE ||
            vk_context->graphics_queue_family_index == UINT32_MAX) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        if (create_info->target_format == VK_FORMAT_UNDEFINED) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        if (create_info->num_sync_indices == 0u ||
            create_info->num_sync_indices > MAX_SYNC_INDICES) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        std::string shader_dir;
        std::string cache_dir;
        if (!copy_required_utf8_cstr(create_info->shader_dir_utf8, &shader_dir) ||
            !copy_optional_utf8_cstr(create_info->cache_dir_utf8, &cache_dir)) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }

        const CreateInput input{
            .target_format = static_cast<vk::Format>(create_info->target_format),
            .num_sync_indices = create_info->num_sync_indices,
            .shader_dir = std::move(shader_dir),
            .cache_dir = std::move(cache_dir),
            .initial_prechain_resolution = create_info->initial_prechain_resolution,
        };

        return create_runtime(vk_context, input, out_chain);
    } catch (const std::bad_alloc&) {
        return fail_exception(static_cast<goggles_chain_t*>(nullptr), true);
    } catch (...) {
        return fail_exception(static_cast<goggles_chain_t*>(nullptr), false);
    }
}

auto goggles_chain_create_vk_ex(const goggles_chain_vk_context_t* vk_context,
                                const goggles_chain_vk_create_info_ex_t* create_info,
                                goggles_chain_t** out_chain) -> goggles_chain_status_t {
    try {
        if (out_chain != nullptr) {
            *out_chain = nullptr;
        }

        if (vk_context == nullptr || create_info == nullptr || out_chain == nullptr) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }

        if (create_info->struct_size < sizeof(goggles_chain_vk_create_info_ex_t)) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        if (vk_context->device == VK_NULL_HANDLE || vk_context->physical_device == VK_NULL_HANDLE ||
            vk_context->graphics_queue == VK_NULL_HANDLE ||
            vk_context->graphics_queue_family_index == UINT32_MAX) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        if (create_info->target_format == VK_FORMAT_UNDEFINED) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        if (create_info->num_sync_indices == 0u ||
            create_info->num_sync_indices > MAX_SYNC_INDICES) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }
        std::string shader_dir;
        std::string cache_dir;
        if (!copy_required_utf8_span(create_info->shader_dir_utf8, create_info->shader_dir_len,
                                     &shader_dir) ||
            !copy_optional_utf8_span(create_info->cache_dir_utf8, create_info->cache_dir_len,
                                     &cache_dir)) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }

        const CreateInput input{
            .target_format = static_cast<vk::Format>(create_info->target_format),
            .num_sync_indices = create_info->num_sync_indices,
            .shader_dir = std::move(shader_dir),
            .cache_dir = std::move(cache_dir),
            .initial_prechain_resolution = create_info->initial_prechain_resolution,
        };

        return create_runtime(vk_context, input, out_chain);
    } catch (const std::bad_alloc&) {
        return fail_exception(static_cast<goggles_chain_t*>(nullptr), true);
    } catch (...) {
        return fail_exception(static_cast<goggles_chain_t*>(nullptr), false);
    }
}

auto goggles_chain_destroy(goggles_chain_t** chain) -> goggles_chain_status_t {
    try {
        if (chain == nullptr || *chain == nullptr) {
            return GOGGLES_CHAIN_STATUS_OK;
        }

        if (!is_valid_chain_handle(*chain)) {
            return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
        }

        auto* runtime = *chain;
        destroy_runtime(runtime);
        runtime->magic = 0u;
        delete runtime;
        *chain = nullptr;
        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain != nullptr ? *chain : nullptr, true);
    } catch (...) {
        return fail_exception(chain != nullptr ? *chain : nullptr, false);
    }
}

auto goggles_chain_preset_load(goggles_chain_t* chain, const char* preset_path_utf8)
    -> goggles_chain_status_t {
    if (!is_valid_chain_handle(chain)) {
        return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
    }
    if (preset_path_utf8 == nullptr) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }

    return goggles_chain_preset_load_ex(chain, preset_path_utf8, std::strlen(preset_path_utf8));
}

auto goggles_chain_preset_load_ex(goggles_chain_t* chain, const char* preset_path_utf8,
                                  size_t preset_path_len) -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, false);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }

        std::string preset_path;
        if (!copy_optional_utf8_span(preset_path_utf8, preset_path_len, &preset_path)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        auto result = chain->runtime->load_preset(std::filesystem::path{preset_path});
        if (!result) {
            return fail_chain(chain, map_error_code(result.error().code), ErrorSubsystem::runtime);
        }

        chain->state = ChainState::ready;
        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_handle_resize(goggles_chain_t* chain, goggles_chain_extent2d_t new_target_extent)
    -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, false);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }

        if (!is_valid_extent(new_target_extent)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        auto result = chain->runtime->handle_resize(to_vulkan_extent(new_target_extent));
        if (!result) {
            return fail_chain(chain, map_error_code(result.error().code), ErrorSubsystem::runtime);
        }

        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_record_vk(goggles_chain_t* chain,
                             const goggles_chain_vk_record_info_t* record_info)
    -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, true);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }

        if (record_info == nullptr ||
            record_info->struct_size < sizeof(goggles_chain_vk_record_info_t)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }
        if (record_info->command_buffer == VK_NULL_HANDLE ||
            record_info->source_image == VK_NULL_HANDLE ||
            record_info->source_view == VK_NULL_HANDLE ||
            record_info->target_view == VK_NULL_HANDLE) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }
        if (!is_valid_extent(record_info->source_extent) ||
            !is_valid_extent(record_info->target_extent)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }
        if (record_info->frame_index >= chain->num_sync_indices) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        ScaleMode scale_mode = ScaleMode::stretch;
        if (!to_scale_mode(record_info->scale_mode, &scale_mode)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }
        if (record_info->scale_mode == GOGGLES_CHAIN_SCALE_MODE_INTEGER &&
            record_info->integer_scale == 0u) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        chain->runtime->record(
            vk::CommandBuffer{record_info->command_buffer}, vk::Image{record_info->source_image},
            vk::ImageView{record_info->source_view}, to_vulkan_extent(record_info->source_extent),
            vk::ImageView{record_info->target_view}, to_vulkan_extent(record_info->target_extent),
            record_info->frame_index, scale_mode, record_info->integer_scale);
        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_stage_policy_set(goggles_chain_t* chain,
                                    const goggles_chain_stage_policy_t* policy)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    if (policy == nullptr || policy->struct_size < sizeof(goggles_chain_stage_policy_t)) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }
    if (!is_valid_stage_mask(policy->enabled_stage_mask)) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }

    chain->stage_mask = policy->enabled_stage_mask;
    apply_stage_policy(chain);
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_stage_policy_get(const goggles_chain_t* chain,
                                    goggles_chain_stage_policy_t* out_policy)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    if (out_policy == nullptr || out_policy->struct_size < sizeof(goggles_chain_stage_policy_t)) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }

    out_policy->enabled_stage_mask = chain->stage_mask;
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_prechain_resolution_set(goggles_chain_t* chain,
                                           goggles_chain_extent2d_t resolution)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    chain->runtime->set_prechain_resolution(to_vulkan_extent(resolution));
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_prechain_resolution_get(const goggles_chain_t* chain,
                                           goggles_chain_extent2d_t* out_resolution)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    if (out_resolution == nullptr) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }

    *out_resolution = to_api_extent(chain->runtime->get_prechain_resolution());
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_control_list(const goggles_chain_t* chain,
                                goggles_chain_control_snapshot_t** out_snapshot)
    -> goggles_chain_status_t {
    try {
        if (out_snapshot != nullptr) {
            *out_snapshot = nullptr;
        }

        const auto state_status = ensure_chain_state(chain, true);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }
        if (out_snapshot == nullptr) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        return build_snapshot(chain->runtime->list_controls(), out_snapshot);
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_control_list_stage(const goggles_chain_t* chain, goggles_chain_stage_t stage,
                                      goggles_chain_control_snapshot_t** out_snapshot)
    -> goggles_chain_status_t {
    try {
        if (out_snapshot != nullptr) {
            *out_snapshot = nullptr;
        }

        const auto state_status = ensure_chain_state(chain, true);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }
        if (out_snapshot == nullptr) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        if (stage == GOGGLES_CHAIN_STAGE_POSTCHAIN) {
            return build_snapshot({}, out_snapshot);
        }

        FilterControlStage control_stage = FilterControlStage::effect;
        if (!to_filter_control_stage(stage, &control_stage)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        return build_snapshot(chain->runtime->list_controls(control_stage), out_snapshot);
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_control_snapshot_get_count(const goggles_chain_control_snapshot_t* snapshot)
    -> size_t {
    if (!is_valid_snapshot_handle(snapshot)) {
        return 0u;
    }
    return snapshot->descriptors.size();
}

auto goggles_chain_control_snapshot_get_data(const goggles_chain_control_snapshot_t* snapshot)
    -> const goggles_chain_control_desc_t* {
    if (!is_valid_snapshot_handle(snapshot) || snapshot->descriptors.empty()) {
        return nullptr;
    }
    return snapshot->descriptors.data();
}

auto goggles_chain_control_snapshot_destroy(goggles_chain_control_snapshot_t** snapshot)
    -> goggles_chain_status_t {
    if (snapshot == nullptr || *snapshot == nullptr) {
        return GOGGLES_CHAIN_STATUS_OK;
    }
    if (!is_valid_snapshot_handle(*snapshot)) {
        return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
    }

    auto* runtime_snapshot = *snapshot;
    runtime_snapshot->magic = 0u;
    delete runtime_snapshot;
    *snapshot = nullptr;
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_control_set_value(goggles_chain_t* chain, goggles_chain_control_id_t control_id,
                                     float value) -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, true);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }

        if (!std::isfinite(value)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        if (!chain->runtime->set_control_value(control_id, value)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_NOT_FOUND, ErrorSubsystem::control);
        }

        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_control_reset_value(goggles_chain_t* chain,
                                       goggles_chain_control_id_t control_id)
    -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, true);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }

        if (!chain->runtime->reset_control_value(control_id)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_NOT_FOUND, ErrorSubsystem::control);
        }

        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_control_reset_all(goggles_chain_t* chain) -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, true);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }

        chain->runtime->reset_controls();
        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_error_last_info_get(const goggles_chain_t* chain,
                                       goggles_chain_error_last_info_t* out_info)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    if (out_info == nullptr || out_info->struct_size < sizeof(goggles_chain_error_last_info_t)) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }

    out_info->status = chain->last_error.status;
    out_info->vk_result = chain->last_error.vk_result;
    out_info->subsystem_code = chain->last_error.subsystem_code;
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_diagnostics_session_create(
    goggles_chain_t* chain, const goggles_chain_diagnostics_create_info_t* create_info)
    -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, false);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }
        if (create_info == nullptr ||
            create_info->struct_size < sizeof(goggles_chain_diagnostics_create_info_t)) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        const auto capture_mode = to_capture_mode(create_info->reporting_mode);
        const auto policy_mode = to_policy_mode(create_info->policy_mode);
        const auto activation_tier = to_activation_tier(create_info->activation_tier);
        if (!capture_mode.has_value() || !policy_mode.has_value() || !activation_tier.has_value()) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        DiagnosticPolicy policy;
        policy.capture_mode = *capture_mode;
        policy.mode = *policy_mode;
        policy.tier = *activation_tier;
        policy.capture_frame_limit = create_info->capture_frame_limit;
        policy.retention_bytes = create_info->retention_bytes;
        policy.promote_fallback_to_error = policy.mode == PolicyMode::strict;
        policy.reflection_loss_is_fatal = policy.mode == PolicyMode::strict;
        chain->runtime->create_diagnostic_session(policy);
        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_diagnostics_session_destroy(goggles_chain_t* chain) -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    chain->runtime->destroy_diagnostic_session();
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_diagnostics_sink_register(goggles_chain_t* chain,
                                             goggles_chain_diagnostic_event_cb callback,
                                             void* user_data, uint32_t* out_sink_id)
    -> goggles_chain_status_t {
    try {
        const auto state_status = ensure_chain_state(chain, false);
        if (state_status != GOGGLES_CHAIN_STATUS_OK) {
            return state_status;
        }
        if (callback == nullptr || out_sink_id == nullptr) {
            return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT,
                              ErrorSubsystem::validation);
        }

        auto* session = chain->runtime->diagnostic_session();
        if (session == nullptr) {
            return diagnostics_not_active(chain);
        }

        *out_sink_id = session->register_sink(std::make_unique<CallbackSink>(callback, user_data));
        return GOGGLES_CHAIN_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail_exception(chain, true);
    } catch (...) {
        return fail_exception(chain, false);
    }
}

auto goggles_chain_diagnostics_sink_unregister(goggles_chain_t* chain, uint32_t sink_id)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }

    auto* session = chain->runtime->diagnostic_session();
    if (session == nullptr) {
        return diagnostics_not_active(chain);
    }

    session->unregister_sink(sink_id);
    return GOGGLES_CHAIN_STATUS_OK;
}

auto goggles_chain_diagnostics_summary_get(const goggles_chain_t* chain,
                                           goggles_chain_diagnostics_summary_t* out_summary)
    -> goggles_chain_status_t {
    const auto state_status = ensure_chain_state(chain, false);
    if (state_status != GOGGLES_CHAIN_STATUS_OK) {
        return state_status;
    }
    if (out_summary == nullptr ||
        out_summary->struct_size < sizeof(goggles_chain_diagnostics_summary_t)) {
        return fail_chain(chain, GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT, ErrorSubsystem::validation);
    }

    const auto* session = chain->runtime->diagnostic_session();
    if (session == nullptr) {
        return diagnostics_not_active(const_cast<goggles_chain_t*>(chain));
    }

    out_summary->reporting_mode = static_cast<uint32_t>(session->policy().capture_mode);
    out_summary->policy_mode = session->policy().mode == PolicyMode::strict
                                   ? GOGGLES_CHAIN_DIAG_POLICY_STRICT
                                   : GOGGLES_CHAIN_DIAG_POLICY_COMPATIBILITY;
    out_summary->error_count = session->event_count(Severity::error);
    out_summary->warning_count = session->event_count(Severity::warning);
    out_summary->info_count = session->event_count(Severity::info);
    return GOGGLES_CHAIN_STATUS_OK;
}

} // extern "C"
