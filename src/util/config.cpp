#include "config.hpp"

#include "profiling.hpp"

#include <toml.hpp>

namespace goggles {

namespace {

auto validate_absolute_or_empty(const std::string& value, const char* name) -> Result<void> {
    if (value.empty()) {
        return {};
    }
    const std::filesystem::path p{value};
    if (!p.is_absolute()) {
        return make_error<void>(ErrorCode::invalid_config,
                                std::string("[paths].") + name + " must be an absolute path");
    }
    return {};
}

auto parse_paths(const toml::value& data, Config& config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    try {
        if (!data.contains("paths")) {
            return {};
        }

        const auto paths = toml::find(data, "paths");
        if (paths.contains("resource_dir")) {
            config.paths.resource_dir = toml::find<std::string>(paths, "resource_dir");
        }
        if (paths.contains("config_dir")) {
            config.paths.config_dir = toml::find<std::string>(paths, "config_dir");
        }
        if (paths.contains("data_dir")) {
            config.paths.data_dir = toml::find<std::string>(paths, "data_dir");
        }
        if (paths.contains("cache_dir")) {
            config.paths.cache_dir = toml::find<std::string>(paths, "cache_dir");
        }
        if (paths.contains("runtime_dir")) {
            config.paths.runtime_dir = toml::find<std::string>(paths, "runtime_dir");
        }

        GOGGLES_TRY(validate_absolute_or_empty(config.paths.resource_dir, "resource_dir"));
        GOGGLES_TRY(validate_absolute_or_empty(config.paths.config_dir, "config_dir"));
        GOGGLES_TRY(validate_absolute_or_empty(config.paths.data_dir, "data_dir"));
        GOGGLES_TRY(validate_absolute_or_empty(config.paths.cache_dir, "cache_dir"));
        GOGGLES_TRY(validate_absolute_or_empty(config.paths.runtime_dir, "runtime_dir"));

        return {};
    } catch (const std::exception& e) {
        return make_error<void>(ErrorCode::invalid_config,
                                "Invalid [paths] configuration: " + std::string(e.what()));
    }
}

auto parse_capture(const toml::value& data, Config& config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    try {
        if (!data.contains("capture")) {
            return {};
        }
        const auto capture = toml::find(data, "capture");
        if (!capture.contains("backend")) {
            return {};
        }

        config.capture.backend = toml::find<std::string>(capture, "backend");
        if (config.capture.backend != "vulkan_layer" && config.capture.backend != "compositor") {
            return make_error<void>(ErrorCode::invalid_config,
                                    "Invalid capture backend: " + config.capture.backend +
                                        " (expected: vulkan_layer or compositor)");
        }

        return {};
    } catch (const std::exception& e) {
        return make_error<void>(ErrorCode::invalid_config,
                                "Invalid [capture] configuration: " + std::string(e.what()));
    }
}

auto parse_shader(const toml::value& data, Config& config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    try {
        if (!data.contains("shader")) {
            return {};
        }
        const auto shader = toml::find(data, "shader");
        if (shader.contains("preset")) {
            config.shader.preset = toml::find<std::string>(shader, "preset");
        }
        return {};
    } catch (const std::exception& e) {
        return make_error<void>(ErrorCode::invalid_config,
                                "Invalid [shader] configuration: " + std::string(e.what()));
    }
}

auto parse_render(const toml::value& data, Config& config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    try {
        if (!data.contains("render")) {
            return {};
        }
        const auto render = toml::find(data, "render");
        if (render.contains("vsync")) {
            config.render.vsync = toml::find<bool>(render, "vsync");
        }
        if (render.contains("target_fps")) {
            auto fps = toml::find<int64_t>(render, "target_fps");
            if (fps < 0 || fps > 1000) {
                return make_error<void>(ErrorCode::invalid_config,
                                        "Invalid target_fps: " + std::to_string(fps) +
                                            " (expected: 0-1000, 0=uncapped)");
            }
            config.render.target_fps = static_cast<uint32_t>(fps);
        }
        if (render.contains("enable_validation")) {
            config.render.enable_validation = toml::find<bool>(render, "enable_validation");
        }
        if (render.contains("scale_mode")) {
            auto mode_str = toml::find<std::string>(render, "scale_mode");
            if (mode_str == "fit") {
                config.render.scale_mode = ScaleMode::fit;
            } else if (mode_str == "fill") {
                config.render.scale_mode = ScaleMode::fill;
            } else if (mode_str == "stretch") {
                config.render.scale_mode = ScaleMode::stretch;
            } else if (mode_str == "integer") {
                config.render.scale_mode = ScaleMode::integer;
            } else if (mode_str == "dynamic") {
                config.render.scale_mode = ScaleMode::dynamic;
            } else {
                return make_error<void>(ErrorCode::invalid_config,
                                        "Invalid scale_mode: " + mode_str +
                                            " (expected: fit, fill, stretch, integer, dynamic)");
            }
        }
        if (render.contains("integer_scale")) {
            auto scale = toml::find<int64_t>(render, "integer_scale");
            if (scale < 0 || scale > 8) {
                return make_error<void>(ErrorCode::invalid_config,
                                        "Invalid integer_scale: " + std::to_string(scale) +
                                            " (expected: 0-8)");
            }
            config.render.integer_scale = static_cast<uint32_t>(scale);
        }

        return {};
    } catch (const std::exception& e) {
        return make_error<void>(ErrorCode::invalid_config,
                                "Invalid [render] configuration: " + std::string(e.what()));
    }
}

auto parse_logging(const toml::value& data, Config& config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    try {
        if (!data.contains("logging")) {
            return {};
        }
        const auto logging = toml::find(data, "logging");
        if (logging.contains("level")) {
            config.logging.level = toml::find<std::string>(logging, "level");

            const auto& level = config.logging.level;
            if (level != "trace" && level != "debug" && level != "info" && level != "warn" &&
                level != "error" && level != "critical") {
                return make_error<void>(
                    ErrorCode::invalid_config,
                    "Invalid log level: " + level +
                        " (expected: trace, debug, info, warn, error, critical)");
            }
        }
        if (logging.contains("file")) {
            config.logging.file = toml::find<std::string>(logging, "file");
        }
        if (logging.contains("timestamp")) {
            config.logging.timestamp = toml::find<bool>(logging, "timestamp");
        }
        return {};
    } catch (const std::exception& e) {
        return make_error<void>(ErrorCode::invalid_config,
                                "Invalid [logging] configuration: " + std::string(e.what()));
    }
}

} // namespace

auto default_config() -> Config {
    return Config{}; // Uses struct defaults
}

auto load_config(const std::filesystem::path& path) -> Result<Config> {
    GOGGLES_PROFILE_FUNCTION();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) {
            return make_error<Config>(ErrorCode::file_read_failed, "Failed to stat config file '" +
                                                                       path.string() +
                                                                       "': " + ec.message());
        }
        return make_error<Config>(ErrorCode::file_not_found,
                                  "Configuration file not found: " + path.string());
    }

    toml::value data;
    try {
        data = toml::parse(path);
    } catch (const std::exception& e) {
        return make_error<Config>(ErrorCode::parse_error,
                                  "Failed to parse TOML: " + std::string(e.what()));
    }

    Config config = default_config();

    GOGGLES_TRY(parse_paths(data, config));
    GOGGLES_TRY(parse_capture(data, config));
    GOGGLES_TRY(parse_shader(data, config));
    GOGGLES_TRY(parse_render(data, config));
    GOGGLES_TRY(parse_logging(data, config));

    return config;
}

auto resolve_logging_file_path(std::string_view logging_file,
                               const std::filesystem::path& config_path) -> std::filesystem::path {
    if (logging_file.empty()) {
        return {};
    }

    std::filesystem::path candidate{logging_file};
    if (candidate.is_absolute()) {
        return candidate.lexically_normal();
    }

    const auto base_dir = config_path.parent_path();
    if (base_dir.empty()) {
        return candidate.lexically_normal();
    }

    return (base_dir / candidate).lexically_normal();
}

} // namespace goggles
