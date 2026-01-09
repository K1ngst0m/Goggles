#include "application.hpp"
#include "cli.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <optional>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/logging.hpp>
#include <vector>

static auto get_reaper_path() -> std::string {
    std::array<char, 4096> exe_path{};
    const ssize_t len = readlink("/proc/self/exe", exe_path.data(), exe_path.size() - 1);
    if (len <= 0) {
        return "goggles-reaper";
    }
    exe_path[static_cast<size_t>(len)] = '\0';
    const std::filesystem::path exe_dir = std::filesystem::path(exe_path.data()).parent_path();
    return (exe_dir / "goggles-reaper").string();
}

static auto spawn_target_app(const std::vector<std::string>& command,
                             const std::string& x11_display, const std::string& wayland_display,
                             uint32_t app_width, uint32_t app_height, const std::string& gpu_uuid)
    -> goggles::Result<pid_t> {
    if (command.empty()) {
        return goggles::make_error<pid_t>(goggles::ErrorCode::invalid_config,
                                          "missing target app command");
    }

    if (x11_display.empty() || wayland_display.empty()) {
        return goggles::make_error<pid_t>(goggles::ErrorCode::input_init_failed,
                                          "input forwarding display information unavailable");
    }

    auto is_override_key = [](std::string_view key) -> bool {
        return key == "GOGGLES_CAPTURE" || key == "GOGGLES_WSI_PROXY" || key == "DISPLAY" ||
               key == "WAYLAND_DISPLAY" || key == "GOGGLES_WIDTH" || key == "GOGGLES_HEIGHT" ||
               key == "GOGGLES_GPU_UUID";
    };

    std::vector<std::string> env_overrides;
    env_overrides.reserve(7);
    env_overrides.emplace_back("GOGGLES_CAPTURE=1");
    env_overrides.emplace_back("GOGGLES_WSI_PROXY=1");
    env_overrides.emplace_back("DISPLAY=" + x11_display);
    env_overrides.emplace_back("WAYLAND_DISPLAY=" + wayland_display);
    env_overrides.emplace_back("GOGGLES_GPU_UUID=" + gpu_uuid);

    if (app_width != 0 && app_height != 0) {
        env_overrides.emplace_back("GOGGLES_WIDTH=" + std::to_string(app_width));
        env_overrides.emplace_back("GOGGLES_HEIGHT=" + std::to_string(app_height));
    }

    std::vector<char*> envp;
    envp.reserve(env_overrides.size() + 64);
    for (auto& entry : env_overrides) {
        envp.push_back(entry.data());
    }
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        std::string_view kv{*entry};
        const auto eq = kv.find('=');
        if (eq != std::string_view::npos) {
            const auto key = kv.substr(0, eq);
            if (is_override_key(key)) {
                continue;
            }
        }
        envp.push_back(*entry);
    }
    envp.push_back(nullptr);

    // Build argv: goggles-reaper <target_command...>
    const std::string reaper_path = get_reaper_path();
    std::vector<char*> argv;
    argv.reserve(command.size() + 2);
    argv.push_back(const_cast<char*>(reaper_path.c_str()));
    for (const auto& arg : command) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int rc =
        posix_spawn(&pid, reaper_path.c_str(), nullptr, nullptr, argv.data(), envp.data());
    if (rc != 0) {
        return goggles::make_error<pid_t>(goggles::ErrorCode::unknown_error,
                                          std::string("posix_spawn() failed: ") +
                                              std::strerror(rc));
    }

    return pid;
}

static auto terminate_child(pid_t pid) -> void {
    if (pid <= 0) {
        return;
    }

    auto reap_with_timeout = [](pid_t child_pid, std::chrono::milliseconds timeout) -> bool {
        constexpr auto POLL_INTERVAL = std::chrono::milliseconds(50);
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        for (;;) {
            int status = 0;
            const pid_t result = waitpid(child_pid, &status, WNOHANG);
            if (result == child_pid) {
                return true;
            }
            if (result == 0) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    return false;
                }
                std::this_thread::sleep_for(POLL_INTERVAL);
                continue;
            }

            if (errno == EINTR) {
                continue;
            }
            // Child already reaped or not our child anymore.
            return true;
        }
    };

    constexpr auto SIGTERM_TIMEOUT = std::chrono::seconds(3);
    constexpr auto SIGKILL_TIMEOUT = std::chrono::seconds(2);

    (void)kill(pid, SIGTERM);
    if (reap_with_timeout(pid, SIGTERM_TIMEOUT)) {
        return;
    }

    GOGGLES_LOG_WARN("Target app did not exit after SIGTERM; sending SIGKILL (pid={})", pid);
    (void)kill(pid, SIGKILL);
    if (reap_with_timeout(pid, SIGKILL_TIMEOUT)) {
        return;
    }

    GOGGLES_LOG_ERROR("Target app did not exit after SIGKILL (pid={})", pid);
}

static auto push_quit_event() -> void {
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit);
}

static auto resolve_xdg_config_home() -> std::optional<std::filesystem::path> {
    if (const char* xdg_config = std::getenv("XDG_CONFIG_HOME"); xdg_config && *xdg_config) {
        return std::filesystem::path(xdg_config);
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".config";
    }
    return std::nullopt;
}

static auto resolve_default_config_path() -> std::optional<std::filesystem::path> {
    if (auto xdg = resolve_xdg_config_home()) {
        auto candidate = *xdg / "goggles" / "goggles.toml";
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
            return candidate;
        }
        if (ec) {
            GOGGLES_LOG_DEBUG("Failed to stat config candidate '{}': {}", candidate.string(),
                              ec.message());
        }
    }

    if (const char* resource_dir = std::getenv("GOGGLES_RESOURCE_DIR");
        resource_dir && *resource_dir) {
        auto candidate = std::filesystem::path(resource_dir) / "config" / "goggles.toml";
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
            return candidate;
        }
        if (ec) {
            GOGGLES_LOG_DEBUG("Failed to stat config candidate '{}': {}", candidate.string(),
                              ec.message());
        }
    }

    {
        auto candidate = std::filesystem::path("config") / "goggles.toml";
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
            return candidate;
        }
        if (ec) {
            GOGGLES_LOG_DEBUG("Failed to stat config candidate '{}': {}", candidate.string(),
                              ec.message());
        }
    }

    return std::nullopt;
}

static auto load_config_for_cli(const goggles::app::CliOptions& cli_opts)
    -> std::optional<goggles::Config> {
    std::optional<std::filesystem::path> config_path;
    const bool explicit_config = !cli_opts.config_path.empty();
    if (!cli_opts.config_path.empty()) {
        config_path = cli_opts.config_path;
    } else {
        config_path = resolve_default_config_path();
    }

    if (!config_path) {
        GOGGLES_LOG_WARN("No configuration file found; using defaults");
        return goggles::default_config();
    }

    GOGGLES_LOG_INFO("Loading configuration: {}", config_path->string());
    auto config_result = goggles::load_config(*config_path);
    if (!config_result) {
        const auto& error = config_result.error();
        GOGGLES_LOG_ERROR("Failed to load configuration from '{}': {} ({})", config_path->string(),
                          error.message, goggles::error_code_name(error.code));
        if (explicit_config) {
            GOGGLES_LOG_CRITICAL("Config was provided explicitly; refusing to continue");
            return std::nullopt;
        }
        GOGGLES_LOG_INFO("Using default configuration");
        return goggles::default_config();
    }
    return config_result.value();
}

static auto apply_log_level(const goggles::Config& config) -> void {
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
}

static auto log_config_summary(const goggles::Config& config) -> void {
    GOGGLES_LOG_DEBUG("Configuration loaded:");
    GOGGLES_LOG_DEBUG("  Capture backend: {}", config.capture.backend);
    GOGGLES_LOG_DEBUG("  Input forwarding: {}", config.input.forwarding);
    GOGGLES_LOG_DEBUG("  Render vsync: {}", config.render.vsync);
    GOGGLES_LOG_DEBUG("  Render target_fps: {}", config.render.target_fps);
    GOGGLES_LOG_DEBUG("  Render enable_validation: {}", config.render.enable_validation);
    GOGGLES_LOG_DEBUG("  Render scale_mode: {}", to_string(config.render.scale_mode));
    GOGGLES_LOG_DEBUG("  Render integer_scale: {}", config.render.integer_scale);
    GOGGLES_LOG_DEBUG("  Log level: {}", config.logging.level);
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

    auto config_result = load_config_for_cli(cli_opts);
    if (!config_result) {
        return EXIT_FAILURE;
    }
    goggles::Config config = config_result.value();

    if (!cli_opts.shader_preset.empty()) {
        config.shader.preset = cli_opts.shader_preset;
        GOGGLES_LOG_INFO("Shader preset overridden by CLI: {}", config.shader.preset);
    }
    if (!config.shader.preset.empty()) {
        std::filesystem::path preset_path{config.shader.preset};
        if (preset_path.is_relative()) {
            if (const char* resource_dir = std::getenv("GOGGLES_RESOURCE_DIR");
                resource_dir && *resource_dir) {
                preset_path = std::filesystem::path(resource_dir) / preset_path;
                config.shader.preset = preset_path.lexically_normal().string();
            }
        }
    }

    apply_log_level(config);
    log_config_summary(config);

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
                                     cli_opts.app_width, cli_opts.app_height, app->gpu_uuid());
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
