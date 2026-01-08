#include "application.hpp"
#include "cli.hpp"

#include <SDL3/SDL.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>
#include <vector>

static auto spawn_target_app(const std::vector<std::string>& command,
                             const std::string& x11_display, const std::string& wayland_display,
                             uint32_t app_width, uint32_t app_height) -> goggles::Result<pid_t> {
    if (command.empty()) {
        return goggles::make_error<pid_t>(goggles::ErrorCode::invalid_config,
                                          "missing target app command");
    }

    if (x11_display.empty() || wayland_display.empty()) {
        return goggles::make_error<pid_t>(goggles::ErrorCode::input_init_failed,
                                          "input forwarding display information unavailable");
    }

    pid_t pid = fork();
    if (pid < 0) {
        return goggles::make_error<pid_t>(goggles::ErrorCode::unknown_error, "fork() failed");
    }

    if (pid == 0) {
        setenv("GOGGLES_CAPTURE", "1", 1);
        setenv("GOGGLES_WSI_PROXY", "1", 1);
        setenv("DISPLAY", x11_display.c_str(), 1);
        setenv("WAYLAND_DISPLAY", wayland_display.c_str(), 1);

        if (app_width != 0 && app_height != 0) {
            setenv("GOGGLES_WIDTH", std::to_string(app_width).c_str(), 1);
            setenv("GOGGLES_HEIGHT", std::to_string(app_height).c_str(), 1);
        }

        std::vector<char*> argv;
        argv.reserve(command.size() + 1);
        for (const auto& arg : command) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    return pid;
}

static auto terminate_child(pid_t pid) -> void {
    if (pid <= 0) {
        return;
    }
    kill(pid, SIGTERM);
    int status = 0;
    waitpid(pid, &status, 0);
}

static auto push_quit_event() -> void {
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit);
}

static auto run_app(int argc, char** argv) -> int {
    auto cli_result = goggles::app::parse_cli(argc, argv);
    if (!cli_result) {
        if (cli_result.error().code == goggles::ErrorCode::ok) {
            return EXIT_SUCCESS;
        }

        std::fprintf(stderr, "Error: %s\n", cli_result.error().message.c_str());
        std::fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
        return EXIT_FAILURE;
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

    bool enable_input_forwarding = !cli_opts.detach;
    if (!cli_opts.detach && !config.input.forwarding) {
        GOGGLES_LOG_INFO("Default mode: input forwarding enabled");
    }
    if (cli_opts.detach && config.input.forwarding) {
        GOGGLES_LOG_INFO("Detach mode: input forwarding disabled");
    }

    auto app_result = goggles::app::Application::create(config, enable_input_forwarding);
    if (!app_result) {
        GOGGLES_LOG_CRITICAL("Failed to initialize app: {} ({})", app_result.error().message,
                             goggles::error_code_name(app_result.error().code));
        return EXIT_FAILURE;
    }

    {
        auto app = std::move(app_result.value());

        pid_t child_pid = -1;
        int child_status = 0;
        bool child_exited = false;

        if (!cli_opts.detach) {
            if (enable_input_forwarding) {
                const auto x11_display = app->x11_display();
                const auto wayland_display = app->wayland_display();

                auto spawn_result =
                    spawn_target_app(cli_opts.app_command, x11_display, wayland_display,
                                     cli_opts.app_width, cli_opts.app_height);
                if (!spawn_result) {
                    GOGGLES_LOG_CRITICAL("Failed to launch target app: {} ({})",
                                         spawn_result.error().message,
                                         goggles::error_code_name(spawn_result.error().code));
                    return EXIT_FAILURE;
                }
                child_pid = spawn_result.value();
                GOGGLES_LOG_INFO("Launched target app (pid={})", child_pid);
            }

            while (app->is_running()) {
                app->pump_events();
                app->tick_frame();

                if (child_pid > 0 && !child_exited) {
                    pid_t result = waitpid(child_pid, &child_status, WNOHANG);
                    if (result == child_pid) {
                        child_exited = true;
                        push_quit_event();
                    }
                }
            }

            if (child_pid > 0 && !child_exited) {
                GOGGLES_LOG_INFO("Viewer exited; terminating target app (pid={})", child_pid);
                terminate_child(child_pid);
                return EXIT_FAILURE;
            }

            if (child_pid > 0 && child_exited) {
                if (WIFEXITED(child_status)) {
                    return WEXITSTATUS(child_status);
                }
                return EXIT_FAILURE;
            }
        } else {
            app->run();
        }
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
