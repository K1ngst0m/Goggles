#include "logging.hpp"

#include "profiling.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace goggles {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> g_console_sink;
std::shared_ptr<spdlog::sinks::sink> g_file_sink;
bool g_timestamp_enabled = false;

constexpr auto CONSOLE_PATTERN = "[%^%l%$] %v";
constexpr auto CONSOLE_PATTERN_TIMESTAMP = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";
constexpr auto FILE_PATTERN = "[%l] %v";
constexpr auto FILE_PATTERN_TIMESTAMP = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";

auto remove_file_sink() -> void {
    if (!g_logger || !g_file_sink) {
        return;
    }

    auto& sinks = g_logger->sinks();
    sinks.erase(std::remove(sinks.begin(), sinks.end(), g_file_sink), sinks.end());
    g_file_sink.reset();
}

auto update_sink_patterns() -> void {
    if (!g_logger || !g_console_sink) {
        return;
    }

    g_console_sink->set_pattern(g_timestamp_enabled ? CONSOLE_PATTERN_TIMESTAMP : CONSOLE_PATTERN);

    if (g_file_sink) {
        g_file_sink->set_pattern(g_timestamp_enabled ? FILE_PATTERN_TIMESTAMP : FILE_PATTERN);
    }
}
} // namespace

void initialize_logger(std::string_view app_name) {
    GOGGLES_PROFILE_FUNCTION();
    if (g_logger) {
        return;
    }

    g_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    g_console_sink->set_pattern(CONSOLE_PATTERN);

    g_logger = std::make_shared<spdlog::logger>(std::string(app_name), g_console_sink);

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
    GOGGLES_PROFILE_FUNCTION();
    if (g_logger) {
        g_logger->set_level(level);
    }
}

void set_log_timestamp_enabled(bool enabled) {
    if (!g_logger) {
        return;
    }

    g_timestamp_enabled = enabled;
    update_sink_patterns();
}

auto set_log_file_path(const std::filesystem::path& path) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    if (!g_logger) {
        initialize_logger();
    }

    if (path.empty()) {
        remove_file_sink();
        return {};
    }

    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return make_error<void>(ErrorCode::file_write_failed,
                                    "Failed to create log directory '" + parent.string() +
                                        "': " + ec.message());
        }
    }

    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> new_sink;
    try {
        new_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
    } catch (const spdlog::spdlog_ex& e) {
        return make_error<void>(ErrorCode::file_write_failed,
                                "Failed to open log file '" + path.string() + "': " + e.what());
    }

    remove_file_sink();
    g_file_sink = new_sink;
    g_logger->sinks().push_back(g_file_sink);
    update_sink_patterns();
    return {};
}

} // namespace goggles
