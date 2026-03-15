#include "logging.hpp"

#include <atomic>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace goggles::filter_chain::detail {

namespace {

/// @brief Library-private spdlog logger, never installed as the process default.
/// Used only as a fallback when no host callback is registered.
std::shared_ptr<spdlog::logger> g_library_logger;

/// @brief The currently active log router for macro-based logging.
/// Points to an Instance-owned LogRouter when one is registered.
std::atomic<const LogRouter*> g_active_router{nullptr};

thread_local const LogRouter* t_scoped_router = nullptr;

constexpr auto CONSOLE_PATTERN = "[%^%l%$] %v";

auto ensure_library_logger() -> std::shared_ptr<spdlog::logger> {
    if (!g_library_logger) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(CONSOLE_PATTERN);
        spdlog::sinks_init_list sinks = {console_sink};
        g_library_logger = std::make_shared<spdlog::logger>("goggles-filter-chain", sinks);
#ifdef NDEBUG
        g_library_logger->set_level(spdlog::level::info);
#else
        g_library_logger->set_level(spdlog::level::debug);
#endif
        g_library_logger->flush_on(spdlog::level::err);
        // Deliberately NOT calling spdlog::set_default_logger — this logger
        // is library-private and must not affect process-global state.
    }
    return g_library_logger;
}

auto fc_level_to_spdlog(goggles_fc_log_level_t level) -> spdlog::level::level_enum {
    switch (level) {
    case GOGGLES_FC_LOG_LEVEL_TRACE:
        return spdlog::level::trace;
    case GOGGLES_FC_LOG_LEVEL_DEBUG:
        return spdlog::level::debug;
    case GOGGLES_FC_LOG_LEVEL_INFO:
        return spdlog::level::info;
    case GOGGLES_FC_LOG_LEVEL_WARN:
        return spdlog::level::warn;
    case GOGGLES_FC_LOG_LEVEL_ERROR:
        return spdlog::level::err;
    case GOGGLES_FC_LOG_LEVEL_CRITICAL:
        return spdlog::level::critical;
    default:
        return spdlog::level::info;
    }
}

void deliver_via_spdlog(goggles_fc_log_level_t level, std::string_view domain,
                        std::string_view message) {
    auto logger = ensure_library_logger();
    auto spdlog_level = fc_level_to_spdlog(level);
    if (!domain.empty()) {
        logger->log(spdlog_level, "[{}] {}", domain, message);
    } else {
        logger->log(spdlog_level, "{}", message);
    }
}

} // namespace

ScopedLogRouter::ScopedLogRouter(const LogRouter* router) : m_previous_router(t_scoped_router) {
    t_scoped_router = router;
}

ScopedLogRouter::~ScopedLogRouter() {
    t_scoped_router = m_previous_router;
}

void log_route(const LogRouter* router, goggles_fc_log_level_t level, std::string_view domain,
               std::string_view message) {
    if (router != nullptr && router->callback != nullptr) {
        goggles_fc_log_message_t msg = goggles_fc_log_message_init();
        msg.level = level;
        msg.domain.data = domain.data();
        msg.domain.size = domain.size();
        msg.message.data = message.data();
        msg.message.size = message.size();
        router->callback(&msg, router->user_data);
        return;
    }

    // Fallback to the library-private spdlog logger.
    deliver_via_spdlog(level, domain, message);
}

void log_route(goggles_fc_log_level_t level, std::string_view domain, std::string_view message) {
    const LogRouter* router = t_scoped_router != nullptr
                                  ? t_scoped_router
                                  : g_active_router.load(std::memory_order_acquire);
    log_route(router, level, domain, message);
}

void log_route_set_active(const LogRouter* router) {
    g_active_router.store(router, std::memory_order_release);
}

auto log_route_get_active() -> const LogRouter* {
    return g_active_router.load(std::memory_order_acquire);
}

auto log_route_get_scoped() -> const LogRouter* {
    return t_scoped_router;
}

} // namespace goggles::filter_chain::detail
