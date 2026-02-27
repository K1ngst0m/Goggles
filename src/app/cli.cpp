#include "app/cli.hpp"

#include <CLI/CLI.hpp>
#include <util/profiling.hpp>

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
    app.add_option("--gpu", options.gpu_selector,
                   "Select GPU by index (e.g. 0) or name substring (e.g. AMD)");
    app.add_flag("--detach", options.detach, "Viewer-only mode (do not launch target app)");
    app.add_option("--app-width", options.app_width,
                   "Source resolution width (also sets GOGGLES_WIDTH for launched app)")
        ->check(CLI::Range(1u, 16384u));
    app.add_option("--app-height", options.app_height,
                   "Source resolution height (also sets GOGGLES_HEIGHT for launched app)")
        ->check(CLI::Range(1u, 16384u));
    app.add_option("--target-fps", options.target_fps, "Override render target FPS (0 = uncapped)")
        ->check(CLI::Range(0u, 1000u));
    app.add_flag("--headless", options.headless, "Run without a window (headless mode)");
    app.add_option("--frames", options.frames,
                   "Number of compositor frames to capture (headless mode)")
        ->check(CLI::Range(1u, 100000u));
    app.add_option("--output", options.output_path, "Output PNG file path (headless mode)");
}

[[nodiscard]] auto validate_default_mode(int argc, bool has_separator, const CliOptions& options)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
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

    if (options.app_command.empty()) {
        return make_error<void>(ErrorCode::parse_error,
                                "missing target app command (use '--detach' for viewer-only mode, "
                                "or pass app after '--')");
    }
    return {};
}

} // namespace

auto parse_cli(int argc, char** argv) -> CliResult {
    GOGGLES_PROFILE_FUNCTION();
    CLI::App app{GOGGLES_PROJECT_NAME " - Low-latency game streaming and post-processing viewer"};
    app.set_version_flag("--version,-v", GOGGLES_PROJECT_NAME " v" GOGGLES_VERSION);
    app.footer(R"(Usage:
  goggles --detach
  goggles --headless --frames N --output <path.png> [options] -- <app> [app_args...]
  goggles [options] -- <app> [app_args...]

Notes:
  - Default mode (no --detach) launches the target app inside the compositor.
  - '--' is required before <app> to avoid app args (e.g. '--config') being parsed as Goggles options.)");

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

    if (options.detach) {
        if (!options.app_command.empty()) {
            return make_error<CliParseOutcome>(ErrorCode::parse_error,
                                               "detach mode does not accept an app command");
        }
        if (options.app_width != 0 || options.app_height != 0) {
            return make_error<CliParseOutcome>(
                ErrorCode::parse_error,
                "--app-width/--app-height are not supported in detach mode");
        }
    }

    if (options.headless) {
        if (options.detach) {
            return make_error<CliParseOutcome>(ErrorCode::parse_error,
                                               "--headless and --detach are mutually exclusive");
        }
        if (options.frames == 0) {
            return make_error<CliParseOutcome>(ErrorCode::parse_error,
                                               "--headless requires --frames");
        }
        if (options.output_path.empty()) {
            return make_error<CliParseOutcome>(ErrorCode::parse_error,
                                               "--headless requires --output");
        }
        // Headless mode also requires an app command (validated by default mode below).
    }

    if (!options.detach) {
        auto validation_result = validate_default_mode(argc, has_separator, options);
        if (!validation_result) {
            return make_error<CliParseOutcome>(validation_result.error().code,
                                               validation_result.error().message,
                                               validation_result.error().location);
        }
    }

    return CliParseOutcome{
        .action = CliAction::run,
        .options = std::move(options),
    };
}

} // namespace goggles::app
