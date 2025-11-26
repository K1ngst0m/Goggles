#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace goggles {

// Initialize the global logger
// Should be called once at application startup
// For capture layer, call with is_layer=true to adjust format and limits
void initialize_logger(std::string_view app_name = "goggles", bool is_layer = false);

// Get the global logger (for advanced usage)
[[nodiscard]] auto get_logger() -> std::shared_ptr<spdlog::logger>;

// Set log level at runtime
void set_log_level(spdlog::level::level_enum level);

} // namespace goggles

// Project-wide logging macros
// These wrap spdlog and use the global logger

#define GOGGLES_LOG_TRACE(...) SPDLOG_LOGGER_TRACE(::goggles::get_logger(), __VA_ARGS__)

#define GOGGLES_LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(::goggles::get_logger(), __VA_ARGS__)

#define GOGGLES_LOG_INFO(...) SPDLOG_LOGGER_INFO(::goggles::get_logger(), __VA_ARGS__)

#define GOGGLES_LOG_WARN(...) SPDLOG_LOGGER_WARN(::goggles::get_logger(), __VA_ARGS__)

#define GOGGLES_LOG_ERROR(...) SPDLOG_LOGGER_ERROR(::goggles::get_logger(), __VA_ARGS__)

#define GOGGLES_LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::goggles::get_logger(), __VA_ARGS__)
