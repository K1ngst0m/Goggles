#include "paths.hpp"

#include "config.hpp"
#include "profiling.hpp"

#include <cstdlib>
#include <optional>
#include <string>

namespace goggles::util {

namespace {

auto is_absolute_or_empty(const std::filesystem::path& path) -> bool {
    return path.empty() || path.is_absolute();
}

auto get_env(std::string_view key) -> std::optional<std::string> {
    const char* value = std::getenv(std::string(key).c_str());
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

auto get_env_path(std::string_view key) -> std::optional<std::filesystem::path> {
    auto value = get_env(key);
    if (!value) {
        return std::nullopt;
    }
    std::filesystem::path p(*value);
    if (!p.is_absolute()) {
        return std::nullopt;
    }
    return p;
}

auto resolve_home_dir() -> std::optional<std::filesystem::path> {
    auto home = get_env_path("HOME");
    if (!home) {
        return std::nullopt;
    }
    return home;
}

auto resolve_xdg_root(std::string_view xdg_key, const std::filesystem::path& home_fallback)
    -> std::optional<std::filesystem::path> {
    if (auto env = get_env_path(xdg_key)) {
        return env;
    }
    return home_fallback;
}

auto resolve_config_root() -> std::optional<std::filesystem::path> {
    auto home = resolve_home_dir();
    if (!home) {
        return std::nullopt;
    }
    return resolve_xdg_root("XDG_CONFIG_HOME", *home / ".config");
}

auto resolve_data_root() -> std::optional<std::filesystem::path> {
    auto home = resolve_home_dir();
    if (!home) {
        return std::nullopt;
    }
    return resolve_xdg_root("XDG_DATA_HOME", *home / ".local" / "share");
}

auto resolve_cache_root() -> std::optional<std::filesystem::path> {
    auto home = resolve_home_dir();
    if (!home) {
        return std::nullopt;
    }
    return resolve_xdg_root("XDG_CACHE_HOME", *home / ".cache");
}

auto resolve_runtime_root() -> std::filesystem::path {
    if (auto env = get_env_path("XDG_RUNTIME_DIR")) {
        return *env;
    }
    try {
        return std::filesystem::temp_directory_path();
    } catch (...) {
        return "/tmp";
    }
}

auto is_resource_root(const std::filesystem::path& candidate) -> bool {
    std::error_code ec;
    const auto config_template = candidate / "config" / "goggles.template.toml";
    if (!std::filesystem::is_regular_file(config_template, ec) || ec) {
        return false;
    }

    const auto shaders_dir = candidate / "shaders";
    return std::filesystem::is_directory(shaders_dir, ec) && !ec;
}

auto find_resource_root(const ResolveContext& ctx) -> std::optional<std::filesystem::path> {
    GOGGLES_PROFILE_FUNCTION();
    if (auto resource_dir = get_env_path("GOGGLES_RESOURCE_DIR")) {
        if (is_resource_root(*resource_dir)) {
            return resource_dir->lexically_normal();
        }
    }

    if (auto appdir = get_env_path("APPDIR")) {
        if (is_resource_root(*appdir)) {
            return appdir->lexically_normal();
        }
    }

    {
        std::filesystem::path current = ctx.exe_dir;
        for (int depth = 0; depth < 8 && !current.empty(); ++depth) {
            if (is_resource_root(current)) {
                return current.lexically_normal();
            }
            auto parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = std::move(parent);
        }
    }

    if (!ctx.cwd.empty() && is_resource_root(ctx.cwd)) {
        return ctx.cwd.lexically_normal();
    }

    return std::nullopt;
}

} // namespace

auto merge_overrides(const OverrideMerge& merge) -> PathOverrides {
    PathOverrides merged = merge.low;

    if (!merge.high.resource_dir.empty()) {
        merged.resource_dir = merge.high.resource_dir;
    }
    if (!merge.high.config_dir.empty()) {
        merged.config_dir = merge.high.config_dir;
    }
    if (!merge.high.data_dir.empty()) {
        merged.data_dir = merge.high.data_dir;
    }
    if (!merge.high.cache_dir.empty()) {
        merged.cache_dir = merge.high.cache_dir;
    }
    if (!merge.high.runtime_dir.empty()) {
        merged.runtime_dir = merge.high.runtime_dir;
    }

    return merged;
}

auto overrides_from_config(const Config& config) -> PathOverrides {
    PathOverrides overrides{};

    if (!config.paths.resource_dir.empty()) {
        overrides.resource_dir = config.paths.resource_dir;
    }
    if (!config.paths.config_dir.empty()) {
        overrides.config_dir = config.paths.config_dir;
    }
    if (!config.paths.data_dir.empty()) {
        overrides.data_dir = config.paths.data_dir;
    }
    if (!config.paths.cache_dir.empty()) {
        overrides.cache_dir = config.paths.cache_dir;
    }
    if (!config.paths.runtime_dir.empty()) {
        overrides.runtime_dir = config.paths.runtime_dir;
    }

    return overrides;
}

auto resolve_config_dir(const PathOverrides& overrides) -> Result<std::filesystem::path> {
    GOGGLES_PROFILE_FUNCTION();
    if (!is_absolute_or_empty(overrides.config_dir)) {
        return make_error<std::filesystem::path>(ErrorCode::invalid_config,
                                                 "paths.config_dir must be an absolute path");
    }

    if (!overrides.config_dir.empty()) {
        return overrides.config_dir.lexically_normal();
    }

    auto root = resolve_config_root();
    if (!root) {
        return make_error<std::filesystem::path>(ErrorCode::invalid_data,
                                                 "Unable to resolve XDG config directory");
    }

    return (*root / "goggles").lexically_normal();
}

auto resolve_app_dirs(const ResolveContext& ctx, const PathOverrides& overrides)
    -> Result<AppDirs> {
    GOGGLES_PROFILE_FUNCTION();
    if (!is_absolute_or_empty(overrides.resource_dir) ||
        !is_absolute_or_empty(overrides.config_dir) || !is_absolute_or_empty(overrides.data_dir) ||
        !is_absolute_or_empty(overrides.cache_dir) ||
        !is_absolute_or_empty(overrides.runtime_dir)) {
        return make_error<AppDirs>(ErrorCode::invalid_config,
                                   "paths.* overrides must be absolute paths");
    }

    std::filesystem::path resource_dir;
    if (!overrides.resource_dir.empty()) {
        resource_dir = overrides.resource_dir.lexically_normal();
    } else if (auto found = find_resource_root(ctx)) {
        resource_dir = *found;
    } else if (!ctx.cwd.empty()) {
        resource_dir = ctx.cwd.lexically_normal();
    } else {
        return make_error<AppDirs>(ErrorCode::file_not_found, "Unable to resolve resource_dir");
    }

    auto config_dir = GOGGLES_TRY(resolve_config_dir(overrides));

    std::filesystem::path data_dir;
    if (!overrides.data_dir.empty()) {
        data_dir = overrides.data_dir.lexically_normal();
    } else {
        auto root = resolve_data_root();
        if (!root) {
            return make_error<AppDirs>(ErrorCode::invalid_data,
                                       "Unable to resolve XDG data directory");
        }
        data_dir = (*root / "goggles").lexically_normal();
    }

    std::filesystem::path cache_dir;
    if (!overrides.cache_dir.empty()) {
        cache_dir = overrides.cache_dir.lexically_normal();
    } else {
        auto root = resolve_cache_root();
        if (!root) {
            return make_error<AppDirs>(ErrorCode::invalid_data,
                                       "Unable to resolve XDG cache directory");
        }
        cache_dir = (*root / "goggles").lexically_normal();
    }

    std::filesystem::path runtime_dir;
    if (!overrides.runtime_dir.empty()) {
        runtime_dir = overrides.runtime_dir.lexically_normal();
    } else {
        runtime_dir = (resolve_runtime_root() / "goggles").lexically_normal();
    }

    return AppDirs{
        .resource_dir = std::move(resource_dir),
        .config_dir = std::move(config_dir),
        .data_dir = std::move(data_dir),
        .cache_dir = std::move(cache_dir),
        .runtime_dir = std::move(runtime_dir),
    };
}

auto resource_path(const AppDirs& dirs, const std::filesystem::path& rel) -> std::filesystem::path {
    return (dirs.resource_dir / rel).lexically_normal();
}

auto config_path(const AppDirs& dirs, const std::filesystem::path& rel) -> std::filesystem::path {
    return (dirs.config_dir / rel).lexically_normal();
}

auto data_path(const AppDirs& dirs, const std::filesystem::path& rel) -> std::filesystem::path {
    return (dirs.data_dir / rel).lexically_normal();
}

auto cache_path(const AppDirs& dirs, const std::filesystem::path& rel) -> std::filesystem::path {
    return (dirs.cache_dir / rel).lexically_normal();
}

auto runtime_path(const AppDirs& dirs, const std::filesystem::path& rel) -> std::filesystem::path {
    return (dirs.runtime_dir / rel).lexically_normal();
}

} // namespace goggles::util
