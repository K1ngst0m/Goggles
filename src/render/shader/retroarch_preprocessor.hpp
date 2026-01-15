#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <util/error.hpp>
#include <vector>

namespace goggles::render {

/// @brief User-facing shader parameter metadata.
struct ShaderParameter {
    std::string name;
    std::string description;
    float default_value;
    float min_value;
    float max_value;
    float step;
};

/// @brief Optional shader metadata extracted from source.
struct ShaderMetadata {
    std::optional<std::string> name_alias;
    std::optional<std::string> format;
};

/// @brief Preprocessed vertex+fragment sources plus parameters/metadata.
struct PreprocessedShader {
    std::string vertex_source;
    std::string fragment_source;
    std::vector<ShaderParameter> parameters;
    ShaderMetadata metadata;
};

/// @brief Preprocesses RetroArch .slangp/.slang shaders into per-stage sources.
class RetroArchPreprocessor {
public:
    /// @brief Loads and preprocesses a shader file from disk.
    [[nodiscard]] auto preprocess(const std::filesystem::path& shader_path)
        -> Result<PreprocessedShader>;

    /// @brief Preprocesses a shader source string using `base_path` for includes.
    [[nodiscard]] auto preprocess_source(const std::string& source,
                                         const std::filesystem::path& base_path)
        -> Result<PreprocessedShader>;

private:
    [[nodiscard]] auto resolve_includes(const std::string& source,
                                        const std::filesystem::path& base_path, int depth = 0)
        -> Result<std::string>;

    [[nodiscard]] auto split_by_stage(const std::string& source)
        -> std::pair<std::string, std::string>;

    [[nodiscard]] auto extract_parameters(const std::string& source)
        -> std::pair<std::string, std::vector<ShaderParameter>>;

    [[nodiscard]] auto extract_metadata(const std::string& source)
        -> std::pair<std::string, ShaderMetadata>;

    static constexpr int MAX_INCLUDE_DEPTH = 32;
};

} // namespace goggles::render
