#pragma once

#include "util/error.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace goggles::app {

struct CliOptions {
    std::filesystem::path config_path;
    std::string shader_preset;
    std::string gpu_selector;
    bool detach = false;
    uint32_t app_width = 0;
    uint32_t app_height = 0;
    std::optional<uint32_t> target_fps;
    bool headless = false;
    uint32_t frames = 0;
    std::filesystem::path output_path;
    std::vector<std::string> app_command;
};

enum class CliAction : std::uint8_t {
    run,
    exit_ok,
};

struct CliParseOutcome {
    CliAction action = CliAction::run;
    CliOptions options;
};

using CliResult = Result<CliParseOutcome>;

[[nodiscard]] auto parse_cli(int argc, char** argv) -> CliResult;

} // namespace goggles::app
