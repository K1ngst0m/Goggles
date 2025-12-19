#include "config.hpp"

#include <fstream>
#include <toml.hpp>

namespace goggles {

auto default_config() -> Config {
    return Config{}; // Uses struct defaults
}

auto load_config(const std::filesystem::path& path) -> Result<Config> {
    if (!std::filesystem::exists(path)) {
        return make_error<Config>(ErrorCode::FILE_NOT_FOUND,
                                  "Configuration file not found: " + path.string());
    }

    toml::value data;
    try {
        data = toml::parse(path);
    } catch (const std::exception& e) {
        return make_error<Config>(ErrorCode::PARSE_ERROR,
                                  "Failed to parse TOML: " + std::string(e.what()));
    }

    Config config = default_config();

    if (data.contains("capture")) {
        const auto capture = toml::find(data, "capture");
        if (capture.contains("backend")) {
            config.capture.backend = toml::find<std::string>(capture, "backend");

            // Validate backend value
            if (config.capture.backend != "vulkan_layer" &&
                config.capture.backend != "compositor") {
                return make_error<Config>(ErrorCode::INVALID_CONFIG,
                                          "Invalid capture backend: " + config.capture.backend +
                                              " (expected: vulkan_layer or compositor)");
            }
        }
    }

    if (data.contains("shader")) {
        const auto shader = toml::find(data, "shader");
        if (shader.contains("preset")) {
            config.shader.preset = toml::find<std::string>(shader, "preset");
        }
    }

    if (data.contains("render")) {
        const auto render = toml::find(data, "render");
        if (render.contains("vsync")) {
            config.render.vsync = toml::find<bool>(render, "vsync");
        }
        if (render.contains("target_fps")) {
            auto fps = toml::find<int64_t>(render, "target_fps");
            if (fps <= 0 || fps > 1000) {
                return make_error<Config>(ErrorCode::INVALID_CONFIG,
                                          "Invalid target_fps: " + std::to_string(fps) +
                                              " (expected: 1-1000)");
            }
            config.render.target_fps = static_cast<uint32_t>(fps);
        }
        if (render.contains("enable_validation")) {
            config.render.enable_validation = toml::find<bool>(render, "enable_validation");
        }
        if (render.contains("scale_mode")) {
            auto mode_str = toml::find<std::string>(render, "scale_mode");
            if (mode_str == "fit") {
                config.render.scale_mode = ScaleMode::FIT;
            } else if (mode_str == "fill") {
                config.render.scale_mode = ScaleMode::FILL;
            } else if (mode_str == "stretch") {
                config.render.scale_mode = ScaleMode::STRETCH;
            } else if (mode_str == "integer") {
                config.render.scale_mode = ScaleMode::INTEGER;
            } else {
                return make_error<Config>(ErrorCode::INVALID_CONFIG,
                                          "Invalid scale_mode: " + mode_str +
                                              " (expected: fit, fill, stretch, integer)");
            }
        }
        if (render.contains("integer_scale")) {
            auto scale = toml::find<int64_t>(render, "integer_scale");
            if (scale < 0 || scale > 8) {
                return make_error<Config>(ErrorCode::INVALID_CONFIG,
                                          "Invalid integer_scale: " + std::to_string(scale) +
                                              " (expected: 0-8)");
            }
            config.render.integer_scale = static_cast<uint32_t>(scale);
        }
    }

    if (data.contains("logging")) {
        const auto logging = toml::find(data, "logging");
        if (logging.contains("level")) {
            config.logging.level = toml::find<std::string>(logging, "level");

            // Validate log level
            const auto& level = config.logging.level;
            if (level != "trace" && level != "debug" && level != "info" && level != "warn" &&
                level != "error" && level != "critical") {
                return make_error<Config>(
                    ErrorCode::INVALID_CONFIG,
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
