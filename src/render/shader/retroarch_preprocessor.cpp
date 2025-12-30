#include "retroarch_preprocessor.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <util/logging.hpp>

namespace goggles::render {

namespace {

constexpr std::string_view PRAGMA_STAGE_VERTEX = "#pragma stage vertex";
constexpr std::string_view PRAGMA_STAGE_FRAGMENT = "#pragma stage fragment";

auto read_file(const std::filesystem::path& path) -> Result<std::string> {
    std::ifstream file(path);
    if (!file) {
        return make_error<std::string>(ErrorCode::file_not_found,
                                       "Failed to open file: " + path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto trim(const std::string& str) -> std::string {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace

auto RetroArchPreprocessor::preprocess(const std::filesystem::path& shader_path)
    -> Result<PreprocessedShader> {
    auto source_result = read_file(shader_path);
    if (!source_result) {
        return make_error<PreprocessedShader>(source_result.error().code,
                                              source_result.error().message);
    }

    return preprocess_source(source_result.value(), shader_path.parent_path());
}

auto RetroArchPreprocessor::preprocess_source(const std::string& source,
                                              const std::filesystem::path& base_path)
    -> Result<PreprocessedShader> {
    // Step 1: Resolve includes
    auto resolved_result = resolve_includes(source, base_path);
    if (!resolved_result) {
        return make_error<PreprocessedShader>(resolved_result.error().code,
                                              resolved_result.error().message);
    }
    std::string resolved = std::move(resolved_result.value());

    // Step 2: Extract parameters (removes pragma lines from source)
    auto [after_params, parameters] = extract_parameters(resolved);

    // Step 3: Extract metadata (removes pragma lines from source)
    auto [after_metadata, metadata] = extract_metadata(after_params);

    // Step 4: Split by stage
    auto [vertex, fragment] = split_by_stage(after_metadata);

    return PreprocessedShader{
        .vertex_source = std::move(vertex),
        .fragment_source = std::move(fragment),
        .parameters = std::move(parameters),
        .metadata = std::move(metadata),
    };
}

auto RetroArchPreprocessor::resolve_includes(const std::string& source,
                                             const std::filesystem::path& base_path, int depth)
    -> Result<std::string> {
    if (depth > MAX_INCLUDE_DEPTH) {
        return make_error<std::string>(ErrorCode::parse_error,
                                       "Maximum include depth exceeded (circular include?)");
    }

    // Match #include "path" or #include <path>
    std::regex include_regex(R"(^\s*#include\s*["<]([^">]+)[">])");
    std::string result;
    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, include_regex)) {
            std::string include_path_str = match[1].str();
            std::filesystem::path include_path = base_path / include_path_str;

            auto include_source = read_file(include_path);
            if (!include_source) {
                return make_error<std::string>(ErrorCode::file_not_found,
                                               "Failed to resolve include: " +
                                                   include_path.string());
            }

            // Recursively resolve includes in the included file
            auto resolved =
                resolve_includes(include_source.value(), include_path.parent_path(), depth + 1);
            if (!resolved) {
                return resolved;
            }

            result += resolved.value();
            result += "\n";
        } else {
            result += line;
            result += "\n";
        }
    }

    return result;
}

auto RetroArchPreprocessor::split_by_stage(const std::string& source)
    -> std::pair<std::string, std::string> {
    enum class Stage : std::uint8_t { shared, vertex, fragment };

    std::string shared;
    std::string vertex;
    std::string fragment;
    std::string* current = &shared;
    Stage current_stage = Stage::shared;

    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);

        if (trimmed.starts_with(PRAGMA_STAGE_VERTEX)) {
            if (current_stage != Stage::vertex) {
                current = &vertex;
                vertex = shared;
                current_stage = Stage::vertex;
            }
            continue;
        }

        if (trimmed.starts_with(PRAGMA_STAGE_FRAGMENT)) {
            if (current_stage != Stage::fragment) {
                current = &fragment;
                fragment = shared;
                current_stage = Stage::fragment;
            }
            continue;
        }

        *current += line;
        *current += "\n";
    }

    if (vertex.empty() && fragment.empty()) {
        vertex = source;
        fragment = source;
    }

    return {vertex, fragment};
}

auto RetroArchPreprocessor::extract_parameters(const std::string& source)
    -> std::pair<std::string, std::vector<ShaderParameter>> {
    std::vector<ShaderParameter> parameters;
    std::string result;

    // #pragma parameter NAME "Description" default min max step
    // Using custom delimiter to handle quotes in the pattern
    std::regex param_regex(
        R"regex(^\s*#pragma\s+parameter\s+(\w+)\s+"([^"]+)"\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+))regex");

    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, param_regex)) {
            ShaderParameter param;
            param.name = match[1].str();
            param.description = match[2].str();
            param.default_value = std::stof(match[3].str());
            param.min_value = std::stof(match[4].str());
            param.max_value = std::stof(match[5].str());
            param.step = std::stof(match[6].str());
            parameters.push_back(param);
            // Don't add the pragma line to output
        } else {
            result += line;
            result += "\n";
        }
    }

    return {result, parameters};
}

auto RetroArchPreprocessor::extract_metadata(const std::string& source)
    -> std::pair<std::string, ShaderMetadata> {
    ShaderMetadata metadata;
    std::string result;

    std::regex name_regex(R"(^\s*#pragma\s+name\s+(\S+))");
    std::regex format_regex(R"(^\s*#pragma\s+format\s+(\S+))");

    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, name_regex)) {
            metadata.name_alias = match[1].str();
            // Don't add the pragma line to output
        } else if (std::regex_search(line, match, format_regex)) {
            metadata.format = match[1].str(); // NOLINT(bugprone-branch-clone)
            // Don't add the pragma line to output
        } else {
            result += line;
            result += "\n";
        }
    }

    return {result, metadata};
}

} // namespace goggles::render
