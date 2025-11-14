#include "config.hpp"

#include <toml.hpp>

#include <fstream>

namespace goggles {

auto default_config() -> Config {
    return Config{}; // Uses struct defaults
}

auto load_config(const std::filesystem::path& path) -> Result<Config> {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        return make_error<Config>(ErrorCode::file_not_found,
                                   "Configuration file not found: " + path.string());
    }

    // Parse TOML file
    toml::value data;
    try {
        data = toml::parse(path);
    } catch (const std::exception& e) {
        return make_error<Config>(ErrorCode::parse_error,
                                   "Failed to parse TOML: " + std::string(e.what()));
    }

    // Start with defaults
    Config config = default_config();

    // [capture]
    if (data.contains("capture")) {
        const auto capture = toml::find(data, "capture");
        if (capture.contains("backend")) {
            config.capture.backend = toml::find<std::string>(capture, "backend");

            // Validate backend value
            if (config.capture.backend != "vulkan_layer" &&
                config.capture.backend != "compositor") {
                return make_error<Config>(
                    ErrorCode::invalid_config,
                    "Invalid capture backend: " + config.capture.backend +
                        " (expected: vulkan_layer or compositor)");
            }
        }
    }

    // [pipeline]
    if (data.contains("pipeline")) {
        const auto pipeline = toml::find(data, "pipeline");
        if (pipeline.contains("shader_preset")) {
            config.pipeline.shader_preset =
                toml::find<std::string>(pipeline, "shader_preset");
        }
    }

    // [render]
    if (data.contains("render")) {
        const auto render = toml::find(data, "render");
        if (render.contains("vsync")) {
            config.render.vsync = toml::find<bool>(render, "vsync");
        }
        if (render.contains("target_fps")) {
            auto fps = toml::find<int64_t>(render, "target_fps");
            if (fps <= 0 || fps > 1000) {
                return make_error<Config>(
                    ErrorCode::invalid_config,
                    "Invalid target_fps: " + std::to_string(fps) +
                        " (expected: 1-1000)");
            }
            config.render.target_fps = static_cast<uint32_t>(fps);
        }
    }

    // [logging]
    if (data.contains("logging")) {
        const auto logging = toml::find(data, "logging");
        if (logging.contains("level")) {
            config.logging.level = toml::find<std::string>(logging, "level");

            // Validate log level
            const auto& level = config.logging.level;
            if (level != "trace" && level != "debug" && level != "info" &&
                level != "warn" && level != "error" && level != "critical") {
                return make_error<Config>(
                    ErrorCode::invalid_config,
                    "Invalid log level: " + level +
                        " (expected: trace, debug, info, warn, error, critical)");
            }
        }
        if (logging.contains("file")) {
            config.logging.file = toml::find<std::string>(logging, "file");
        }
    }

    return config;
}

} // namespace goggles
