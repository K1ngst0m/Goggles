#pragma once

#include "error.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace goggles {

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
    } render;

    struct Logging {
        std::string level = "info";
        std::string file;
    } logging;
};

[[nodiscard]] auto load_config(const std::filesystem::path& path) -> Result<Config>;
[[nodiscard]] auto default_config() -> Config;

} // namespace goggles
