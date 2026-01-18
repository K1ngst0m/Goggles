#include "app/cli.hpp"

#include <CLI/CLI.hpp>

namespace goggles::app {
namespace {

[[nodiscard]] auto make_exit_ok() -> CliResult {
    return CliParseOutcome{
        .action = CliAction::exit_ok,
        .options = {},
    };
}

[[nodiscard]] auto is_separator_arg(const char* arg) -> bool {
    return arg != nullptr && arg[0] == '-' && arg[1] == '-' && arg[2] == '\0';
}

[[nodiscard]] auto find_separator_index(int argc, char** argv) -> int {
    for (int i = 0; i < argc; ++i) {
        if (is_separator_arg(argv[i])) {
            return i;
        }
    }
    return -1;
}

auto register_options(CLI::App& app, CliOptions& options) -> void {
    app.add_option("-c,--config", options.config_path, "Path to configuration file");
    app.add_option("-s,--shader", options.shader_preset, "Override shader preset (path to .slangp)")
        ->check(CLI::ExistingFile);
    app.add_flag(
        "--input-forwarding", options.enable_input_forwarding,
        "Deprecated: default mode enables input forwarding; use --detach for viewer-only mode");
    app.add_flag("--detach", options.detach,
                 "Viewer-only mode (do not launch target app; disables input forwarding)");
    app.add_flag("--wsi-proxy", options.wsi_proxy,
                 "Default mode only: enable WSI proxy mode (sets GOGGLES_WSI_PROXY=1 for launched "
                 "app; virtualizes window and swapchain)");
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
    app.add_flag("--layer-log", options.layer_log,
                 "Default mode only: enable vk-layer logging (sets GOGGLES_DEBUG_LOG=1)");
    app.add_option("--layer-log-level", options.layer_log_level,
                   "Default mode only: vk-layer log level (sets GOGGLES_DEBUG_LOG_LEVEL; implies "
                   "--layer-log)");
    app.add_option("--target-fps", options.target_fps, "Override render target FPS (0 = uncapped)")
        ->check(CLI::Range(0u, 1000u));
}

[[nodiscard]] auto validate_detach_mode(const CliOptions& options) -> Result<void> {
    if (options.enable_input_forwarding) {
        return make_error<void>(ErrorCode::parse_error,
                                "--input-forwarding is not supported with --detach");
    }
    if (options.wsi_proxy) {
        return make_error<void>(ErrorCode::parse_error,
                                "--wsi-proxy is not supported with --detach");
    }
    if (options.app_width != 0 || options.app_height != 0) {
        return make_error<void>(ErrorCode::parse_error,
                                "--app-width/--app-height are not supported with --detach");
    }
    if (!options.dump_dir.empty() || !options.dump_frame_range.empty() ||
        !options.dump_frame_mode.empty()) {
        return make_error<void>(ErrorCode::parse_error,
                                "--dump-* options are not supported with --detach");
    }
    if (options.layer_log || !options.layer_log_level.empty()) {
        return make_error<void>(ErrorCode::parse_error,
                                "--layer-log options are not supported with --detach");
    }
    if (!options.app_command.empty()) {
        return make_error<void>(ErrorCode::parse_error,
                                "detach mode does not accept an app command");
    }
    return {};
}

[[nodiscard]] auto validate_default_mode(int argc, bool has_separator, const CliOptions& options)
    -> Result<void> {
    if (!has_separator) {
        if (argc <= 1) {
            return make_error<void>(ErrorCode::parse_error,
                                    "missing target app command (use '--detach' for viewer-only "
                                    "mode, or pass app after '--')");
        }
        return make_error<void>(ErrorCode::parse_error,
                                "missing '--' separator before target app command (use '--detach' "
                                "for viewer-only mode)");
    }

    if (options.app_width != 0 || options.app_height != 0) {
        if (options.app_width == 0 || options.app_height == 0) {
            return make_error<void>(ErrorCode::parse_error,
                                    "--app-width and --app-height must be provided together");
        }
    }
    if (options.app_command.empty()) {
        return make_error<void>(ErrorCode::parse_error,
                                "missing target app command (use '--detach' for viewer-only mode, "
                                "or pass app after '--')");
    }
    return {};
}

auto normalize(CliOptions& options) -> void {
    if (!options.layer_log_level.empty()) {
        options.layer_log = true;
    }
}

} // namespace

auto parse_cli(int argc, char** argv) -> CliResult {
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
    register_options(app, options);

    const int separator_index = find_separator_index(argc, argv);
    const bool has_separator = separator_index >= 0;
    const int viewer_argc = has_separator ? separator_index : argc;

    try {
        app.parse(viewer_argc, argv);
    } catch (const CLI::ParseError& e) {
        if (e.get_exit_code() == 0) {
            (void)app.exit(e);
            return make_exit_ok();
        }

        if (!has_separator) {
            return make_error<CliParseOutcome>(
                ErrorCode::parse_error,
                (argc <= 1)
                    ? "missing target app command (use '--detach' for viewer-only mode, or pass "
                      "app "
                      "after '--')"
                    : "missing '--' separator before target app command (use '--detach' for "
                      "viewer-only mode)");
        }

        (void)app.exit(e);
        return make_error<CliParseOutcome>(ErrorCode::parse_error,
                                           "Failed to parse command line arguments.");
    }

    if (has_separator) {
        for (int i = separator_index + 1; i < argc; ++i) {
            options.app_command.emplace_back(argv[i]);
        }
    }

    Result<void> validation_result{};
    if (options.detach) {
        validation_result = validate_detach_mode(options);
    } else {
        validation_result = validate_default_mode(argc, has_separator, options);
    }
    if (!validation_result) {
        return make_error<CliParseOutcome>(validation_result.error().code,
                                           validation_result.error().message,
                                           validation_result.error().location);
    }

    normalize(options);
    return CliParseOutcome{
        .action = CliAction::run,
        .options = std::move(options),
    };
}

} // namespace goggles::app
