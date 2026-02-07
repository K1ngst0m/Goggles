#include "retroarch_preprocessor.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <util/logging.hpp>
#include <util/profiling.hpp>

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

// Slang doesn't support vec *= mat compound assignment
auto fix_compound_assign(const std::string& source) -> std::string {
    static const std::regex COMPOUND_ASSIGN(R"((\b\w+(?:\.\w+)?)\s*\*=\s*([^;]+);)");
    std::smatch match;
    std::string::const_iterator search_start = source.cbegin();
    std::string output;
    size_t last_pos = 0;

    auto is_matrix_expr = [](const std::string& expr) {
        return expr.find("mat") != std::string::npos ||
               expr.find("transpose") != std::string::npos ||
               expr.find("inverse") != std::string::npos || expr.find("IPT") != std::string::npos ||
               expr.find("LMS") != std::string::npos || expr.find("CAT") != std::string::npos ||
               expr.find("RGB") != std::string::npos || expr.find("XYZ") != std::string::npos ||
               expr.find("YUV") != std::string::npos || expr.find("color") != std::string::npos;
    };

    while (std::regex_search(search_start, source.cend(), match, COMPOUND_ASSIGN)) {
        std::string var = match[1].str();
        std::string expr = match[2].str();
        if (is_matrix_expr(expr)) {
            auto match_pos =
                static_cast<size_t>(match.position() + (search_start - source.cbegin()));
            output += source.substr(last_pos, match_pos - last_pos);
            output.append(var).append(" = ").append(var).append(" * (").append(expr).append(");");
            last_pos = match_pos + static_cast<size_t>(match.length());
        }
        search_start = match.suffix().first;
    }
    output += source.substr(last_pos);
    return output;
}

// Slang doesn't support mat3==mat3 in ternary (returns bmat3 instead of bool)
auto fix_matrix_compare(const std::string& source) -> std::string {
    // Replace (m_in==m_ou) with (m_in[0]==m_ou[0] && m_in[1]==m_ou[1] && m_in[2]==m_ou[2])
    static const std::regex MAT_COMPARE(R"(\((\w+)\s*==\s*(\w+)\))");
    std::smatch match;

    auto is_matrix_var = [](const std::string& name) {
        return name.starts_with("m_") || name.find("_mat") != std::string::npos ||
               name.find("prims") != std::string::npos;
    };

    std::string::const_iterator search_start = source.cbegin();
    std::string output;
    size_t last_pos = 0;

    while (std::regex_search(search_start, source.cend(), match, MAT_COMPARE)) {
        std::string lhs = match[1].str();
        std::string rhs = match[2].str();
        if (is_matrix_var(lhs) && is_matrix_var(rhs)) {
            auto match_pos =
                static_cast<size_t>(match.position() + (search_start - source.cbegin()));
            output += source.substr(last_pos, match_pos - last_pos);
            output.append("(").append(lhs).append("[0]==").append(rhs).append("[0] && ");
            output.append(lhs).append("[1]==").append(rhs).append("[1] && ");
            output.append(lhs).append("[2]==").append(rhs).append("[2])");
            last_pos = match_pos + static_cast<size_t>(match.length());
        }
        search_start = match.suffix().first;
    }
    output += source.substr(last_pos);
    return output;
}

auto fix_slang_compat(const std::string& source) -> std::string {
    std::string result = fix_compound_assign(source);
    result = fix_matrix_compare(result);
    return result;
}

} // namespace

auto RetroArchPreprocessor::preprocess(const std::filesystem::path& shader_path)
    -> Result<PreprocessedShader> {
    GOGGLES_PROFILE_FUNCTION();
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
    GOGGLES_PROFILE_FUNCTION();
    // Step 1: Resolve includes
    auto resolved_result = resolve_includes(source, base_path);
    if (!resolved_result) {
        return make_error<PreprocessedShader>(resolved_result.error().code,
                                              resolved_result.error().message);
    }
    std::string resolved = std::move(resolved_result.value());

    // Step 1.5: Fix Slang incompatibilities (vec *= mat -> vec = vec * mat)
    resolved = fix_slang_compat(resolved);

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
    GOGGLES_PROFILE_FUNCTION();
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
    GOGGLES_PROFILE_FUNCTION();
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
    GOGGLES_PROFILE_FUNCTION();
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
    GOGGLES_PROFILE_FUNCTION();
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
