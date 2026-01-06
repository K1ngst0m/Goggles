#include "capture/capture_receiver.hpp"
#include "cli.hpp"
#include "input/input_forwarder.hpp"

#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <render/backend/vulkan_backend.hpp>
#include <render/chain/filter_chain.hpp>
#include <ui/imgui_layer.hpp>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>
#include <util/unique_fd.hpp>
#include <utility>

static auto scan_presets(const std::filesystem::path& dir) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> presets;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) {
        return presets;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
         it != std::filesystem::recursive_directory_iterator() && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec) && !ec && it->path().extension() == ".slangp") {
            presets.push_back(it->path());
        }
    }
    std::ranges::sort(presets);
    return presets;
}

static void handle_ui_state(goggles::render::VulkanBackend& vulkan_backend,
                            goggles::ui::ImGuiLayer& imgui_layer) {
    static bool last_shader_enabled = false;
    auto& state = imgui_layer.state();

    if (state.shader_enabled != last_shader_enabled) {
        vulkan_backend.set_shader_enabled(state.shader_enabled);
        last_shader_enabled = state.shader_enabled;
    }

    if (!state.reload_requested) {
        return;
    }
    state.reload_requested = false;

    if (state.selected_preset_index < 0 ||
        std::cmp_greater_equal(state.selected_preset_index, state.preset_catalog.size())) {
        return;
    }

    const auto& preset = state.preset_catalog[static_cast<size_t>(state.selected_preset_index)];
    auto result = vulkan_backend.reload_shader_preset(preset);
    if (!result) {
        GOGGLES_LOG_ERROR("Failed to load preset '{}': {}", preset.string(),
                          result.error().message);
    } else {
        state.current_preset = preset;
        GOGGLES_LOG_INFO("Loaded preset: {}", preset.string());
    }
}

static void run_main_loop(goggles::render::VulkanBackend& vulkan_backend,
                          goggles::CaptureReceiver* capture_receiver,
                          goggles::input::InputForwarder* input_forwarder,
                          goggles::ui::ImGuiLayer* imgui_layer) {
    bool running = true;
    while (running) {
        GOGGLES_PROFILE_FRAME("Main");

        SDL_Event event;
        bool window_resized = false;
        {
            GOGGLES_PROFILE_SCOPE("EventProcessing");
            while (SDL_PollEvent(&event)) {
                if (imgui_layer) {
                    imgui_layer->process_event(event);
                }

                if (event.type == SDL_EVENT_QUIT) {
                    GOGGLES_LOG_INFO("Quit event received");
                    running = false;
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    window_resized = true;
                } else if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_F1 && imgui_layer) {
                        imgui_layer->toggle_visibility();
                    } else if (input_forwarder &&
                               !(imgui_layer && imgui_layer->wants_capture_keyboard())) {
                        auto forward_result = input_forwarder->forward_key(event.key);
                        if (!forward_result) {
                            GOGGLES_LOG_ERROR("Failed to forward key: {}",
                                              forward_result.error().message);
                        }
                    }
                } else if (event.type == SDL_EVENT_KEY_UP) {
                    if (input_forwarder &&
                        !(imgui_layer && imgui_layer->wants_capture_keyboard())) {
                        auto forward_result = input_forwarder->forward_key(event.key);
                        if (!forward_result) {
                            GOGGLES_LOG_ERROR("Failed to forward key: {}",
                                              forward_result.error().message);
                        }
                    }
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                           event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    if (input_forwarder && !(imgui_layer && imgui_layer->wants_capture_mouse())) {
                        auto forward_result = input_forwarder->forward_mouse_button(event.button);
                        if (!forward_result) {
                            GOGGLES_LOG_ERROR("Failed to forward mouse button: {}",
                                              forward_result.error().message);
                        }
                    }
                } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    if (input_forwarder && !(imgui_layer && imgui_layer->wants_capture_mouse())) {
                        auto forward_result = input_forwarder->forward_mouse_motion(event.motion);
                        if (!forward_result) {
                            GOGGLES_LOG_ERROR("Failed to forward mouse motion: {}",
                                              forward_result.error().message);
                        }
                    }
                } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                    if (input_forwarder && !(imgui_layer && imgui_layer->wants_capture_mouse())) {
                        auto forward_result = input_forwarder->forward_mouse_wheel(event.wheel);
                        if (!forward_result) {
                            GOGGLES_LOG_ERROR("Failed to forward mouse wheel: {}",
                                              forward_result.error().message);
                        }
                    }
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

        if (capture_receiver) {
            GOGGLES_PROFILE_SCOPE("CaptureReceive");
            capture_receiver->poll_frame();
        }

        if (capture_receiver && capture_receiver->semaphores_updated()) {
            vulkan_backend.cleanup_sync_semaphores();
            auto ready_fd =
                goggles::util::UniqueFd::dup_from(capture_receiver->get_frame_ready_fd());
            auto consumed_fd =
                goggles::util::UniqueFd::dup_from(capture_receiver->get_frame_consumed_fd());
            if (!ready_fd || !consumed_fd) {
                GOGGLES_LOG_ERROR("Failed to dup semaphore fds");
                capture_receiver->clear_sync_semaphores();
            } else {
                auto import_result = vulkan_backend.import_sync_semaphores(std::move(ready_fd),
                                                                           std::move(consumed_fd));
                if (!import_result) {
                    GOGGLES_LOG_ERROR("Failed to import sync semaphores: {}",
                                      import_result.error().message);
                    capture_receiver->clear_sync_semaphores();
                } else {
                    GOGGLES_LOG_INFO("Sync semaphores imported successfully");
                }
            }
            capture_receiver->clear_semaphores_updated();
        }

        if (imgui_layer) {
            handle_ui_state(vulkan_backend, *imgui_layer);
            imgui_layer->begin_frame();
        }

        bool needs_resize = false;
        auto ui_callback = [&](vk::CommandBuffer cmd, vk::ImageView view, vk::Extent2D extent) {
            if (imgui_layer) {
                imgui_layer->end_frame();
                imgui_layer->record(cmd, view, extent);
            }
        };

        if (capture_receiver && capture_receiver->has_frame()) {
            GOGGLES_PROFILE_SCOPE("RenderFrame");
            auto render_result =
                imgui_layer ? vulkan_backend.render_frame_with_ui(capture_receiver->get_frame(),
                                                                  ui_callback)
                            : vulkan_backend.render_frame(capture_receiver->get_frame());
            if (!render_result) {
                GOGGLES_LOG_ERROR("Render failed: {}", render_result.error().message);
            } else {
                needs_resize = !*render_result;
            }
        } else {
            GOGGLES_PROFILE_SCOPE("RenderClear");
            auto render_result = imgui_layer ? vulkan_backend.render_clear_with_ui(ui_callback)
                                             : vulkan_backend.render_clear();
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
    GOGGLES_LOG_DEBUG("  Input forwarding: {}", config.input.forwarding);
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

    auto backend_result = goggles::render::VulkanBackend::create(
        window, config.render.enable_validation, "shaders", config.render.scale_mode,
        config.render.integer_scale);
    if (!backend_result) {
        GOGGLES_LOG_CRITICAL("Failed to initialize Vulkan: {} ({})", backend_result.error().message,
                             goggles::error_code_name(backend_result.error().code));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    auto vulkan_backend = std::move(backend_result.value());

    vulkan_backend->load_shader_preset(config.shader.preset);

    goggles::ui::ImGuiConfig imgui_config{
        .instance = vulkan_backend->instance(),
        .physical_device = vulkan_backend->physical_device(),
        .device = vulkan_backend->device(),
        .queue_family = vulkan_backend->graphics_queue_family(),
        .queue = vulkan_backend->graphics_queue(),
        .swapchain_format = vulkan_backend->swapchain_format(),
        .image_count = vulkan_backend->swapchain_image_count(),
    };

    std::unique_ptr<goggles::ui::ImGuiLayer> imgui_layer;
    auto imgui_result = goggles::ui::ImGuiLayer::create(window, imgui_config);
    if (!imgui_result) {
        GOGGLES_LOG_WARN("ImGui disabled: {}", imgui_result.error().message);
    } else {
        imgui_layer = std::move(imgui_result.value());
        auto presets = scan_presets("shaders/retroarch");
        imgui_layer->set_preset_catalog(std::move(presets));
        imgui_layer->set_current_preset(vulkan_backend->current_preset_path());
        imgui_layer->state().shader_enabled = !config.shader.preset.empty();
        GOGGLES_LOG_INFO("ImGui layer initialized (F1 to toggle)");
    }

    auto receiver_result = goggles::CaptureReceiver::create();
    std::unique_ptr<goggles::CaptureReceiver> capture_receiver;
    if (!receiver_result) {
        GOGGLES_LOG_WARN("Capture disabled - running in viewer-only mode ({})",
                         receiver_result.error().message);
    } else {
        capture_receiver = std::move(receiver_result.value());
    }

    bool enable_input_forwarding = config.input.forwarding;
    if (cli_opts.enable_input_forwarding) {
        enable_input_forwarding = true;
    }

    std::unique_ptr<goggles::input::InputForwarder> input_forwarder;

    if (enable_input_forwarding) {
        GOGGLES_LOG_INFO("Initializing input forwarding...");
        auto input_forwarder_result = goggles::input::InputForwarder::create();

        if (!input_forwarder_result) {
            GOGGLES_LOG_WARN("Input forwarding disabled: {}",
                             input_forwarder_result.error().message);
        } else {
            input_forwarder = std::move(input_forwarder_result.value());
            int display_num = input_forwarder->display_number();
            GOGGLES_LOG_INFO("Input forwarding initialized on DISPLAY :{}", display_num);
        }
    } else {
        GOGGLES_LOG_INFO("Input forwarding disabled");
    }

    run_main_loop(*vulkan_backend, capture_receiver.get(), input_forwarder.get(),
                  imgui_layer.get());

    GOGGLES_LOG_INFO("Shutting down...");
    imgui_layer.reset();
    if (capture_receiver) {
        capture_receiver->shutdown();
    }
    vulkan_backend->shutdown();
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
