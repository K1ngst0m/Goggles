#include "logging.hpp"

#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace goggles {

namespace {
std::shared_ptr<spdlog::logger> g_logger;

auto update_console_pattern(bool timestamp_enabled) -> void {
    if (!g_logger) {
        return;
    }

    auto sinks = g_logger->sinks();
    if (sinks.empty()) {
        return;
    }

    const auto& sink = sinks.front();

    if (timestamp_enabled) {
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        return;
    }

    sink->set_pattern("[%^%l%$] %v");
}
} // namespace

void initialize_logger(std::string_view app_name) {
    if (g_logger) {
        return;
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    console_sink->set_pattern("[%^%l%$] %v");

    g_logger = std::make_shared<spdlog::logger>(std::string(app_name), console_sink);

#ifdef NDEBUG
    g_logger->set_level(spdlog::level::info);
#else
    g_logger->set_level(spdlog::level::debug);
#endif

    g_logger->flush_on(spdlog::level::err);
    spdlog::set_default_logger(g_logger);
}

auto get_logger() -> std::shared_ptr<spdlog::logger> {
    if (!g_logger) {
        initialize_logger();
    }
    return g_logger;
}

void set_log_level(spdlog::level::level_enum level) {
    if (g_logger) {
        g_logger->set_level(level);
    }
}

void set_log_timestamp_enabled(bool enabled) {
    if (!g_logger) {
        return;
    }

    update_console_pattern(enabled);
}

} // namespace goggles
