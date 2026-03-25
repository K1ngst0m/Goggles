#pragma once

#include "scale_mode.hpp"

#include <cstdint>
#include <filesystem>
#include <goggles/error.hpp>
#include <string>
#include <string_view>

namespace goggles {

struct Config {
    struct Paths {
        std::string resource_dir;
        std::string config_dir;
        std::string data_dir;
        std::string cache_dir;
        std::string runtime_dir;
    } paths;

    struct Shader {
        std::string preset;
    } shader;

    struct Render {
        bool vsync = true;
        uint32_t target_fps = 60; // 0 = uncapped
        bool enable_validation = false;
        ScaleMode scale_mode = ScaleMode::fill;
        uint32_t integer_scale = 0;
        std::string gpu_selector;
        // Injected from CLI --app-width/--app-height; not parsed from TOML.
        uint32_t source_width = 0;
        uint32_t source_height = 0;
    } render;

    struct Logging {
        std::string level = "info";
        std::string file;
        bool timestamp = false;
    } logging;
};

[[nodiscard]] auto load_config(const std::filesystem::path& path) -> Result<Config>;
[[nodiscard]] auto default_config() -> Config;

/// Relative paths resolve against `config_path.parent_path()`; empty returns empty.
[[nodiscard]] auto resolve_logging_file_path(std::string_view logging_file,
                                             const std::filesystem::path& config_path)
    -> std::filesystem::path;

} // namespace goggles
