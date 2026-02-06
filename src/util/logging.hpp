#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include "error.hpp"

#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace goggles {

/// @brief Initializes the global logger.
/// @param app_name Application name used in log formatting.
void initialize_logger(std::string_view app_name = "goggles");
/// @brief Returns the global logger instance.
[[nodiscard]] auto get_logger() -> std::shared_ptr<spdlog::logger>;
/// @brief Sets the global logger verbosity level.
/// @param level New verbosity threshold.
void set_log_level(spdlog::level::level_enum level);
/// @brief Enables or disables timestamps in log output.
/// @param enabled True to include timestamps in formatted log lines.
void set_log_timestamp_enabled(bool enabled);
/// @brief Enables file logging to the provided path, replacing any previous file sink.
/// @param path Target file path. Empty path disables file logging.
/// @return Success or error when the file sink cannot be created.
[[nodiscard]] auto set_log_file_path(const std::filesystem::path& path) -> Result<void>;

} // namespace goggles

#ifdef GOGGLES_LOG_TAG
#define GOGGLES_LOG_TAG_PREFIX "[" GOGGLES_LOG_TAG "] "
#else
#define GOGGLES_LOG_TAG_PREFIX ""
#endif

#define GOGGLES_LOG_TRACE(...)                                                                     \
    SPDLOG_LOGGER_TRACE(::goggles::get_logger(), GOGGLES_LOG_TAG_PREFIX __VA_ARGS__)

#define GOGGLES_LOG_DEBUG(...)                                                                     \
    SPDLOG_LOGGER_DEBUG(::goggles::get_logger(), GOGGLES_LOG_TAG_PREFIX __VA_ARGS__)

#define GOGGLES_LOG_INFO(...)                                                                      \
    SPDLOG_LOGGER_INFO(::goggles::get_logger(), GOGGLES_LOG_TAG_PREFIX __VA_ARGS__)

#define GOGGLES_LOG_WARN(...)                                                                      \
    SPDLOG_LOGGER_WARN(::goggles::get_logger(), GOGGLES_LOG_TAG_PREFIX __VA_ARGS__)

#define GOGGLES_LOG_ERROR(...)                                                                     \
    SPDLOG_LOGGER_ERROR(::goggles::get_logger(), GOGGLES_LOG_TAG_PREFIX __VA_ARGS__)

#define GOGGLES_LOG_CRITICAL(...)                                                                  \
    SPDLOG_LOGGER_CRITICAL(::goggles::get_logger(), GOGGLES_LOG_TAG_PREFIX __VA_ARGS__)
