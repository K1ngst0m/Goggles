#include "api/abi_validation.hpp"
#include "runtime/chain.hpp"
#include "runtime/device.hpp"
#include "runtime/instance.hpp"
#include "runtime/program.hpp"

#include <cstdint>
#include <goggles_filter_chain.h>
#include <new>

using namespace goggles::filter_chain::detail;
using goggles::filter_chain::runtime::Chain;
using goggles::filter_chain::runtime::Device;
using goggles::filter_chain::runtime::Instance;
using goggles::filter_chain::runtime::Program;

/* ── Exception safety guards for extern "C" boundary ────────────────────── */
// std::bad_alloc can propagate from std::make_unique, std::string, std::vector
// operations even with VULKAN_HPP_NO_EXCEPTIONS. Letting C++ exceptions cross
// the C ABI boundary is undefined behavior.

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define FC_API_GUARD_BEGIN try {
#define FC_API_GUARD_END                                                                           \
    }                                                                                              \
    catch (const std::bad_alloc&) {                                                                \
        return GOGGLES_FC_STATUS_OUT_OF_MEMORY;                                                    \
    }                                                                                              \
    catch (...) {                                                                                  \
        return GOGGLES_FC_STATUS_RUNTIME_ERROR;                                                    \
    }

#define FC_API_VOID_GUARD_BEGIN try {
#define FC_API_VOID_GUARD_END                                                                      \
    }                                                                                              \
    catch (...) { /* swallow — void return, destructor context */                                  \
    }
// NOLINTEND(cppcoreguidelines-macro-usage)

/* ── Process-global queries ──────────────────────────────────────────────── */

extern "C" {

uint32_t GOGGLES_FC_CALL goggles_fc_get_api_version() {
    return GOGGLES_FC_API_VERSION;
}

uint32_t GOGGLES_FC_CALL goggles_fc_get_abi_version() {
    return GOGGLES_FC_ABI_VERSION;
}

goggles_fc_capability_flags_t GOGGLES_FC_CALL goggles_fc_get_capabilities() {
    return GOGGLES_FC_CAPABILITY_VULKAN | GOGGLES_FC_CAPABILITY_FILE_SOURCE |
           GOGGLES_FC_CAPABILITY_MEMORY_SOURCE | GOGGLES_FC_CAPABILITY_LOG_CALLBACK;
}

const char* GOGGLES_FC_CALL goggles_fc_status_string(goggles_fc_status_t status) {
    switch (status) {
    case GOGGLES_FC_STATUS_OK:
        return "OK";
    case GOGGLES_FC_STATUS_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case GOGGLES_FC_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case GOGGLES_FC_STATUS_NOT_FOUND:
        return "NOT_FOUND";
    case GOGGLES_FC_STATUS_PRESET_ERROR:
        return "PRESET_ERROR";
    case GOGGLES_FC_STATUS_IO_ERROR:
        return "IO_ERROR";
    case GOGGLES_FC_STATUS_VULKAN_ERROR:
        return "VULKAN_ERROR";
    case GOGGLES_FC_STATUS_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case GOGGLES_FC_STATUS_NOT_SUPPORTED:
        return "NOT_SUPPORTED";
    case GOGGLES_FC_STATUS_RUNTIME_ERROR:
        return "RUNTIME_ERROR";
    case GOGGLES_FC_STATUS_INVALID_DEPENDENCY:
        return "INVALID_DEPENDENCY";
    case GOGGLES_FC_STATUS_VALIDATION_ERROR:
        return "VALIDATION_ERROR";
    default:
        return "UNKNOWN_STATUS";
    }
}

bool GOGGLES_FC_CALL goggles_fc_is_success(goggles_fc_status_t status) {
    return status == GOGGLES_FC_STATUS_OK;
}

bool GOGGLES_FC_CALL goggles_fc_is_error(goggles_fc_status_t status) {
    return status != GOGGLES_FC_STATUS_OK;
}

/* ── Instance lifecycle ──────────────────────────────────────────────────── */

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_instance_create(
    const goggles_fc_instance_create_info_t* create_info, goggles_fc_instance_t** out_instance) {
    FC_API_GUARD_BEGIN
    auto status = validate_create_info(create_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_output(out_instance);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }

    auto* instance = Instance::allocate();
    if (instance == nullptr) {
        *out_instance = nullptr;
        return GOGGLES_FC_STATUS_OUT_OF_MEMORY;
    }

    if (create_info->log_callback != nullptr) {
        instance->set_log_callback(create_info->log_callback, create_info->log_user_data);
        // For single-instance use (the common case), also register as the global
        // macro log router so GOGGLES_LOG_* macros route through this callback.
        instance->activate_as_global_router();
    }

    *out_instance = instance->as_handle();
    return GOGGLES_FC_STATUS_OK;
    FC_API_GUARD_END
}

void GOGGLES_FC_CALL goggles_fc_instance_destroy(goggles_fc_instance_t* instance) {
    FC_API_VOID_GUARD_BEGIN
    if (instance == nullptr) {
        return;
    }
    delete Instance::from_handle(instance);
    FC_API_VOID_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_instance_set_log_callback(
    goggles_fc_instance_t* instance, goggles_fc_log_callback_t log_callback, void* user_data) {
    FC_API_GUARD_BEGIN
    auto status = validate_instance_handle(instance);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    auto* inst = Instance::from_handle(instance);
    status = inst->set_log_callback(log_callback, user_data);
    if (status == GOGGLES_FC_STATUS_OK) {
        // Also register as the global macro router so GOGGLES_LOG_* macros
        // route through this instance's callback.
        inst->activate_as_global_router();
    }
    return status;
    FC_API_GUARD_END
}

/* ── Device lifecycle ─────────────────────────────────────────────────────── */

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_device_create_vk(
    goggles_fc_instance_t* instance, const goggles_fc_vk_device_create_info_t* create_info,
    goggles_fc_device_t** out_device) {
    FC_API_GUARD_BEGIN
    auto status = validate_instance_handle(instance);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_vk_device_create_info(create_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_output(out_device);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Device::create(Instance::from_handle(instance), create_info, out_device);
    FC_API_GUARD_END
}

void GOGGLES_FC_CALL goggles_fc_device_destroy(goggles_fc_device_t* device) {
    FC_API_VOID_GUARD_BEGIN
    if (device == nullptr) {
        return;
    }
    delete Device::from_handle(device);
    FC_API_VOID_GUARD_END
}

/* ── Program lifecycle ───────────────────────────────────────────────────── */

goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_program_create(goggles_fc_device_t* device, const goggles_fc_preset_source_t* source,
                          goggles_fc_program_t** out_program) {
    FC_API_GUARD_BEGIN
    auto status = validate_device_handle(device);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_preset_source(source);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_output(out_program);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Program::create(Device::from_handle(device), source, out_program);
    FC_API_GUARD_END
}

void GOGGLES_FC_CALL goggles_fc_program_destroy(goggles_fc_program_t* program) {
    FC_API_VOID_GUARD_BEGIN
    if (program == nullptr) {
        return;
    }
    delete Program::from_handle(program);
    FC_API_VOID_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_program_get_source_info(
    const goggles_fc_program_t* program, goggles_fc_program_source_info_t* out_source_info) {
    FC_API_GUARD_BEGIN
    auto status = validate_program_handle(program);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_create_info(out_source_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Program::from_handle(program)->get_source_info(out_source_info);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_program_get_report(
    const goggles_fc_program_t* program, goggles_fc_program_report_t* out_report) {
    FC_API_GUARD_BEGIN
    auto status = validate_program_handle(program);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_create_info(out_report);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Program::from_handle(program)->get_report(out_report);
    FC_API_GUARD_END
}

/* ── Chain lifecycle ─────────────────────────────────────────────────────── */

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_create(
    goggles_fc_device_t* device, const goggles_fc_program_t* program,
    const goggles_fc_chain_create_info_t* create_info, goggles_fc_chain_t** out_chain) {
    FC_API_GUARD_BEGIN
    auto status = validate_device_handle(device);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_program_handle(program);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_chain_create_info(create_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_output(out_chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::create(Device::from_handle(device), Program::from_handle(program), create_info,
                         out_chain);
    FC_API_GUARD_END
}

void GOGGLES_FC_CALL goggles_fc_chain_destroy(goggles_fc_chain_t* chain) {
    FC_API_VOID_GUARD_BEGIN
    if (chain == nullptr) {
        return;
    }
    delete Chain::from_handle(chain);
    FC_API_VOID_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_bind_program(goggles_fc_chain_t* chain, const goggles_fc_program_t* program) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_program_handle(program);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->bind_program(Program::from_handle(program));
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_clear(goggles_fc_chain_t* chain) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->clear();
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_resize(
    goggles_fc_chain_t* chain, const goggles_fc_extent_2d_t* new_source_extent) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (new_source_extent == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    if (new_source_extent->width == 0 || new_source_extent->height == 0) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return Chain::from_handle(chain)->resize(new_source_extent);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_set_prechain_resolution(
    goggles_fc_chain_t* chain, const goggles_fc_extent_2d_t* resolution) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (resolution == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return Chain::from_handle(chain)->set_prechain_resolution(resolution);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_set_stage_mask(goggles_fc_chain_t* chain,
                                                                    uint32_t stage_mask) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->set_stage_mask(stage_mask);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_retarget(
    goggles_fc_chain_t* chain, const goggles_fc_chain_target_info_t* target_info) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_create_info(target_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (target_info->target_format == VK_FORMAT_UNDEFINED) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return Chain::from_handle(chain)->retarget(target_info);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_record_vk(
    goggles_fc_chain_t* chain, const goggles_fc_record_info_vk_t* record_info) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_record_info(record_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->record_vk(record_info);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_report(
    const goggles_fc_chain_t* chain, goggles_fc_chain_report_t* out_report) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_create_info(out_report);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->get_report(out_report);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_last_error(
    const goggles_fc_chain_t* chain, goggles_fc_chain_error_info_t* out_error) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_create_info(out_error);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->get_last_error(out_error);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_get_control_count(const goggles_fc_chain_t* chain, uint32_t* out_count) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (out_count == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return Chain::from_handle(chain)->get_control_count(out_count);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_control_info(
    const goggles_fc_chain_t* chain, uint32_t index, goggles_fc_control_info_t* out_control) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_create_info(out_control);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->get_control_info(index, out_control);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_find_control_index(const goggles_fc_chain_t* chain, uint32_t stage,
                                    goggles_fc_utf8_view_t name, uint32_t* out_index) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (out_index == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    status = validate_control_identity(stage, name);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->find_control_index(stage, {name.data, name.size}, out_index);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_set_control_value_f32(goggles_fc_chain_t* chain, uint32_t index, float value) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->set_control_value_f32(index, value);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_set_control_value_f32_by_name(
    goggles_fc_chain_t* chain, uint32_t stage, goggles_fc_utf8_view_t name, float value) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    status = validate_control_identity(stage, name);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->set_control_value_f32(stage, {name.data, name.size}, value);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_reset_control_value(goggles_fc_chain_t* chain,
                                                                         uint32_t index) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->reset_control_value(index);
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_reset_all_controls(goggles_fc_chain_t* chain) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    return Chain::from_handle(chain)->reset_all_controls();
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_prechain_resolution(
    const goggles_fc_chain_t* chain, goggles_fc_extent_2d_t* out_resolution) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (out_resolution == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return Chain::from_handle(chain)->get_prechain_resolution(out_resolution);
    FC_API_GUARD_END
}

/* ── Chain diagnostics ───────────────────────────────────────────────────── */

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_create_diagnostic_session(
    goggles_fc_chain_t* chain, const goggles_fc_diagnostic_policy_t* policy) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    Chain::from_handle(chain)->create_diagnostic_session(policy);
    return GOGGLES_FC_STATUS_OK;
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL
goggles_fc_chain_destroy_diagnostic_session(goggles_fc_chain_t* chain) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    Chain::from_handle(chain)->destroy_diagnostic_session();
    return GOGGLES_FC_STATUS_OK;
    FC_API_GUARD_END
}

goggles_fc_status_t GOGGLES_FC_CALL goggles_fc_chain_get_diagnostic_summary(
    const goggles_fc_chain_t* chain, goggles_fc_diagnostic_summary_t* out_summary) {
    FC_API_GUARD_BEGIN
    auto status = validate_chain_handle(chain);
    if (status != GOGGLES_FC_STATUS_OK) {
        return status;
    }
    if (out_summary == nullptr) {
        return GOGGLES_FC_STATUS_INVALID_ARGUMENT;
    }
    return Chain::from_handle(chain)->get_diagnostic_summary(out_summary);
    FC_API_GUARD_END
}

} // extern "C"
