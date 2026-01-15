#pragma once

#include "error.hpp"

#include <filesystem>

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

/// @brief Resolved directory roots for app filesystem operations.
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

/// @brief Merges override sets, preferring non-empty fields from `merge.high`.
/// @param merge The override sets to combine.
/// @return The merged overrides.
[[nodiscard]] auto merge_overrides(const OverrideMerge& merge) -> PathOverrides;

/// @brief Extracts path overrides from a parsed configuration.
/// @param config The parsed app configuration.
/// @return The overrides defined by the configuration.
[[nodiscard]] auto overrides_from_config(const Config& config) -> PathOverrides;

/// @brief Resolves the writable config directory for Goggles.
/// @param overrides The optional config directory override.
/// @return The resolved config directory.
[[nodiscard]] auto resolve_config_dir(const PathOverrides& overrides)
    -> Result<std::filesystem::path>;

/// @brief Resolves app directory roots using overrides and XDG defaults.
/// @param ctx The process context used for resource discovery.
/// @param overrides The optional directory root overrides.
/// @return The resolved directory roots.
[[nodiscard]] auto resolve_app_dirs(const ResolveContext& ctx, const PathOverrides& overrides)
    -> Result<AppDirs>;

/// @brief Joins `rel` under the resolved resource directory.
/// @param dirs The resolved directory roots.
/// @param rel The relative path under the resource directory.
/// @return The combined path.
[[nodiscard]] auto resource_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;

/// @brief Joins `rel` under the resolved config directory.
/// @param dirs The resolved directory roots.
/// @param rel The relative path under the config directory.
/// @return The combined path.
[[nodiscard]] auto config_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;

/// @brief Joins `rel` under the resolved data directory.
/// @param dirs The resolved directory roots.
/// @param rel The relative path under the data directory.
/// @return The combined path.
[[nodiscard]] auto data_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;

/// @brief Joins `rel` under the resolved cache directory.
/// @param dirs The resolved directory roots.
/// @param rel The relative path under the cache directory.
/// @return The combined path.
[[nodiscard]] auto cache_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;

/// @brief Joins `rel` under the resolved runtime directory.
/// @param dirs The resolved directory roots.
/// @param rel The relative path under the runtime directory.
/// @return The combined path.
[[nodiscard]] auto runtime_path(const AppDirs& dirs, const std::filesystem::path& rel)
    -> std::filesystem::path;

} // namespace goggles::util
