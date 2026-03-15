#pragma once

#include <filesystem>
#include <goggles/filter_chain/result.hpp>
#include <goggles_filter_chain.h>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::filter_chain::runtime {
struct ResolvedSource;
class SourceResolver;
} // namespace goggles::filter_chain::runtime

namespace goggles::render {

enum class ScaleType : std::uint8_t { source, viewport, absolute };

enum class FilterMode : std::uint8_t { linear, nearest };

enum class WrapMode : std::uint8_t { clamp_to_border, clamp_to_edge, repeat, mirrored_repeat };

/// @brief Shader pass configuration parsed from a preset file.
struct ShaderPassConfig {
    std::filesystem::path shader_path;
    ScaleType scale_type_x = ScaleType::source;
    ScaleType scale_type_y = ScaleType::source;
    float scale_x = 1.0F;
    float scale_y = 1.0F;
    FilterMode filter_mode = FilterMode::linear;
    vk::Format framebuffer_format = vk::Format::eR8G8B8A8Unorm;
    bool mipmap = false;
    WrapMode wrap_mode = WrapMode::clamp_to_edge;
    std::optional<std::string> alias;
    uint32_t frame_count_mod = 0; // 0 = disabled
};

/// @brief Texture configuration parsed from a preset file.
struct TextureConfig {
    std::string name;
    std::filesystem::path path;
    FilterMode filter_mode = FilterMode::linear;
    bool mipmap = false;
    WrapMode wrap_mode = WrapMode::clamp_to_border;
    bool linear = false;
};

/// @brief Parameter override parsed from a preset file.
struct ParameterOverride {
    std::string name;
    float value;
};

/// @brief Parsed preset configuration.
struct PresetConfig {
    std::vector<ShaderPassConfig> passes;
    std::vector<TextureConfig> textures;
    std::vector<ParameterOverride> parameters;
};

/// @brief Parses RetroArch-style shader preset files (.slangp).
class PresetParser {
public:
    static constexpr int MAX_REFERENCE_DEPTH = 8;

    /// @brief Loads a preset from disk, including referenced presets.
    /// @param preset_path Path to the preset file.
    /// @return Parsed configuration or an error.
    [[nodiscard]] auto load(const std::filesystem::path& preset_path) -> Result<PresetConfig>;

    /// @brief Loads a preset from a resolved source (file or memory).
    ///
    /// The resolved source provides already-loaded bytes plus a base_path for
    /// relative reference resolution. When import_callbacks is non-null, relative
    /// includes are resolved through the callback before falling back to base_path
    /// filesystem resolution.
    ///
    /// @param resolved   Pre-resolved source bytes with provenance and base_path.
    /// @param resolver   Source resolver for relative #reference and include resolution.
    /// @param import_callbacks  Optional import callbacks for host-driven resolution.
    /// @return Parsed configuration or an error.
    [[nodiscard]] auto load(const filter_chain::runtime::ResolvedSource& resolved,
                            filter_chain::runtime::SourceResolver& resolver,
                            const goggles_fc_import_callbacks_t* import_callbacks)
        -> Result<PresetConfig>;

private:
    [[nodiscard]] auto load_recursive(const std::filesystem::path& preset_path, int depth,
                                      std::vector<std::filesystem::path>& visited)
        -> Result<PresetConfig>;

    [[nodiscard]] auto load_recursive_resolved(
        const std::string& content, const std::filesystem::path& base_path, int depth,
        std::vector<std::string>& visited_names, filter_chain::runtime::SourceResolver& resolver,
        const goggles_fc_import_callbacks_t* import_callbacks) -> Result<PresetConfig>;

    [[nodiscard]] auto parse_ini(const std::string& content, const std::filesystem::path& base_path)
        -> Result<PresetConfig>;

    [[nodiscard]] static auto parse_reference(const std::string& content)
        -> std::optional<std::string>;

    [[nodiscard]] auto parse_scale_type(const std::string& value) -> ScaleType;
    [[nodiscard]] auto parse_wrap_mode(const std::string& value) -> WrapMode;
    [[nodiscard]] auto parse_format(bool is_float, bool is_srgb) -> vk::Format;
};

} // namespace goggles::render
