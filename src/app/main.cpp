#include "application.hpp"
#include "cli.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>

static auto run_app(int argc, char** argv) -> int {
    auto cli_result = goggles::app::parse_cli(argc, argv);
    if (!cli_result) {
        return (cli_result.error().code == goggles::ErrorCode::ok) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    const auto& cli_opts = cli_result.value();

    goggles::initialize_logger("goggles");
    GOGGLES_LOG_INFO(GOGGLES_PROJECT_NAME " v" GOGGLES_VERSION " starting");

    auto config_result = goggles::load_config(cli_opts.config_path);

    goggles::Config config;
    if (!config_result) {
        const auto& error = config_result.error();
        GOGGLES_LOG_ERROR("Failed to load configuration from '{}': {} ({})",
                          cli_opts.config_path.string(), error.message,
                          goggles::error_code_name(error.code));
        GOGGLES_LOG_INFO("Using default configuration");
        config = goggles::default_config();
    } else {
        config = config_result.value();
    }

    if (!cli_opts.shader_preset.empty()) {
        config.shader.preset = cli_opts.shader_preset;
        GOGGLES_LOG_INFO("Shader preset overridden by CLI: {}", config.shader.preset);
    }

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
    GOGGLES_LOG_DEBUG("  Input forwarding: {}", config.input.forwarding);
    GOGGLES_LOG_DEBUG("  Render vsync: {}", config.render.vsync);
    GOGGLES_LOG_DEBUG("  Render target_fps: {}", config.render.target_fps);
    GOGGLES_LOG_DEBUG("  Render enable_validation: {}", config.render.enable_validation);
    GOGGLES_LOG_DEBUG("  Render scale_mode: {}", to_string(config.render.scale_mode));
    GOGGLES_LOG_DEBUG("  Render integer_scale: {}", config.render.integer_scale);
    GOGGLES_LOG_DEBUG("  Log level: {}", config.logging.level);

    bool enable_input_forwarding = config.input.forwarding;
    if (cli_opts.enable_input_forwarding) {
        enable_input_forwarding = true;
    }

    auto app_result = goggles::app::Application::create(config, enable_input_forwarding);
    if (!app_result) {
        GOGGLES_LOG_CRITICAL("Failed to initialize app: {} ({})", app_result.error().message,
                             goggles::error_code_name(app_result.error().code));
        return EXIT_FAILURE;
    }

    {
        auto app = std::move(app_result.value());
        app->run();
        GOGGLES_LOG_INFO("Shutting down...");
    }
    GOGGLES_LOG_INFO("Goggles terminated successfully");
    return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int {
    try {
        return run_app(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[CRITICAL] Unhandled exception: %s\n", e.what());
        try {
            GOGGLES_LOG_CRITICAL("Unhandled exception caught in main: {}", e.what());
            spdlog::shutdown();
        } catch (...) {
            std::fprintf(stderr, "[CRITICAL] Logger failed to handle exception\n");
        }
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "[CRITICAL] Unknown exception caught in main\n");
        return EXIT_FAILURE;
    }
}
