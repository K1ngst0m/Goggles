#include "preset_parser.hpp"

#include <charconv>
#include <fstream>
#include <optional>
#include <regex>
#include <format>
#include <sstream>
#include <unordered_map>
#include <util/logging.hpp>

namespace goggles::render {

namespace {

auto read_file(const std::filesystem::path& path) -> Result<std::string> {
    std::ifstream file(path);
    if (!file) {
        return make_error<std::string>(ErrorCode::FILE_NOT_FOUND,
                                       "Failed to open preset: " + path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto trim(const std::string& str) -> std::string {
    auto start = str.find_first_not_of(" \t\r\n\"");
    if (start == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\r\n\"");
    return str.substr(start, end - start + 1);
}

auto parse_bool(const std::string& value) -> bool {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes";
}

auto parse_float_safe(const std::string& str, float default_value) -> float {
    float result = default_value;
    std::from_chars(str.data(), str.data() + str.size(), result);
    return result;
}

auto parse_int_safe(const std::string& str) -> std::optional<int> {
    int result = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
    if (ec == std::errc{}) {
        return result;
    }
    return std::nullopt;
}

auto parse_wrap_mode_value(const std::string& value) -> WrapMode {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "clamp_to_edge") {
        return WrapMode::CLAMP_TO_EDGE;
    }
    if (lower == "repeat") {
        return WrapMode::REPEAT;
    }
    if (lower == "mirrored_repeat") {
        return WrapMode::MIRRORED_REPEAT;
    }
    return WrapMode::CLAMP_TO_BORDER;
}

using ValueMap = std::unordered_map<std::string, std::string>;

void parse_textures(const ValueMap& values, const std::filesystem::path& base_path,
                    std::vector<TextureConfig>& textures) {
    auto textures_it = values.find("textures");
    if (textures_it == values.end()) {
        return;
    }

    std::string textures_str = textures_it->second;
    std::istringstream tex_stream(textures_str);
    std::string tex_name;
    while (std::getline(tex_stream, tex_name, ';')) {
        tex_name = trim(tex_name);
        if (tex_name.empty()) {
            continue;
        }

        TextureConfig tex;
        tex.name = tex_name;

        auto tex_path_it = values.find(tex_name);
        if (tex_path_it != values.end()) {
            tex.path = base_path / tex_path_it->second;
        }

        auto tex_linear_it = values.find(tex_name + "_linear");
        if (tex_linear_it != values.end()) {
            bool is_linear = parse_bool(tex_linear_it->second);
            tex.linear = is_linear;
            tex.filter_mode = is_linear ? FilterMode::LINEAR : FilterMode::NEAREST;
        }

        auto tex_mipmap_it = values.find(tex_name + "_mipmap");
        if (tex_mipmap_it != values.end()) {
            tex.mipmap = parse_bool(tex_mipmap_it->second);
        }

        auto tex_wrap_it = values.find(tex_name + "_wrap_mode");
        if (tex_wrap_it != values.end()) {
            tex.wrap_mode = parse_wrap_mode_value(tex_wrap_it->second);
        }

        textures.push_back(std::move(tex));
    }
}

void parse_parameters(const ValueMap& values, std::vector<ParameterOverride>& parameters) {
    for (const auto& [key, value] : values) {
        if (key.starts_with("shader") || key.starts_with("scale") || key.starts_with("filter") ||
            key.starts_with("float") || key.starts_with("srgb") || key.starts_with("alias") ||
            key.starts_with("mipmap") || key.starts_with("wrap_mode") || key == "shaders" ||
            key == "textures" || key.find("_linear") != std::string::npos ||
            key.find("_mipmap") != std::string::npos ||
            key.find("_wrap_mode") != std::string::npos) {
            continue;
        }

        float param_value = 0.0F;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), param_value);
        if (ec == std::errc{}) {
            parameters.push_back({.name = key, .value = param_value});
        }
    }
}

} // namespace

auto PresetParser::load(const std::filesystem::path& preset_path) -> Result<PresetConfig> {
    auto content_result = read_file(preset_path);
    if (!content_result) {
        return make_error<PresetConfig>(content_result.error().code,
                                        content_result.error().message);
    }

    return parse_ini(content_result.value(), preset_path.parent_path());
}

auto PresetParser::parse_ini(const std::string& content, const std::filesystem::path& base_path)
    -> Result<PresetConfig> {
    PresetConfig config;
    std::unordered_map<std::string, std::string> values;

    // Parse all key = value pairs
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string value = trim(trimmed.substr(eq_pos + 1));

        values[key] = value;
    }

    // Get shader count
    auto shaders_it = values.find("shaders");
    if (shaders_it == values.end()) {
        return make_error<PresetConfig>(ErrorCode::PARSE_ERROR, "Preset missing 'shaders' count");
    }

    auto shader_count_opt = parse_int_safe(shaders_it->second);
    if (!shader_count_opt) {
        return make_error<PresetConfig>(ErrorCode::PARSE_ERROR,
                                        "Invalid 'shaders' count: " + shaders_it->second);
    }
    int shader_count = *shader_count_opt;

    // Parse each shader pass
    for (int i = 0; i < shader_count; ++i) {
        ShaderPassConfig pass;
        std::string prefix = "shader" + std::to_string(i);
        std::string scale_prefix = "scale" + std::to_string(i);
        std::string filter_prefix = "filter_linear" + std::to_string(i);
        std::string float_prefix = "float_framebuffer" + std::to_string(i);
        std::string srgb_prefix = "srgb_framebuffer" + std::to_string(i);
        std::string alias_prefix = "alias" + std::to_string(i);
        std::string mipmap_prefix = "mipmap_input" + std::to_string(i);

        // Shader path
        auto shader_it = values.find(prefix);
        if (shader_it == values.end()) {
            return make_error<PresetConfig>(ErrorCode::PARSE_ERROR,
                                            "Missing shader path for pass " + std::to_string(i));
        }
        pass.shader_path = base_path / shader_it->second;

        // Scale type (default: source)
        auto scale_type_it = values.find("scale_type" + std::to_string(i));
        if (scale_type_it != values.end()) {
            pass.scale_type_x = parse_scale_type(scale_type_it->second);
            pass.scale_type_y = pass.scale_type_x;
        }

        // Separate X/Y scale types
        auto scale_type_x_it = values.find("scale_type_x" + std::to_string(i));
        if (scale_type_x_it != values.end()) {
            pass.scale_type_x = parse_scale_type(scale_type_x_it->second);
        }
        auto scale_type_y_it = values.find("scale_type_y" + std::to_string(i));
        if (scale_type_y_it != values.end()) {
            pass.scale_type_y = parse_scale_type(scale_type_y_it->second);
        }

        // Scale factors
        auto scale_it = values.find(scale_prefix);
        if (scale_it != values.end()) {
            pass.scale_x = parse_float_safe(scale_it->second, 1.0F);
            pass.scale_y = pass.scale_x;
        }
        auto scale_x_it = values.find("scale_x" + std::to_string(i));
        if (scale_x_it != values.end()) {
            pass.scale_x = parse_float_safe(scale_x_it->second, 1.0F);
        }
        auto scale_y_it = values.find("scale_y" + std::to_string(i));
        if (scale_y_it != values.end()) {
            pass.scale_y = parse_float_safe(scale_y_it->second, 1.0F);
        }

        // Filter mode
        auto filter_it = values.find(filter_prefix);
        if (filter_it != values.end()) {
            pass.filter_mode =
                parse_bool(filter_it->second) ? FilterMode::LINEAR : FilterMode::NEAREST;
        }

        // Framebuffer format
        bool is_float = false;
        bool is_srgb = false;
        auto float_it = values.find(float_prefix);
        if (float_it != values.end()) {
            is_float = parse_bool(float_it->second);
        }
        auto srgb_it = values.find(srgb_prefix);
        if (srgb_it != values.end()) {
            is_srgb = parse_bool(srgb_it->second);
        }
        pass.framebuffer_format = parse_format(is_float, is_srgb);

        // Alias
        auto alias_it = values.find(alias_prefix);
        if (alias_it != values.end()) {
            pass.alias = alias_it->second;
        }

        // Mipmap
        auto mipmap_it = values.find(mipmap_prefix);
        if (mipmap_it != values.end()) {
            pass.mipmap = parse_bool(mipmap_it->second);
        }

        // Wrap mode
        auto wrap_prefix = std::format("wrap_mode{}", i);
        auto wrap_it = values.find(wrap_prefix);
        if (wrap_it != values.end()) {
            pass.wrap_mode = parse_wrap_mode_value(wrap_it->second);
        }

        config.passes.push_back(std::move(pass));
    }

    parse_textures(values, base_path, config.textures);
    parse_parameters(values, config.parameters);

    GOGGLES_LOG_INFO("Loaded preset with {} passes, {} textures, {} parameter overrides",
                     config.passes.size(), config.textures.size(), config.parameters.size());

    return config;
}

auto PresetParser::parse_scale_type(const std::string& value) -> ScaleType {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "viewport") {
        return ScaleType::VIEWPORT;
    }
    if (lower == "absolute") {
        return ScaleType::ABSOLUTE;
    }
    return ScaleType::SOURCE;
}

auto PresetParser::parse_format(bool is_float, bool is_srgb) -> vk::Format {
    if (is_float) {
        return vk::Format::eR16G16B16A16Sfloat;
    }
    if (is_srgb) {
        return vk::Format::eR8G8B8A8Srgb;
    }
    return vk::Format::eR8G8B8A8Unorm;
}

auto PresetParser::parse_wrap_mode(const std::string& value) -> WrapMode {
    return parse_wrap_mode_value(value);
}

} // namespace goggles::render
