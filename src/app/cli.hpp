#pragma once

#include "util/error.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <string>

namespace goggles::app {

struct CliOptions {
    std::filesystem::path config_path = "config/goggles.toml";
    std::string shader_preset;
};

using CliResult = Result<CliOptions>;

[[nodiscard]] inline auto parse_cli(int argc, char** argv) -> CliResult {
    CLI::App app{GOGGLES_PROJECT_NAME " - Low-latency game streaming and post-processing viewer"};
    app.set_version_flag("--version,-v", GOGGLES_PROJECT_NAME " v" GOGGLES_VERSION);

    CliOptions options;

    app.add_option("-c,--config", options.config_path, "Path to configuration file")
        ->check(CLI::ExistingFile);

    app.add_option("-s,--shader", options.shader_preset, "Override shader preset (path to .slangp)")
        ->check(CLI::ExistingFile);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        int exit_code = app.exit(e);
        if (exit_code == 0) {
            return make_error<CliOptions>(ErrorCode::ok, "User requested help or version info.");
        }
        return make_error<CliOptions>(ErrorCode::parse_error,
                                      "Failed to parse command line arguments.");
    }

    return options;
}

} // namespace goggles::app
