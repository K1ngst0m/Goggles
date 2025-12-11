#include "capture/capture_receiver.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <filesystem>
#include <render/backend/vulkan_backend.hpp>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>

auto main() -> int {
    goggles::initialize_logger("goggles");
    GOGGLES_LOG_INFO("Goggles v0.1.0 starting");

    const auto config_path = std::filesystem::path("config/goggles.toml");
    auto config_result = goggles::load_config(config_path);

    goggles::Config config;
    if (!config_result) {
        const auto& error = config_result.error();
        GOGGLES_LOG_ERROR("Failed to load configuration: {} ({})", error.message,
                          goggles::error_code_name(error.code));
        GOGGLES_LOG_INFO("Using default configuration");
        config = goggles::default_config();
    } else {
        config = config_result.value();
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
    GOGGLES_LOG_INFO("Window created (1280x720, Vulkan-enabled)");

    goggles::render::VulkanBackend vulkan_backend;
    auto init_result = vulkan_backend.init(window, config.render.enable_validation);
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

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GOGGLES_LOG_INFO("Quit event received");
                running = false;
            }
        }

        capture_receiver.poll_frame();

        if (capture_receiver.has_frame()) {
            auto render_result = vulkan_backend.render_frame(capture_receiver.get_frame());
            if (!render_result) {
                GOGGLES_LOG_ERROR("Render failed: {}", render_result.error().message);
            }
        } else {
            auto render_result = vulkan_backend.render_clear();
            if (!render_result) {
                GOGGLES_LOG_ERROR("Clear failed: {}", render_result.error().message);
            }
        }
    }

    GOGGLES_LOG_INFO("Shutting down...");
    capture_receiver.shutdown();
    vulkan_backend.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    GOGGLES_LOG_INFO("Goggles terminated successfully");

    return EXIT_SUCCESS;
}
