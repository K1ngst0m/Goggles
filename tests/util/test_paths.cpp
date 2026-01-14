#include "util/paths.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

class EnvVarGuard {
public:
    EnvVarGuard(std::string key, std::optional<std::string> value) : m_key(std::move(key)) {
        const char* prev = std::getenv(m_key.c_str());
        if (prev != nullptr) {
            m_prev = std::string(prev);
        }

        if (value.has_value()) {
            setenv(m_key.c_str(), value->c_str(), 1);
        } else {
            unsetenv(m_key.c_str());
        }
    }

    ~EnvVarGuard() {
        if (m_prev.has_value()) {
            setenv(m_key.c_str(), m_prev->c_str(), 1);
        } else {
            unsetenv(m_key.c_str());
        }
    }

    EnvVarGuard(const EnvVarGuard&) = delete;
    EnvVarGuard& operator=(const EnvVarGuard&) = delete;
    EnvVarGuard(EnvVarGuard&&) = delete;
    EnvVarGuard& operator=(EnvVarGuard&&) = delete;

private:
    std::string m_key;
    std::optional<std::string> m_prev;
};

class TempDir {
public:
    explicit TempDir(std::string_view name_prefix) {
        auto base = std::filesystem::temp_directory_path();
        auto tmpl_path = base / (std::string(name_prefix) + "-XXXXXX");
        auto tmpl = tmpl_path.string();

        std::vector<char> buf;
        buf.reserve(tmpl.size() + 1);
        buf.insert(buf.end(), tmpl.begin(), tmpl.end());
        buf.push_back('\0');

        char* dir = mkdtemp(buf.data());
        if (dir == nullptr) {
            m_path = base / std::string(name_prefix);
            std::error_code ec;
            std::filesystem::create_directories(m_path, ec);
            return;
        }

        m_path = std::filesystem::path(dir);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return m_path; }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

private:
    std::filesystem::path m_path;
};

auto create_resource_root(const std::filesystem::path& root) -> void {
    std::error_code ec;
    std::filesystem::create_directories(root / "config", ec);
    std::filesystem::create_directories(root / "shaders", ec);

    std::FILE* f = std::fopen((root / "config" / "goggles.template.toml").c_str(), "wb");
    if (f != nullptr) {
        std::fclose(f);
    }
}

} // namespace

TEST_CASE("paths: merge_overrides prefers high", "[paths]") {
    goggles::util::PathOverrides low{};
    low.cache_dir = "/low/cache";
    low.data_dir = "/low/data";

    goggles::util::PathOverrides high{};
    high.cache_dir = "/high/cache";

    auto merged = goggles::util::merge_overrides({.high = high, .low = low});
    REQUIRE(merged.cache_dir == std::filesystem::path("/high/cache"));
    REQUIRE(merged.data_dir == std::filesystem::path("/low/data"));
}

TEST_CASE("paths: resolve_config_dir uses override", "[paths]") {
    goggles::util::PathOverrides overrides{};
    overrides.config_dir = "/tmp/goggles-test-config";
    auto result = goggles::util::resolve_config_dir(overrides);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == std::filesystem::path("/tmp/goggles-test-config"));
}

TEST_CASE("paths: resolve_config_dir uses XDG_CONFIG_HOME", "[paths]") {
    TempDir tmp("goggles_paths");
    auto xdg_config = (tmp.path() / "xdg_config").string();

    EnvVarGuard home("HOME", (tmp.path() / "home").string());
    EnvVarGuard xdg("XDG_CONFIG_HOME", xdg_config);

    goggles::util::PathOverrides overrides{};
    auto result = goggles::util::resolve_config_dir(overrides);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == std::filesystem::path(xdg_config) / "goggles");
}

TEST_CASE("paths: resolve_app_dirs resolves resource_dir via sentinel root", "[paths]") {
    TempDir tmp("goggles_paths");
    const auto resource_root = tmp.path() / "resources";
    create_resource_root(resource_root);

    const auto xdg_config = tmp.path() / "xdg_config";
    const auto xdg_data = tmp.path() / "xdg_data";
    const auto xdg_cache = tmp.path() / "xdg_cache";
    const auto xdg_runtime = tmp.path() / "xdg_runtime";

    EnvVarGuard home("HOME", (tmp.path() / "home").string());
    EnvVarGuard config_home("XDG_CONFIG_HOME", xdg_config.string());
    EnvVarGuard data_home("XDG_DATA_HOME", xdg_data.string());
    EnvVarGuard cache_home("XDG_CACHE_HOME", xdg_cache.string());
    EnvVarGuard runtime_dir("XDG_RUNTIME_DIR", xdg_runtime.string());
    EnvVarGuard resource_env("GOGGLES_RESOURCE_DIR", std::nullopt);
    EnvVarGuard appdir_env("APPDIR", std::nullopt);

    goggles::util::ResolveContext ctx{
        .exe_dir = tmp.path(),
        .cwd = resource_root,
    };

    auto result = goggles::util::resolve_app_dirs(ctx, {});
    REQUIRE(result.has_value());

    const auto& dirs = result.value();
    REQUIRE(dirs.resource_dir == resource_root.lexically_normal());
    REQUIRE(dirs.config_dir == (xdg_config / "goggles").lexically_normal());
    REQUIRE(dirs.data_dir == (xdg_data / "goggles").lexically_normal());
    REQUIRE(dirs.cache_dir == (xdg_cache / "goggles").lexically_normal());
    REQUIRE(dirs.runtime_dir == (xdg_runtime / "goggles").lexically_normal());
}

TEST_CASE("paths: resolve_app_dirs rejects relative overrides", "[paths]") {
    TempDir tmp("goggles_paths");
    goggles::util::ResolveContext ctx{
        .exe_dir = tmp.path(),
        .cwd = tmp.path(),
    };

    goggles::util::PathOverrides overrides{};
    overrides.cache_dir = "relative/cache";
    auto result = goggles::util::resolve_app_dirs(ctx, overrides);
    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == goggles::ErrorCode::invalid_config);
}

TEST_CASE("paths: join helpers normalize", "[paths]") {
    goggles::util::AppDirs dirs{
        .resource_dir = "/tmp/goggles_res",
        .config_dir = "/tmp/goggles_cfg",
        .data_dir = "/tmp/goggles_data",
        .cache_dir = "/tmp/goggles_cache",
        .runtime_dir = "/tmp/goggles_run",
    };

    REQUIRE(goggles::util::resource_path(dirs, "a/../b") ==
            std::filesystem::path("/tmp/goggles_res/b"));
    REQUIRE(goggles::util::config_path(dirs, "x/./y") ==
            std::filesystem::path("/tmp/goggles_cfg/x/y"));
}
