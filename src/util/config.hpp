#pragma once

#include "error.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace goggles {

/// @brief How the output image scales to the target rectangle.
enum class ScaleMode : uint8_t {
    fit,
    fill,
    stretch,
    integer,
    dynamic,
};

/// @brief Returns the config string for a `ScaleMode` value.
/// @param mode Scale mode value.
/// @return Stable string identifier.
[[nodiscard]] constexpr auto to_string(ScaleMode mode) -> const char* {
    switch (mode) {
    case ScaleMode::fit:
        return "fit";
    case ScaleMode::fill:
        return "fill";
    case ScaleMode::stretch:
        return "stretch";
    case ScaleMode::integer:
        return "integer";
    case ScaleMode::dynamic:
        return "dynamic";
    }
    return "unknown";
}

/// @brief Parsed application configuration.
struct Config {
    struct Paths {
        std::string resource_dir;
        std::string config_dir;
        std::string data_dir;
        std::string cache_dir;
        std::string runtime_dir;
    } paths;

    struct Capture {
        std::string backend = "vulkan_layer";
    } capture;

    struct Input {
        bool forwarding = false;
    } input;

    struct Shader {
        std::string preset;
    } shader;

    struct Render {
        bool vsync = true;
        uint32_t target_fps = 60; // 0 = uncapped
        bool enable_validation = false;
        ScaleMode scale_mode = ScaleMode::fill;
        uint32_t integer_scale = 0;
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

} // namespace goggles
