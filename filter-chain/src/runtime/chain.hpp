#pragma once

#include <cstdint>
#include <goggles/filter_chain/filter_controls.hpp>
#include <goggles_filter_chain.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace goggles::diagnostics {
class DiagnosticSession;
struct DiagnosticPolicy;
} // namespace goggles::diagnostics

namespace goggles::render {
class ChainResources;
class ChainExecutor;
class ChainControls;
class ShaderRuntime;
} // namespace goggles::render

namespace goggles::filter_chain::runtime {

class Device;
class Program;

/// @brief Internal chain implementation backing `goggles_fc_chain_t`.
///
/// Owns executable runtime state for one program on one device: pipelines,
/// descriptors, intermediate images, control state, frame history, and
/// record-time validation. Tracks last-error and structured report state
/// separately from log text.
///
/// A Chain is created from a Device and a Program. The Program must be
/// affine to the same Device. Multiple chains may share the same Program
/// handle as long as they are all on the same device.
class Chain {
public:
    [[nodiscard]] static auto create(Device* device, const Program* program,
                                     const goggles_fc_chain_create_info_t* create_info,
                                     goggles_fc_chain_t** out_chain) -> goggles_fc_status_t;

    ~Chain();

    Chain(const Chain&) = delete;
    auto operator=(const Chain&) -> Chain& = delete;
    Chain(Chain&&) = delete;
    auto operator=(Chain&&) -> Chain& = delete;

    /// @brief Return the owning device.
    [[nodiscard]] auto device() const -> Device* { return m_device; }

    /// @brief Return the bound program.
    [[nodiscard]] auto program() const -> const Program* { return m_program; }

    /// @brief Bind a new program to this chain, replacing the old one.
    /// The program must be affine to the same device as this chain.
    [[nodiscard]] auto bind_program(const Program* program) -> goggles_fc_status_t;

    /// @brief Clear the chain state (reset frame history, etc.).
    [[nodiscard]] auto clear() -> goggles_fc_status_t;

    /// @brief Resize the chain's source resolution.
    [[nodiscard]] auto resize(const goggles_fc_extent_2d_t* new_source_extent)
        -> goggles_fc_status_t;

    /// @brief Update the prechain resolution without touching output sizing.
    [[nodiscard]] auto set_prechain_resolution(const goggles_fc_extent_2d_t* resolution)
        -> goggles_fc_status_t;

    /// @brief Update the active stage mask (prechain/effect/postchain enable bits).
    [[nodiscard]] auto set_stage_mask(uint32_t mask) -> goggles_fc_status_t;

    /// @brief Retarget the chain's output format.
    [[nodiscard]] auto retarget(const goggles_fc_chain_target_info_t* target_info)
        -> goggles_fc_status_t;

    /// @brief Record filter chain commands into a Vulkan command buffer.
    [[nodiscard]] auto record_vk(const goggles_fc_record_info_vk_t* record_info)
        -> goggles_fc_status_t;

    /// @brief Populate a chain report struct for the caller.
    [[nodiscard]] auto get_report(goggles_fc_chain_report_t* out) const -> goggles_fc_status_t;

    /// @brief Populate a chain error info struct for the caller.
    [[nodiscard]] auto get_last_error(goggles_fc_chain_error_info_t* out) const
        -> goggles_fc_status_t;

    /// @brief Get the number of controls in the chain.
    [[nodiscard]] auto get_control_count(uint32_t* out_count) const -> goggles_fc_status_t;

    /// @brief Get info about a specific control.
    [[nodiscard]] auto get_control_info(uint32_t index, goggles_fc_control_info_t* out) const
        -> goggles_fc_status_t;

    /// @brief Resolve the current index for a control identified by stage and name.
    [[nodiscard]] auto find_control_index(uint32_t stage, std::string_view name,
                                          uint32_t* out_index) const -> goggles_fc_status_t;

    /// @brief Set a control value by index.
    [[nodiscard]] auto set_control_value_f32(uint32_t index, float value) -> goggles_fc_status_t;

    /// @brief Set a control value by stage and name.
    [[nodiscard]] auto set_control_value_f32(uint32_t stage, std::string_view name, float value)
        -> goggles_fc_status_t;

    /// @brief Reset a single control to its default value by index.
    [[nodiscard]] auto reset_control_value(uint32_t index) -> goggles_fc_status_t;

    /// @brief Reset all controls to their default values without resetting the frame counter.
    [[nodiscard]] auto reset_all_controls() -> goggles_fc_status_t;

    /// @brief Create and attach a diagnostic session with the given policy.
    /// If a session already exists, it is destroyed first.
    void create_diagnostic_session(const goggles_fc_diagnostic_policy_t* policy);

    /// @brief Destroy and detach the diagnostic session.
    void destroy_diagnostic_session();

    /// @brief Return the current diagnostic session, or nullptr.
    [[nodiscard]] auto diagnostic_session() const -> goggles::diagnostics::DiagnosticSession*;

    /// @brief Populate a diagnostic summary struct for the caller.
    [[nodiscard]] auto get_diagnostic_summary(goggles_fc_diagnostic_summary_t* out) const
        -> goggles_fc_status_t;

    /// @brief Get the current prechain resolution.
    [[nodiscard]] auto get_prechain_resolution(goggles_fc_extent_2d_t* out) const
        -> goggles_fc_status_t;

    /// @brief Return the raw pointer suitable for casting to goggles_fc_chain_t*.
    [[nodiscard]] auto as_handle() -> goggles_fc_chain_t*;

    /// @brief Recover the Chain from an opaque handle.
    [[nodiscard]] static auto from_handle(goggles_fc_chain_t* handle) -> Chain*;
    [[nodiscard]] static auto from_handle(const goggles_fc_chain_t* handle) -> const Chain*;

    /// @brief Check whether the opaque handle points to a live Chain (magic validation).
    [[nodiscard]] static auto check_magic(const void* handle) -> bool {
        if (handle == nullptr) {
            return false;
        }
        return static_cast<const Chain*>(handle)->m_magic == CHAIN_MAGIC;
    }

private:
    Chain() = default;

    /// @brief Record the last error for structured error reporting.
    void set_last_error(goggles_fc_status_t status, int32_t vk_result = 0,
                        uint32_t subsystem_code = 0) const;

    /// @brief Refresh cached control descriptors from resources.
    void refresh_controls() const;

    [[nodiscard]] auto find_control_index_impl(uint32_t stage, std::string_view name) const
        -> std::optional<uint32_t>;

    Device* m_device = nullptr;
    const Program* m_program = nullptr;

    std::unique_ptr<goggles::render::ChainResources> m_resources;
    std::unique_ptr<goggles::render::ShaderRuntime> m_shader_runtime;
    std::unique_ptr<goggles::render::ChainExecutor> m_executor;
    std::unique_ptr<goggles::render::ChainControls> m_controls;

    // Diagnostics (optional, created on demand)
    std::unique_ptr<goggles::diagnostics::DiagnosticSession> m_diagnostic_session;

    // Chain configuration
    uint32_t m_frames_in_flight = 1;
    uint32_t m_stage_mask = GOGGLES_FC_STAGE_MASK_ALL;
    uint32_t m_frames_rendered = 0;

    // Cached control descriptors for structured query (mutable for const lazy-refresh pattern)
    mutable std::vector<goggles::render::FilterControlDescriptor> m_cached_controls;
    mutable bool m_controls_dirty = true;

    // Last error tracking for structured error reporting (mutable for const method error paths)
    mutable goggles_fc_chain_error_info_t m_last_error{};

    static constexpr uint32_t CHAIN_MAGIC = 0x47464343u; // "GFCC"
    uint32_t m_magic = CHAIN_MAGIC;
};

} // namespace goggles::filter_chain::runtime
