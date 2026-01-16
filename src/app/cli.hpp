#pragma once

#include "util/error.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace goggles::app {

struct CliOptions {
    std::filesystem::path config_path;
    std::string shader_preset;
    bool detach = false;
    uint32_t app_width = 0;
    uint32_t app_height = 0;
    std::string dump_dir;
    std::string dump_frame_range;
    std::string dump_frame_mode;
    bool enable_input_forwarding = false;
    std::optional<uint32_t> target_fps;
    std::vector<std::string> app_command;
};

using CliResult = Result<CliOptions>;

[[nodiscard]] inline auto parse_cli(int argc, char** argv) -> CliResult {
    CLI::App app{GOGGLES_PROJECT_NAME " - Low-latency game streaming and post-processing viewer"};
    app.set_version_flag("--version,-v", GOGGLES_PROJECT_NAME " v" GOGGLES_VERSION);
    app.footer(R"(Usage:
  goggles --detach
  goggles [options] -- <app> [app_args...]

Notes:
  - Default mode (no --detach) launches the target app with capture + input forwarding enabled.
  - '--' is required before <app> to avoid app args (e.g. '--config') being parsed as Goggles options.
  - --app-width/--app-height apply only in default mode.)");

    CliOptions options;

    app.add_option("-c,--config", options.config_path, "Path to configuration file");

    app.add_option("-s,--shader", options.shader_preset, "Override shader preset (path to .slangp)")
        ->check(CLI::ExistingFile);

    app.add_flag(
        "--input-forwarding", options.enable_input_forwarding,
        "Deprecated: default mode enables input forwarding; use --detach for viewer-only mode");

    app.add_flag("--detach", options.detach,
                 "Viewer-only mode (do not launch target app; disables input forwarding)");

    app.add_option("--app-width", options.app_width,
                   "Default mode only: virtual surface width (sets GOGGLES_WIDTH for launched app)")
        ->check(CLI::Range(1u, 16384u));

    app.add_option(
           "--app-height", options.app_height,
           "Default mode only: virtual surface height (sets GOGGLES_HEIGHT for launched app)")
        ->check(CLI::Range(1u, 16384u));

    app.add_option(
        "--dump-dir", options.dump_dir,
        "Default mode only: dump directory for target app (sets GOGGLES_DUMP_DIR; default is "
        "/tmp/goggles_dump in layer)");

    app.add_option("--dump-frame-range", options.dump_frame_range,
                   "Default mode only: dump frames (sets GOGGLES_DUMP_FRAME_RANGE, e.g. 3,5,8-13)");

    app.add_option("--dump-frame-mode", options.dump_frame_mode,
                   "Default mode only: dump mode (sets GOGGLES_DUMP_FRAME_MODE; ppm only for now)");

    app.add_option("--target-fps", options.target_fps, "Override render target FPS (0 = uncapped)")
        ->check(CLI::Range(0u, 1000u));

    int separator_index = -1;
    for (int i = 0; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--") {
            separator_index = i;
            break;
        }
    }

    int viewer_argc = (separator_index >= 0) ? separator_index : argc;

    try {
        app.parse(viewer_argc, argv);
    } catch (const CLI::ParseError& e) {
        bool requested_help_or_version = false;
        for (int i = 1; i < viewer_argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--help" || arg == "-h" || arg == "--version" || arg == "-v") {
                requested_help_or_version = true;
                break;
            }
        }

        if (separator_index < 0 && !requested_help_or_version) {
            if (argc <= 1) {
                return make_error<CliOptions>(ErrorCode::parse_error,
                                              "missing target app command (use '--detach' for "
                                              "viewer-only mode, or pass app after '--')");
            }
            return make_error<CliOptions>(ErrorCode::parse_error,
                                          "missing '--' separator before target app command (use "
                                          "'--detach' for viewer-only mode)");
        }

        int exit_code = app.exit(e);
        if (exit_code == 0) {
            return make_error<CliOptions>(ErrorCode::ok, "User requested help or version info.");
        }
        return make_error<CliOptions>(ErrorCode::parse_error,
                                      "Failed to parse command line arguments.");
    }

    if (separator_index >= 0) {
        for (int i = separator_index + 1; i < argc; ++i) {
            options.app_command.emplace_back(argv[i]);
        }
    }

    if (options.detach) {
        if (options.enable_input_forwarding) {
            return make_error<CliOptions>(ErrorCode::parse_error,
                                          "--input-forwarding is not supported with --detach");
        }
        if (options.app_width != 0 || options.app_height != 0) {
            return make_error<CliOptions>(
                ErrorCode::parse_error, "--app-width/--app-height are not supported with --detach");
        }
        if (!options.dump_dir.empty() || !options.dump_frame_range.empty() ||
            !options.dump_frame_mode.empty()) {
            return make_error<CliOptions>(ErrorCode::parse_error,
                                          "--dump-* options are not supported with --detach");
        }
        if (!options.app_command.empty()) {
            return make_error<CliOptions>(ErrorCode::parse_error,
                                          "detach mode does not accept an app command");
        }
        return options;
    }

    if (separator_index < 0) {
        if (argc <= 1) {
            return make_error<CliOptions>(ErrorCode::parse_error,
                                          "missing target app command (use '--detach' for "
                                          "viewer-only mode, or pass app after '--')");
        }
        return make_error<CliOptions>(ErrorCode::parse_error,
                                      "missing '--' separator before target app command (use "
                                      "'--detach' for viewer-only mode)");
    }

    if (options.app_width != 0 || options.app_height != 0) {
        if (options.app_width == 0 || options.app_height == 0) {
            return make_error<CliOptions>(ErrorCode::parse_error,
                                          "--app-width and --app-height must be provided together");
        }
    }

    if (options.app_command.empty()) {
        return make_error<CliOptions>(ErrorCode::parse_error,
                                      "missing target app command (use '--detach' for viewer-only "
                                      "mode, or pass app after '--')");
    }

    return options;
}

} // namespace goggles::app
