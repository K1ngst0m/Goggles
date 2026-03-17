#pragma once

#include "error.hpp"
#include "scale_mode.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace goggles {

/// @brief Parsed application configuration.
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

/// @brief Loads a configuration file from disk.
/// @param path Path to a TOML configuration file.
/// @return Parsed configuration or an error.
[[nodiscard]] auto load_config(const std::filesystem::path& path) -> Result<Config>;

/// @brief Returns a default configuration.
[[nodiscard]] auto default_config() -> Config;

/// @brief Resolves `logging.file` into an effective filesystem path.
/// @param logging_file Raw `[logging].file` value from config.
/// @param config_path Path of the config file that provided `logging_file`.
/// @return Empty path when `logging_file` is empty; otherwise absolute path as-is or a relative
/// path
///         resolved against `config_path.parent_path()`.
[[nodiscard]] auto resolve_logging_file_path(std::string_view logging_file,
                                             const std::filesystem::path& config_path)
    -> std::filesystem::path;

} // namespace goggles
