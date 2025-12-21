#pragma once

#include "error.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace goggles {

enum class ScaleMode : uint8_t {
    fit,
    fill,
    stretch,
    integer,
};
;

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
    }
    return "unknown";
}

struct Config {
    struct Capture {
        std::string backend = "vulkan_layer";
    } capture;

    struct Shader {
        std::string preset;
    } shader;

    struct Render {
        bool vsync = true;
        uint32_t target_fps = 60;
        bool enable_validation = false;
        ScaleMode scale_mode = ScaleMode::stretch;
        uint32_t integer_scale = 0;
    } render;

    struct Logging {
        std::string level = "info";
        std::string file;
    } logging;
};

[[nodiscard]] auto load_config(const std::filesystem::path& path) -> Result<Config>;
[[nodiscard]] auto default_config() -> Config;

} // namespace goggles
