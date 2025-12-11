#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

enum class ScaleType { SOURCE, VIEWPORT, ABSOLUTE };

enum class FilterMode { LINEAR, NEAREST };

struct ShaderPassConfig {
    std::filesystem::path shader_path;
    ScaleType scale_type_x = ScaleType::SOURCE;
    ScaleType scale_type_y = ScaleType::SOURCE;
    float scale_x = 1.0F;
    float scale_y = 1.0F;
    FilterMode filter_mode = FilterMode::LINEAR;
    vk::Format framebuffer_format = vk::Format::eR8G8B8A8Unorm;
    bool mipmap = false;
    std::optional<std::string> alias;
};

struct TextureConfig {
    std::string name;
    std::filesystem::path path;
    FilterMode filter_mode = FilterMode::LINEAR;
    bool mipmap = false;
};

struct ParameterOverride {
    std::string name;
    float value;
};

struct PresetConfig {
    std::vector<ShaderPassConfig> passes;
    std::vector<TextureConfig> textures;
    std::vector<ParameterOverride> parameters;
};

class PresetParser {
public:
    [[nodiscard]] auto load(const std::filesystem::path& preset_path) -> Result<PresetConfig>;

private:
    [[nodiscard]] auto parse_ini(const std::string& content,
                                 const std::filesystem::path& base_path) -> Result<PresetConfig>;

    [[nodiscard]] auto parse_scale_type(const std::string& value) -> ScaleType;
    [[nodiscard]] auto parse_format(bool is_float, bool is_srgb) -> vk::Format;
};

} // namespace goggles::render
