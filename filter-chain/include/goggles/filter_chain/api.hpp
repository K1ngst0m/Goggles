#pragma once

#include <cstdint>
#include <goggles/filter_chain/common.hpp>
#include <goggles/filter_chain/error.hpp>
#include <goggles_filter_chain.h>
#include <string>
#include <string_view>

namespace goggles::filter_chain {

/// @brief RAII wrapper for `goggles_fc_instance_t`.
class Instance {
public:
    Instance() = default;
    ~Instance();

    Instance(const Instance&) = delete;
    auto operator=(const Instance&) -> Instance& = delete;

    Instance(Instance&& other) noexcept;
    auto operator=(Instance&& other) noexcept -> Instance&;

    [[nodiscard]] static auto create(const goggles_fc_instance_create_info_t* create_info)
        -> goggles::Result<Instance>;

    [[nodiscard]] auto set_log_callback(goggles_fc_log_callback_t callback, void* user_data)
        -> goggles::Result<void>;

    [[nodiscard]] auto handle() const -> goggles_fc_instance_t* { return m_handle; }
    [[nodiscard]] explicit operator bool() const { return m_handle != nullptr; }

private:
    explicit Instance(goggles_fc_instance_t* handle) : m_handle(handle) {}
    goggles_fc_instance_t* m_handle = nullptr;
};

/// @brief RAII wrapper for `goggles_fc_device_t`.
class Device {
public:
    Device() = default;
    ~Device();

    Device(const Device&) = delete;
    auto operator=(const Device&) -> Device& = delete;

    Device(Device&& other) noexcept;
    auto operator=(Device&& other) noexcept -> Device&;

    [[nodiscard]] static auto create(Instance& instance,
                                     const goggles_fc_vk_device_create_info_t* create_info)
        -> goggles::Result<Device>;

    [[nodiscard]] auto handle() const -> goggles_fc_device_t* { return m_handle; }
    [[nodiscard]] explicit operator bool() const { return m_handle != nullptr; }

private:
    explicit Device(goggles_fc_device_t* handle) : m_handle(handle) {}
    goggles_fc_device_t* m_handle = nullptr;
};

/// @brief RAII wrapper for `goggles_fc_program_t`.
class Program {
public:
    Program() = default;
    ~Program();

    Program(const Program&) = delete;
    auto operator=(const Program&) -> Program& = delete;

    Program(Program&& other) noexcept;
    auto operator=(Program&& other) noexcept -> Program&;

    [[nodiscard]] static auto create(Device& device, const goggles_fc_preset_source_t* source)
        -> goggles::Result<Program>;

    [[nodiscard]] auto get_source_info() const -> goggles::Result<goggles_fc_program_source_info_t>;
    [[nodiscard]] auto get_report() const -> goggles::Result<goggles_fc_program_report_t>;

    [[nodiscard]] auto handle() const -> goggles_fc_program_t* { return m_handle; }
    [[nodiscard]] explicit operator bool() const { return m_handle != nullptr; }

private:
    explicit Program(goggles_fc_program_t* handle) : m_handle(handle) {}
    goggles_fc_program_t* m_handle = nullptr;
};

/// @brief RAII wrapper for `goggles_fc_chain_t`.
class Chain {
public:
    Chain() = default;
    ~Chain();

    Chain(const Chain&) = delete;
    auto operator=(const Chain&) -> Chain& = delete;

    Chain(Chain&& other) noexcept;
    auto operator=(Chain&& other) noexcept -> Chain&;

    [[nodiscard]] static auto create(Device& device, const Program& program,
                                     const goggles_fc_chain_create_info_t* create_info)
        -> goggles::Result<Chain>;

    [[nodiscard]] auto bind_program(const Program& program) -> goggles::Result<void>;
    [[nodiscard]] auto clear() -> goggles::Result<void>;
    [[nodiscard]] auto resize(const goggles_fc_extent_2d_t* new_source_extent)
        -> goggles::Result<void>;
    [[nodiscard]] auto set_prechain_resolution(const goggles_fc_extent_2d_t* resolution)
        -> goggles::Result<void>;
    [[nodiscard]] auto set_stage_mask(uint32_t mask) -> goggles::Result<void>;
    [[nodiscard]] auto retarget(const goggles_fc_chain_target_info_t* target_info)
        -> goggles::Result<void>;
    [[nodiscard]] auto record_vk(const goggles_fc_record_info_vk_t* record_info)
        -> goggles::Result<void>;

    [[nodiscard]] auto get_report() const -> goggles::Result<goggles_fc_chain_report_t>;
    [[nodiscard]] auto get_last_error() const -> goggles::Result<goggles_fc_chain_error_info_t>;

    [[nodiscard]] auto get_control_count() const -> goggles::Result<uint32_t>;
    [[nodiscard]] auto find_control_index(Stage stage, std::string_view name) const
        -> goggles::Result<uint32_t>;
    [[nodiscard]] auto get_control_info(uint32_t index) const
        -> goggles::Result<goggles_fc_control_info_t>;
    [[nodiscard]] auto set_control_value_f32(uint32_t index, float value) -> goggles::Result<void>;
    [[nodiscard]] auto set_control_value_f32(Stage stage, std::string_view name, float value)
        -> goggles::Result<void>;

    [[nodiscard]] auto handle() const -> goggles_fc_chain_t* { return m_handle; }
    [[nodiscard]] explicit operator bool() const { return m_handle != nullptr; }

private:
    explicit Chain(goggles_fc_chain_t* handle) : m_handle(handle) {}
    goggles_fc_chain_t* m_handle = nullptr;
};

/// @brief Return the packed API semantic version.
[[nodiscard]] inline auto get_api_version() -> uint32_t {
    return goggles_fc_get_api_version();
}

/// @brief Return the ABI major version.
[[nodiscard]] inline auto get_abi_version() -> uint32_t {
    return goggles_fc_get_abi_version();
}

/// @brief Query capability flags.
[[nodiscard]] inline auto get_capabilities() -> goggles_fc_capability_flags_t {
    return goggles_fc_get_capabilities();
}

/// @brief Map a status code to a human-readable string.
[[nodiscard]] inline auto status_string(goggles_fc_status_t status) -> const char* {
    return goggles_fc_status_string(status);
}

} // namespace goggles::filter_chain
