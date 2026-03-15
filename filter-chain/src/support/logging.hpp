#pragma once

#include <format>
#include <goggles/filter_chain/error.hpp>
#include <goggles_filter_chain.h>
#include <string>
#include <string_view>

namespace goggles::filter_chain::detail {

/// @brief Instance-owned log-router state.
///
/// Each Instance holds its own LogRouter so multiple instances never share or
/// overwrite each other's callback registrations. The router delivers messages
/// synchronously on the calling thread. When no callback is registered, the
/// router forwards to an internal spdlog logger that is library-private and
/// never installed as the process-global spdlog default.
struct LogRouter {
    goggles_fc_log_callback_t callback = nullptr;
    void* user_data = nullptr;
};

class ScopedLogRouter {
public:
    explicit ScopedLogRouter(const LogRouter* router);
    ~ScopedLogRouter();

    ScopedLogRouter(const ScopedLogRouter&) = delete;
    auto operator=(const ScopedLogRouter&) -> ScopedLogRouter& = delete;
    ScopedLogRouter(ScopedLogRouter&&) = delete;
    auto operator=(ScopedLogRouter&&) -> ScopedLogRouter& = delete;

private:
    const LogRouter* m_previous_router = nullptr;
};

/// @brief Deliver a log message through a specific log router.
///
/// If the router has a callback, the message is delivered through it. Otherwise
/// the library-private spdlog logger is used as a fallback.
///
/// @param router  The instance-owned log router (may be nullptr for fallback-only).
/// @param level   Log severity level (GOGGLES_FC_LOG_LEVEL_* value).
/// @param domain  Domain tag for the log message.
/// @param message The log text.
void log_route(const LogRouter* router, goggles_fc_log_level_t level, std::string_view domain,
               std::string_view message);

/// @brief Convenience overload: deliver through the globally-registered active router.
///
/// This is used by the GOGGLES_LOG_* macros which do not carry an Instance pointer.
/// It first routes through any thread-local scoped router active for the current
/// API call, then falls back to the process-wide active router (or the library
/// logger if none is active).
void log_route(goggles_fc_log_level_t level, std::string_view domain, std::string_view message);

/// @brief Register an Instance's log router as the active router for macro-based logging.
///
/// This allows GOGGLES_LOG_* macros (which cannot pass an Instance pointer) to route
/// through the most recently registered Instance's callback. Passing nullptr restores
/// fallback-only behavior.
void log_route_set_active(const LogRouter* router);

/// @brief Query the currently active log router (may be nullptr).
[[nodiscard]] auto log_route_get_active() -> const LogRouter*;

/// @brief Return the currently scoped router for this thread, if any.
[[nodiscard]] auto log_route_get_scoped() -> const LogRouter*;

} // namespace goggles::filter_chain::detail

/// @brief Safe formatting wrapper that catches exceptions from std::format.
/// On formatting failure, returns a fallback error indicator.
/// This prevents exceptions from propagating through C ABI boundaries.
namespace goggles::filter_chain::detail {

template <typename... Args>
auto safe_format(std::format_string<Args...> fmt, Args&&... args) noexcept -> std::string {
    try {
        return std::format(fmt, std::forward<Args>(args)...);
    } catch (...) {
        return "<log format error>";
    }
}

} // namespace goggles::filter_chain::detail

#ifdef GOGGLES_LOG_TAG
#define GOGGLES_LOG_DOMAIN GOGGLES_LOG_TAG
#else
#define GOGGLES_LOG_DOMAIN "filter-chain"
#endif

// Internal log macros route ALL messages through the instance-owned log router.
// When a host callback is registered, messages are delivered through it.
// When no callback is registered, messages fall back to the library-private
// spdlog logger. These macros NEVER touch the process-global spdlog default.

#define GOGGLES_LOG_TRACE(...)                                                                     \
    ::goggles::filter_chain::detail::log_route(                                                    \
        GOGGLES_FC_LOG_LEVEL_TRACE, GOGGLES_LOG_DOMAIN,                                            \
        ::goggles::filter_chain::detail::safe_format(__VA_ARGS__))

#define GOGGLES_LOG_DEBUG(...)                                                                     \
    ::goggles::filter_chain::detail::log_route(                                                    \
        GOGGLES_FC_LOG_LEVEL_DEBUG, GOGGLES_LOG_DOMAIN,                                            \
        ::goggles::filter_chain::detail::safe_format(__VA_ARGS__))

#define GOGGLES_LOG_INFO(...)                                                                      \
    ::goggles::filter_chain::detail::log_route(                                                    \
        GOGGLES_FC_LOG_LEVEL_INFO, GOGGLES_LOG_DOMAIN,                                             \
        ::goggles::filter_chain::detail::safe_format(__VA_ARGS__))

#define GOGGLES_LOG_WARN(...)                                                                      \
    ::goggles::filter_chain::detail::log_route(                                                    \
        GOGGLES_FC_LOG_LEVEL_WARN, GOGGLES_LOG_DOMAIN,                                             \
        ::goggles::filter_chain::detail::safe_format(__VA_ARGS__))

#define GOGGLES_LOG_ERROR(...)                                                                     \
    ::goggles::filter_chain::detail::log_route(                                                    \
        GOGGLES_FC_LOG_LEVEL_ERROR, GOGGLES_LOG_DOMAIN,                                            \
        ::goggles::filter_chain::detail::safe_format(__VA_ARGS__))

#define GOGGLES_LOG_CRITICAL(...)                                                                  \
    ::goggles::filter_chain::detail::log_route(                                                    \
        GOGGLES_FC_LOG_LEVEL_CRITICAL, GOGGLES_LOG_DOMAIN,                                         \
        ::goggles::filter_chain::detail::safe_format(__VA_ARGS__))
