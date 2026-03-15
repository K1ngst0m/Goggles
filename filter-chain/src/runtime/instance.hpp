#pragma once

#include "support/logging.hpp"

#include <cstdint>
#include <goggles_filter_chain.h>

namespace goggles::filter_chain::runtime {

/// @brief Internal instance implementation backing `goggles_fc_instance_t`.
///
/// Owns immutable library policy, replaceable log callback registration,
/// borrowed callback/user-data lifetime tracking, and synchronous log delivery
/// on the calling thread.
///
/// Each Instance owns its own LogRouter so multiple instances never share or
/// overwrite each other's callback registrations.
class Instance {
public:
    /// @brief Allocate and return a new Instance, or nullptr on OOM.
    [[nodiscard]] static auto allocate() -> Instance*;

    ~Instance();

    Instance(const Instance&) = delete;
    auto operator=(const Instance&) -> Instance& = delete;
    Instance(Instance&&) = delete;
    auto operator=(Instance&&) -> Instance& = delete;

    /// @brief Replace the log callback. Pass nullptr to disable callback routing.
    ///        The callback pointer and user_data are borrowed until the next replacement
    ///        or instance destruction.
    ///
    ///        This does NOT automatically register the instance as the global macro
    ///        log router. Call activate_as_global_router() separately if this instance
    ///        should also receive macro-based log output (GOGGLES_LOG_* macros).
    auto set_log_callback(goggles_fc_log_callback_t callback, void* user_data)
        -> goggles_fc_status_t;

    /// @brief Register this instance's log router as the active router for
    ///        macro-based logging (GOGGLES_LOG_* macros that cannot pass an Instance).
    ///
    ///        Only one instance can be the global router at a time. The last caller
    ///        wins. This is separate from set_log_callback so that multi-instance
    ///        hosts can control which instance receives macro-level diagnostics.
    void activate_as_global_router();

    /// @brief Deliver a log message synchronously on the calling thread.
    ///        Routes through the instance-owned log router. Falls back to the
    ///        library-private spdlog logger when no callback is registered.
    void log(goggles_fc_log_level_t level, const char* domain, const char* message) const;

    /// @brief Query the currently registered log callback.
    [[nodiscard]] auto log_callback() const -> goggles_fc_log_callback_t {
        return m_log_router.callback;
    }

    /// @brief Query the currently registered log user_data.
    [[nodiscard]] auto log_user_data() const -> void* { return m_log_router.user_data; }

    /// @brief Return a const pointer to the instance-owned log router.
    [[nodiscard]] auto log_router() const -> const detail::LogRouter* { return &m_log_router; }

    /// @brief Return the raw pointer suitable for casting to goggles_fc_instance_t*.
    [[nodiscard]] auto as_handle() -> goggles_fc_instance_t*;

    /// @brief Recover the Instance from an opaque handle.
    [[nodiscard]] static auto from_handle(goggles_fc_instance_t* handle) -> Instance*;
    [[nodiscard]] static auto from_handle(const goggles_fc_instance_t* handle) -> const Instance*;

    /// @brief Check whether the opaque handle points to a live Instance (magic validation).
    [[nodiscard]] static auto check_magic(const void* handle) -> bool {
        if (handle == nullptr) {
            return false;
        }
        return static_cast<const Instance*>(handle)->m_magic == INSTANCE_MAGIC;
    }

private:
    Instance() = default;

    detail::LogRouter m_log_router;

    static constexpr uint32_t INSTANCE_MAGIC = 0x47464349u; // "GFCI"
    uint32_t m_magic = INSTANCE_MAGIC;
};

} // namespace goggles::filter_chain::runtime
