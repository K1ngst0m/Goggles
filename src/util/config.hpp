#pragma once

#include "error.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace goggles {

// Configuration structure matching TOML schema
struct Config {
    // [capture]
    struct Capture {
        std::string backend = "vulkan_layer"; // or "compositor"
    } capture;

    // [pipeline]
    struct Pipeline {
        std::string shader_preset; // Empty = no shaders loaded initially
    } pipeline;

    // [render]
    struct Render {
        bool vsync = true;
        uint32_t target_fps = 60;
    } render;

    // [logging]
    struct Logging {
        std::string level = "info"; // trace, debug, info, warn, error, critical
        std::string file;           // Empty = console only
    } logging;
};

// Load configuration from TOML file
// Returns Error if file not found, parse error, or validation fails
[[nodiscard]] auto load_config(const std::filesystem::path& path) -> Result<Config>;

// Get default configuration (without loading from file)
[[nodiscard]] auto default_config() -> Config;

} // namespace goggles
