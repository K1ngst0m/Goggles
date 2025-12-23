#include "capture/capture_receiver.hpp"
#include "cli.hpp"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <render/backend/vulkan_backend.hpp>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

static void run_main_loop(goggles::render::VulkanBackend& vulkan_backend,
                          goggles::CaptureReceiver& capture_receiver) {
    bool running = true;
    while (running) {
        GOGGLES_PROFILE_FRAME("Main");

        SDL_Event event;
        bool window_resized = false;
        {
            GOGGLES_PROFILE_SCOPE("EventProcessing");
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    GOGGLES_LOG_INFO("Quit event received");
                    running = false;
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    window_resized = true;
                }
            }
        }

        if (window_resized) {
            GOGGLES_PROFILE_SCOPE("HandleResize");
            auto resize_result = vulkan_backend.handle_resize();
            if (!resize_result) {
                GOGGLES_LOG_ERROR("Resize failed: {}", resize_result.error().message);
            }
        }

        {
            GOGGLES_PROFILE_SCOPE("CaptureReceive");
            capture_receiver.poll_frame();
        }

        bool needs_resize = false;
        if (capture_receiver.has_frame()) {
            GOGGLES_PROFILE_SCOPE("RenderFrame");
            auto render_result = vulkan_backend.render_frame(capture_receiver.get_frame());
            if (!render_result) {
                GOGGLES_LOG_ERROR("Render failed: {}", render_result.error().message);
            } else {
                needs_resize = !*render_result;
            }
        } else {
            GOGGLES_PROFILE_SCOPE("RenderClear");
            auto render_result = vulkan_backend.render_clear();
            if (!render_result) {
                GOGGLES_LOG_ERROR("Clear failed: {}", render_result.error().message);
            } else {
                needs_resize = !*render_result;
            }
        }

        if (needs_resize) {
            GOGGLES_PROFILE_SCOPE("HandleResize");
            auto resize_result = vulkan_backend.handle_resize();
            if (!resize_result) {
                GOGGLES_LOG_ERROR("Resize failed: {}", resize_result.error().message);
            }
        }
    }
}

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
    GOGGLES_LOG_DEBUG("  Render vsync: {}", config.render.vsync);
    GOGGLES_LOG_DEBUG("  Render target_fps: {}", config.render.target_fps);
    GOGGLES_LOG_DEBUG("  Render enable_validation: {}", config.render.enable_validation);
    GOGGLES_LOG_DEBUG("  Render scale_mode: {}", to_string(config.render.scale_mode));
    GOGGLES_LOG_DEBUG("  Render integer_scale: {}", config.render.integer_scale);
    GOGGLES_LOG_DEBUG("  Log level: {}", config.logging.level);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        GOGGLES_LOG_CRITICAL("Failed to initialize SDL3: {}", SDL_GetError());
        return EXIT_FAILURE;
    }
    GOGGLES_LOG_INFO("SDL3 initialized");

    SDL_Window* window =
        SDL_CreateWindow("Goggles", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        GOGGLES_LOG_CRITICAL("Failed to create window: {}", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    goggles::render::VulkanBackend vulkan_backend;
    auto init_result = vulkan_backend.init(window, config.render.enable_validation, "shaders",
                                           config.render.scale_mode, config.render.integer_scale);
    if (!init_result) {
        GOGGLES_LOG_CRITICAL("Failed to initialize Vulkan: {} ({})", init_result.error().message,
                             goggles::error_code_name(init_result.error().code));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    vulkan_backend.load_shader_preset(config.shader.preset);

    goggles::CaptureReceiver capture_receiver;
    if (!capture_receiver.init()) {
        GOGGLES_LOG_WARN("Failed to initialize capture receiver");
    }

    run_main_loop(vulkan_backend, capture_receiver);

    GOGGLES_LOG_INFO("Shutting down...");
    capture_receiver.shutdown();
    vulkan_backend.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
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
