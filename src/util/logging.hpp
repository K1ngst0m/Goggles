#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <filesystem>
#include <goggles/error.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace goggles {

void initialize_logger(std::string_view app_name = "goggles");
[[nodiscard]] auto get_logger() -> std::shared_ptr<spdlog::logger>;
void set_log_level(spdlog::level::level_enum level);
void set_log_timestamp_enabled(bool enabled);
/// Empty path disables file logging; replaces any previous file sink.
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
