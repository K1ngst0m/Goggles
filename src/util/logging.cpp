#include "logging.hpp"

#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace goggles {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
} // namespace

void initialize_logger(std::string_view app_name, bool is_layer) {
    if (g_logger) {
        return;
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    if (is_layer) {
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [goggles_vklayer] %v");
    } else {
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    }

    g_logger = std::make_shared<spdlog::logger>(std::string(app_name), console_sink);

#ifdef NDEBUG
    g_logger->set_level(spdlog::level::info);
#else
    g_logger->set_level(spdlog::level::debug);
#endif

    // Layer: restrict to error+ to avoid polluting host app output
    if (is_layer) {
        g_logger->set_level(spdlog::level::err);
    }

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

} // namespace goggles
