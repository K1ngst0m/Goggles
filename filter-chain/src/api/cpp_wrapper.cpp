#include <goggles/filter_chain/api.hpp>
#include <utility>

namespace goggles::filter_chain {

namespace {

auto fc_status_to_error_code(goggles_fc_status_t status) -> goggles::ErrorCode {
    switch (status) {
    case GOGGLES_FC_STATUS_INVALID_ARGUMENT:
    case GOGGLES_FC_STATUS_VALIDATION_ERROR:
        return goggles::ErrorCode::invalid_data;
    case GOGGLES_FC_STATUS_NOT_INITIALIZED:
    case GOGGLES_FC_STATUS_INVALID_DEPENDENCY:
        return goggles::ErrorCode::vulkan_init_failed;
    case GOGGLES_FC_STATUS_NOT_FOUND:
        return goggles::ErrorCode::file_not_found;
    case GOGGLES_FC_STATUS_PRESET_ERROR:
        return goggles::ErrorCode::shader_load_failed;
    case GOGGLES_FC_STATUS_IO_ERROR:
        return goggles::ErrorCode::file_read_failed;
    case GOGGLES_FC_STATUS_VULKAN_ERROR:
        return goggles::ErrorCode::vulkan_init_failed;
    case GOGGLES_FC_STATUS_NOT_SUPPORTED:
        return goggles::ErrorCode::invalid_data;
    case GOGGLES_FC_STATUS_OUT_OF_MEMORY:
    case GOGGLES_FC_STATUS_RUNTIME_ERROR:
    case GOGGLES_FC_STATUS_OK:
    default:
        return goggles::ErrorCode::unknown_error;
    }
}

auto status_to_result(goggles_fc_status_t status, const char* context) -> goggles::Result<void> {
    if (status == GOGGLES_FC_STATUS_OK) {
        return {};
    }
    return goggles::make_error<void>(fc_status_to_error_code(status),
                                     std::string(context) + ": " +
                                         goggles_fc_status_string(status));
}

} // namespace

/* ── Instance ────────────────────────────────────────────────────────────── */

Instance::~Instance() {
    if (m_handle != nullptr) {
        goggles_fc_instance_destroy(m_handle);
    }
}

Instance::Instance(Instance&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

auto Instance::operator=(Instance&& other) noexcept -> Instance& {
    if (this != &other) {
        if (m_handle != nullptr) {
            goggles_fc_instance_destroy(m_handle);
        }
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

auto Instance::create(const goggles_fc_instance_create_info_t* create_info)
    -> goggles::Result<Instance> {
    goggles_fc_instance_t* handle = nullptr;
    const auto status = goggles_fc_instance_create(create_info, &handle);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<Instance>(fc_status_to_error_code(status),
                                             std::string("Failed to create instance: ") +
                                                 goggles_fc_status_string(status));
    }
    return Instance{handle};
}

auto Instance::set_log_callback(goggles_fc_log_callback_t callback, void* user_data)
    -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Instance not initialized");
    }
    return status_to_result(goggles_fc_instance_set_log_callback(m_handle, callback, user_data),
                            "Failed to set log callback");
}

/* ── Device ──────────────────────────────────────────────────────────────── */

Device::~Device() {
    if (m_handle != nullptr) {
        goggles_fc_device_destroy(m_handle);
    }
}

Device::Device(Device&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

auto Device::operator=(Device&& other) noexcept -> Device& {
    if (this != &other) {
        if (m_handle != nullptr) {
            goggles_fc_device_destroy(m_handle);
        }
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

auto Device::create(Instance& instance, const goggles_fc_vk_device_create_info_t* create_info)
    -> goggles::Result<Device> {
    goggles_fc_device_t* handle = nullptr;
    const auto status = goggles_fc_device_create_vk(instance.handle(), create_info, &handle);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<Device>(fc_status_to_error_code(status),
                                           std::string("Failed to create device: ") +
                                               goggles_fc_status_string(status));
    }
    return Device{handle};
}

/* ── Program ─────────────────────────────────────────────────────────────── */

Program::~Program() {
    if (m_handle != nullptr) {
        goggles_fc_program_destroy(m_handle);
    }
}

Program::Program(Program&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

auto Program::operator=(Program&& other) noexcept -> Program& {
    if (this != &other) {
        if (m_handle != nullptr) {
            goggles_fc_program_destroy(m_handle);
        }
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

auto Program::create(Device& device, const goggles_fc_preset_source_t* source)
    -> goggles::Result<Program> {
    goggles_fc_program_t* handle = nullptr;
    const auto status = goggles_fc_program_create(device.handle(), source, &handle);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<Program>(fc_status_to_error_code(status),
                                            std::string("Failed to create program: ") +
                                                goggles_fc_status_string(status));
    }
    return Program{handle};
}

auto Program::get_source_info() const -> goggles::Result<goggles_fc_program_source_info_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<goggles_fc_program_source_info_t>(
            goggles::ErrorCode::vulkan_init_failed, "Program not initialized");
    }
    auto info = goggles_fc_program_source_info_init();
    const auto status = goggles_fc_program_get_source_info(m_handle, &info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<goggles_fc_program_source_info_t>(
            fc_status_to_error_code(status),
            std::string("Failed to get program source info: ") + goggles_fc_status_string(status));
    }
    return info;
}

auto Program::get_report() const -> goggles::Result<goggles_fc_program_report_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<goggles_fc_program_report_t>(
            goggles::ErrorCode::vulkan_init_failed, "Program not initialized");
    }
    auto report = goggles_fc_program_report_init();
    const auto status = goggles_fc_program_get_report(m_handle, &report);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<goggles_fc_program_report_t>(
            fc_status_to_error_code(status),
            std::string("Failed to get program report: ") + goggles_fc_status_string(status));
    }
    return report;
}

/* ── Chain ───────────────────────────────────────────────────────────────── */

Chain::~Chain() {
    if (m_handle != nullptr) {
        goggles_fc_chain_destroy(m_handle);
    }
}

Chain::Chain(Chain&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

auto Chain::operator=(Chain&& other) noexcept -> Chain& {
    if (this != &other) {
        if (m_handle != nullptr) {
            goggles_fc_chain_destroy(m_handle);
        }
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

auto Chain::create(Device& device, const Program& program,
                   const goggles_fc_chain_create_info_t* create_info) -> goggles::Result<Chain> {
    goggles_fc_chain_t* handle = nullptr;
    const auto status =
        goggles_fc_chain_create(device.handle(), program.handle(), create_info, &handle);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<Chain>(fc_status_to_error_code(status),
                                          std::string("Failed to create chain: ") +
                                              goggles_fc_status_string(status));
    }
    return Chain{handle};
}

auto Chain::bind_program(const Program& program) -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_bind_program(m_handle, program.handle()),
                            "Failed to bind program");
}

auto Chain::clear() -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_clear(m_handle), "Failed to clear chain");
}

auto Chain::resize(const goggles_fc_extent_2d_t* new_source_extent) -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_resize(m_handle, new_source_extent),
                            "Failed to resize chain");
}

auto Chain::set_prechain_resolution(const goggles_fc_extent_2d_t* resolution)
    -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_set_prechain_resolution(m_handle, resolution),
                            "Failed to set prechain resolution");
}

auto Chain::set_stage_mask(uint32_t mask) -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_set_stage_mask(m_handle, mask),
                            "Failed to set stage mask");
}

auto Chain::retarget(const goggles_fc_chain_target_info_t* target_info) -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_retarget(m_handle, target_info),
                            "Failed to retarget chain");
}

auto Chain::record_vk(const goggles_fc_record_info_vk_t* record_info) -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_record_vk(m_handle, record_info),
                            "Failed to record chain");
}

auto Chain::get_report() const -> goggles::Result<goggles_fc_chain_report_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<goggles_fc_chain_report_t>(
            goggles::ErrorCode::vulkan_init_failed, "Chain not initialized");
    }
    auto report = goggles_fc_chain_report_init();
    const auto status = goggles_fc_chain_get_report(m_handle, &report);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<goggles_fc_chain_report_t>(
            fc_status_to_error_code(status),
            std::string("Failed to get chain report: ") + goggles_fc_status_string(status));
    }
    return report;
}

auto Chain::get_last_error() const -> goggles::Result<goggles_fc_chain_error_info_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<goggles_fc_chain_error_info_t>(
            goggles::ErrorCode::vulkan_init_failed, "Chain not initialized");
    }
    auto error_info = goggles_fc_chain_error_info_init();
    const auto status = goggles_fc_chain_get_last_error(m_handle, &error_info);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<goggles_fc_chain_error_info_t>(
            fc_status_to_error_code(status),
            std::string("Failed to get chain last error: ") + goggles_fc_status_string(status));
    }
    return error_info;
}

auto Chain::get_control_count() const -> goggles::Result<uint32_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<uint32_t>(goggles::ErrorCode::vulkan_init_failed,
                                             "Chain not initialized");
    }
    uint32_t count = 0;
    const auto status = goggles_fc_chain_get_control_count(m_handle, &count);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<uint32_t>(fc_status_to_error_code(status),
                                             std::string("Failed to get control count: ") +
                                                 goggles_fc_status_string(status));
    }
    return count;
}

auto Chain::find_control_index(Stage stage, std::string_view name) const
    -> goggles::Result<uint32_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<uint32_t>(goggles::ErrorCode::vulkan_init_failed,
                                             "Chain not initialized");
    }
    uint32_t index = 0;
    const auto utf8_name = goggles_fc_utf8_view_t{.data = name.data(), .size = name.size()};
    const auto status = goggles_fc_chain_find_control_index(m_handle, static_cast<uint32_t>(stage),
                                                            utf8_name, &index);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<uint32_t>(fc_status_to_error_code(status),
                                             std::string("Failed to find control index: ") +
                                                 goggles_fc_status_string(status));
    }
    return index;
}

auto Chain::get_control_info(uint32_t index) const -> goggles::Result<goggles_fc_control_info_t> {
    if (m_handle == nullptr) {
        return goggles::make_error<goggles_fc_control_info_t>(
            goggles::ErrorCode::vulkan_init_failed, "Chain not initialized");
    }
    auto control = goggles_fc_control_info_init();
    const auto status = goggles_fc_chain_get_control_info(m_handle, index, &control);
    if (status != GOGGLES_FC_STATUS_OK) {
        return goggles::make_error<goggles_fc_control_info_t>(
            fc_status_to_error_code(status),
            std::string("Failed to get control info: ") + goggles_fc_status_string(status));
    }
    return control;
}

auto Chain::set_control_value_f32(uint32_t index, float value) -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    return status_to_result(goggles_fc_chain_set_control_value_f32(m_handle, index, value),
                            "Failed to set control value");
}

auto Chain::set_control_value_f32(Stage stage, std::string_view name, float value)
    -> goggles::Result<void> {
    if (m_handle == nullptr) {
        return goggles::make_error<void>(goggles::ErrorCode::vulkan_init_failed,
                                         "Chain not initialized");
    }
    const auto utf8_name = goggles_fc_utf8_view_t{.data = name.data(), .size = name.size()};
    return status_to_result(goggles_fc_chain_set_control_value_f32_by_name(
                                m_handle, static_cast<uint32_t>(stage), utf8_name, value),
                            "Failed to set control value");
}

} // namespace goggles::filter_chain
