#pragma once

#include <filesystem>
#include <goggles/error.hpp>

namespace goggles {
struct Config;
}

namespace goggles::util {

/// @brief Optional directory root overrides for path resolution.
///
/// Leave fields empty to use XDG/environment defaults. Non-empty overrides must be absolute paths.
struct PathOverrides {
    std::filesystem::path resource_dir;
    std::filesystem::path config_dir;
    std::filesystem::path data_dir;
    std::filesystem::path cache_dir;
    std::filesystem::path runtime_dir;
};

/// @brief Process context for resolving packaged resources.
///
/// Uses `exe_dir` to search for packaged assets and falls back to `cwd` for developer workflows.
struct ResolveContext {
    std::filesystem::path exe_dir;
    std::filesystem::path cwd;
};

struct AppDirs {
    std::filesystem::path resource_dir;
    std::filesystem::path config_dir;
    std::filesystem::path data_dir;
    std::filesystem::path cache_dir;
    std::filesystem::path runtime_dir;
};

/// @brief Override inputs grouped to avoid ambiguous parameter ordering.
struct OverrideMerge {
    const PathOverrides& high;
    const PathOverrides& low;
};

[[nodiscard]] auto merge_overrides(const OverrideMerge& merge) -> PathOverrides;
[[nodiscard]] auto overrides_from_config(const Config& config) -> PathOverrides;
[[nodiscard]] auto resolve_config_dir(const PathOverrides& overrides)
    -> Result<std::filesystem::path>;
[[nodiscard]] auto resolve_app_dirs(const ResolveContext& ctx, const PathOverrides& overrides)
    -> Result<AppDirs>;

[[nodiscard]] auto resource_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;
[[nodiscard]] auto config_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;
[[nodiscard]] auto data_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;
[[nodiscard]] auto cache_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;
[[nodiscard]] auto runtime_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;

} // namespace goggles::util
