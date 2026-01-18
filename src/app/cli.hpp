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
    bool detach = false;
    bool wsi_proxy = false;
    uint32_t app_width = 0;
    uint32_t app_height = 0;
    std::string dump_dir;
    std::string dump_frame_range;
    std::string dump_frame_mode;
    bool layer_log = false;
    std::string layer_log_level;
    bool enable_input_forwarding = false;
    std::optional<uint32_t> target_fps;
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
