// Goggles - Real-time game capture and GPU post-processing

#include "capture_receiver.hpp"

#include <cstdlib>
#include <filesystem>

#include <SDL3/SDL.h>

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

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        GOGGLES_LOG_CRITICAL("Failed to initialize SDL3: {}", SDL_GetError());
        return EXIT_FAILURE;
    }
    GOGGLES_LOG_INFO("SDL3 initialized");

    // Create window with Vulkan support
    SDL_Window* window = SDL_CreateWindow("Goggles", 1280, 720, SDL_WINDOW_VULKAN);
    if (window == nullptr) {
        GOGGLES_LOG_CRITICAL("Failed to create window: {}", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }
    GOGGLES_LOG_INFO("Window created (1280x720, Vulkan-enabled)");

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        GOGGLES_LOG_CRITICAL("Failed to create renderer: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    GOGGLES_LOG_INFO("Renderer created");

    // Initialize capture receiver
    goggles::CaptureReceiver capture_receiver;
    if (!capture_receiver.init()) {
        GOGGLES_LOG_WARN("Failed to initialize capture receiver");
    }

    SDL_Texture* capture_texture = nullptr;

    // Event loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GOGGLES_LOG_INFO("Quit event received");
                running = false;
            }
        }

        // Poll for captured frames
        static uint64_t last_data_hash = 0;
        static uint64_t frame_num = 0;
        
        if (capture_receiver.poll_frame()) {
            // Check if actual pixel data changed
            const auto& frame = capture_receiver.get_frame();
            if (frame.data != nullptr) {
                uint64_t hash = 0;
                auto* p = static_cast<const uint64_t*>(frame.data);
                for (int i = 0; i < 8; ++i) {
                    hash ^= p[i];  // Quick hash of first 64 bytes
                }
                
                if (hash != last_data_hash) {
                    ++frame_num;
                    if (frame_num % 30 == 1) {
                        GOGGLES_LOG_DEBUG("Frame {}: data changed, hash={:016x}", frame_num, hash);
                    }
                    last_data_hash = hash;
                }
            }
            capture_texture = capture_receiver.update_texture(renderer, capture_texture);
        }
        
        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (capture_texture != nullptr) {
            SDL_RenderTexture(renderer, capture_texture, nullptr, nullptr);
        }

        SDL_RenderPresent(renderer);
    }

    // Cleanup capture
    if (capture_texture != nullptr) {
        SDL_DestroyTexture(capture_texture);
    }
    capture_receiver.shutdown();

    // Cleanup
    GOGGLES_LOG_INFO("Shutting down...");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    GOGGLES_LOG_INFO("Goggles terminated successfully");

    return EXIT_SUCCESS;
}
