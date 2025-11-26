// Goggles - Real-time game capture and GPU post-processing

#include <cstdlib>
#include <filesystem>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>

auto main() -> int {
    // Initialize logging system
    goggles::initialize_logger("goggles");

    GOGGLES_LOG_INFO("Goggles v0.1.0 starting");

    // Load configuration
    const auto config_path = std::filesystem::path("config/goggles.toml");
    auto config_result = goggles::load_config(config_path);

    goggles::Config config;
    if (!config_result) {
        const auto& error = config_result.error();
        GOGGLES_LOG_ERROR("Failed to load configuration: {} ({})", error.message,
                          goggles::error_code_name(error.code));
        GOGGLES_LOG_INFO("Using default configuration");
        // Fall back to defaults
        config = goggles::default_config();
    } else {
        config = config_result.value();
    }

    // Apply log level from config
    if (config.logging.level == "trace") {
        goggles::set_log_level(spdlog::level::trace);
    } else if (config.logging.level == "debug") {
        goggles::set_log_level(spdlog::level::debug);
    } else if (config.logging.level == "info") {
        goggles::set_log_level(spdlog::level::info);
    } else if (config.logging.level == "warn") {
        goggles::set_log_level(spdlog::level::warn);
    } else if (config.logging.level == "error") {
        goggles::set_log_level(spdlog::level::err);
    } else if (config.logging.level == "critical") {
        goggles::set_log_level(spdlog::level::critical);
    }

    GOGGLES_LOG_DEBUG("Configuration loaded:");
    GOGGLES_LOG_DEBUG("  Capture backend: {}", config.capture.backend);
    GOGGLES_LOG_DEBUG("  Render vsync: {}", config.render.vsync);
    GOGGLES_LOG_DEBUG("  Render target_fps: {}", config.render.target_fps);
    GOGGLES_LOG_DEBUG("  Log level: {}", config.logging.level);

    // TODO: Initialize subsystems based on config
    // - Capture backend
    // - Pipeline
    // - Render backend

    GOGGLES_LOG_INFO("Goggles initialized successfully");
    GOGGLES_LOG_INFO("No functional implementation yet (bootstrap phase)");

    return EXIT_SUCCESS;
}
