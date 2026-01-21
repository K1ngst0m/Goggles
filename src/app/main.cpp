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
#include <util/paths.hpp>
#include <vector>

static auto get_exe_dir() -> std::filesystem::path {
    std::array<char, 4096> exe_path{};
    const ssize_t len = readlink("/proc/self/exe", exe_path.data(), exe_path.size() - 1);
    if (len <= 0) {
        return {};
    }
    exe_path[static_cast<size_t>(len)] = '\0';
    return std::filesystem::path(exe_path.data()).parent_path();
}

static auto get_reaper_path() -> std::string {
    const std::filesystem::path exe_dir = get_exe_dir();
    if (exe_dir.empty()) {
        return "goggles-reaper";
    }
    return (exe_dir / "goggles-reaper").string();
}

static auto spawn_target_app(const std::vector<std::string>& command,
                             const std::string& x11_display, const std::string& wayland_display,
                             uint32_t app_width, uint32_t app_height, const std::string& gpu_uuid,
                             const std::string& dump_dir, const std::string& dump_frame_range,
                             const std::string& dump_frame_mode, bool layer_log,
                             const std::string& layer_log_level, bool wsi_proxy)
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
               key == "GOGGLES_GPU_UUID" || key == "GOGGLES_DUMP_DIR" ||
               key == "GOGGLES_DUMP_FRAME_RANGE" || key == "GOGGLES_DUMP_FRAME_MODE" ||
               key == "GOGGLES_DEBUG_LOG" || key == "GOGGLES_DEBUG_LOG_LEVEL";
    };

    std::vector<std::string> env_overrides;
    env_overrides.reserve(12);
    env_overrides.emplace_back("GOGGLES_CAPTURE=1");
    env_overrides.emplace_back(wsi_proxy ? "GOGGLES_WSI_PROXY=1" : "GOGGLES_WSI_PROXY=0");
    env_overrides.emplace_back("DISPLAY=" + x11_display);
    env_overrides.emplace_back("WAYLAND_DISPLAY=" + wayland_display);
    env_overrides.emplace_back("GOGGLES_GPU_UUID=" + gpu_uuid);

    if (!dump_dir.empty()) {
        env_overrides.emplace_back("GOGGLES_DUMP_DIR=" + dump_dir);
    }
    if (!dump_frame_range.empty()) {
        env_overrides.emplace_back("GOGGLES_DUMP_FRAME_RANGE=" + dump_frame_range);
    }
    if (!dump_frame_mode.empty()) {
        env_overrides.emplace_back("GOGGLES_DUMP_FRAME_MODE=" + dump_frame_mode);
    }

    if (layer_log) {
        env_overrides.emplace_back("GOGGLES_DEBUG_LOG=1");
    }
    if (!layer_log_level.empty()) {
        env_overrides.emplace_back("GOGGLES_DEBUG_LOG_LEVEL=" + layer_log_level);
    }

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

struct FileCopyPaths {
    std::filesystem::path src;
    std::filesystem::path dst;
};

static auto copy_file_atomic(const FileCopyPaths& paths) -> goggles::Result<std::filesystem::path> {
    std::error_code ec;
    const auto dst_dir = paths.dst.parent_path();
    std::filesystem::create_directories(dst_dir, ec);
    if (ec) {
        GOGGLES_LOG_DEBUG("Failed to create config directory '{}': {}", dst_dir.string(),
                          ec.message());
        return goggles::make_error<std::filesystem::path>(
            goggles::ErrorCode::file_write_failed,
            "Failed to create config directory '" + dst_dir.string() + "': " + ec.message());
    }

    auto tmp = paths.dst;
    tmp += ".tmp";

    std::filesystem::copy_file(paths.src, tmp, std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return goggles::make_error<std::filesystem::path>(goggles::ErrorCode::file_write_failed,
                                                          "Failed to write config file '" +
                                                              tmp.string() + "': " + ec.message());
    }

    std::filesystem::rename(tmp, paths.dst, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return goggles::make_error<std::filesystem::path>(
            goggles::ErrorCode::file_write_failed, "Failed to rename config file '" + tmp.string() +
                                                       "' -> '" + paths.dst.string() +
                                                       "': " + ec.message());
    }

    return paths.dst;
}

static auto load_config_for_cli(const goggles::app::CliOptions& cli_opts,
                                const goggles::util::AppDirs& bootstrap_dirs) -> goggles::Config {
    const auto default_config_path = goggles::util::config_path(bootstrap_dirs, "goggles.toml");
    const bool explicit_config = !cli_opts.config_path.empty();

    std::filesystem::path config_path =
        explicit_config ? cli_opts.config_path : default_config_path;

    std::error_code ec;
    const bool exists = std::filesystem::is_regular_file(config_path, ec) && !ec;
    if (!explicit_config && !exists) {
        const auto template_path =
            goggles::util::resource_path(bootstrap_dirs, "config/goggles.template.toml");
        if (std::filesystem::is_regular_file(template_path, ec) && !ec) {
            auto copy_result =
                copy_file_atomic(FileCopyPaths{.src = template_path, .dst = config_path});
            if (copy_result) {
                GOGGLES_LOG_INFO("Wrote default configuration: {}", config_path.string());
            } else {
                GOGGLES_LOG_WARN("Failed to write default configuration: {} ({})",
                                 copy_result.error().message,
                                 goggles::error_code_name(copy_result.error().code));
            }
        }
    }

    if (std::filesystem::is_regular_file(config_path, ec) && !ec) {
        GOGGLES_LOG_INFO("Loading configuration: {}", config_path.string());
        auto config_result = goggles::load_config(config_path);
        if (config_result) {
            return config_result.value();
        }

        const auto& error = config_result.error();
        GOGGLES_LOG_WARN("Failed to load configuration from '{}': {} ({})", config_path.string(),
                         error.message, goggles::error_code_name(error.code));
        if (explicit_config) {
            GOGGLES_LOG_WARN("Explicit config ignored; falling back to defaults");
        }
        return goggles::default_config();
    }

    if (ec) {
        GOGGLES_LOG_WARN("Failed to stat configuration file '{}': {}", config_path.string(),
                         ec.message());
    } else if (explicit_config) {
        GOGGLES_LOG_WARN("Configuration file not found: {}; falling back to defaults",
                         config_path.string());
    } else {
        GOGGLES_LOG_INFO("No configuration file found; using defaults");
    }

    return goggles::default_config();
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
        std::fprintf(stderr, "Error: %s\n", cli_result.error().message.c_str());
        std::fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
        return EXIT_FAILURE;
    }
    const auto& cli_outcome = cli_result.value();
    if (cli_outcome.action == goggles::app::CliAction::exit_ok) {
        return EXIT_SUCCESS;
    }
    const auto& cli_opts = cli_outcome.options;

    goggles::initialize_logger("goggles");
    GOGGLES_LOG_INFO(GOGGLES_PROJECT_NAME " v" GOGGLES_VERSION " starting");

    const goggles::util::ResolveContext resolve_ctx{
        .exe_dir = get_exe_dir(),
        .cwd = []() -> std::filesystem::path {
            try {
                return std::filesystem::current_path();
            } catch (...) {
                return {};
            }
        }(),
    };

    auto bootstrap_dirs_result = goggles::util::resolve_app_dirs(resolve_ctx, {});
    if (!bootstrap_dirs_result) {
        GOGGLES_LOG_WARN("Failed to resolve app directories: {} ({})",
                         bootstrap_dirs_result.error().message,
                         goggles::error_code_name(bootstrap_dirs_result.error().code));
        return EXIT_FAILURE;
    }
    const auto& bootstrap_dirs = bootstrap_dirs_result.value();

    goggles::Config config = load_config_for_cli(cli_opts, bootstrap_dirs);
    auto final_overrides = goggles::util::overrides_from_config(config);
    auto final_dirs_result = goggles::util::resolve_app_dirs(resolve_ctx, final_overrides);
    goggles::util::AppDirs app_dirs = bootstrap_dirs;
    if (!final_dirs_result) {
        GOGGLES_LOG_WARN("Failed to resolve app directories from config overrides: {} ({})",
                         final_dirs_result.error().message,
                         goggles::error_code_name(final_dirs_result.error().code));
        GOGGLES_LOG_WARN("Using bootstrap directories");
    } else {
        app_dirs = final_dirs_result.value();
    }

    if (!cli_opts.shader_preset.empty()) {
        config.shader.preset = cli_opts.shader_preset;
        GOGGLES_LOG_INFO("Shader preset overridden by CLI: {}", config.shader.preset);
    }
    if (cli_opts.target_fps.has_value()) {
        config.render.target_fps = *cli_opts.target_fps;
        GOGGLES_LOG_INFO("Target FPS overridden by CLI: {}", config.render.target_fps);
    }
    if (cli_opts.app_width != 0 || cli_opts.app_height != 0) {
        config.render.source_width = cli_opts.app_width;
        config.render.source_height = cli_opts.app_height;
        GOGGLES_LOG_INFO("Source resolution: {}x{}", config.render.source_width,
                         config.render.source_height);
    }
    if (!config.shader.preset.empty()) {
        std::filesystem::path preset_path{config.shader.preset};
        if (preset_path.is_relative()) {
            preset_path = goggles::util::resource_path(app_dirs, preset_path);
            config.shader.preset = preset_path.string();
        }
    }

    apply_log_level(config);
    goggles::set_log_timestamp_enabled(config.logging.timestamp);
    log_config_summary(config);

    bool enable_input_forwarding = !cli_opts.detach;
    if (!cli_opts.detach && !config.input.forwarding) {
        GOGGLES_LOG_INFO("Default mode: input forwarding enabled");
    }
    if (cli_opts.detach && config.input.forwarding) {
        GOGGLES_LOG_INFO("Detach mode: input forwarding disabled");
    }

    auto app_result = goggles::app::Application::create(config, app_dirs, enable_input_forwarding);
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

                auto spawn_result = spawn_target_app(
                    cli_opts.app_command, x11_display, wayland_display, cli_opts.app_width,
                    cli_opts.app_height, app->gpu_uuid(), cli_opts.dump_dir,
                    cli_opts.dump_frame_range, cli_opts.dump_frame_mode, cli_opts.layer_log,
                    cli_opts.layer_log_level, cli_opts.wsi_proxy);
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
