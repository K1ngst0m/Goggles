#pragma once

#include "slang_reflect.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <util/error.hpp>
#include <vector>

namespace goggles::render {

enum class ShaderStage : std::uint8_t { vertex, fragment };

/// @brief SPIR-V plus entry point name.
struct CompiledShader {
    std::vector<uint32_t> spirv;
    std::string entry_point;
};

/// @brief Compiled RetroArch pass (vertex+fragment) with reflection metadata.
struct RetroArchCompiledShader {
    std::vector<uint32_t> vertex_spirv;
    std::vector<uint32_t> fragment_spirv;
    ReflectionData vertex_reflection;
    ReflectionData fragment_reflection;
};

/// @brief Compiles shaders and caches SPIR-V to disk.
class ShaderRuntime {
public:
    /// @brief Creates a shader runtime using `cache_dir` for persistent caches.
    /// @return A runtime or an error.
    [[nodiscard]] static auto create(const std::filesystem::path& cache_dir)
        -> ResultPtr<ShaderRuntime>;

    ~ShaderRuntime();

    ShaderRuntime(const ShaderRuntime&) = delete;
    ShaderRuntime& operator=(const ShaderRuntime&) = delete;
    ShaderRuntime(ShaderRuntime&&) noexcept;
    ShaderRuntime& operator=(ShaderRuntime&&) noexcept;

    /// @brief Releases compiler resources.
    void shutdown();

    /// @brief Compiles a shader file and returns SPIR-V for `entry_point`.
    [[nodiscard]] auto compile_shader(const std::filesystem::path& source_path,
                                      const std::string& entry_point = "main")
        -> Result<CompiledShader>;

    /// @brief Compiles a RetroArch shader pass and returns SPIR-V plus reflection.
    [[nodiscard]] auto compile_retroarch_shader(const std::string& vertex_source,
                                                const std::string& fragment_source,
                                                const std::string& module_name)
        -> Result<RetroArchCompiledShader>;

    /// @brief Returns the cache directory used by this runtime.
    [[nodiscard]] auto get_cache_dir() const -> std::filesystem::path;

private:
    ShaderRuntime();
    [[nodiscard]] auto get_cache_path(const std::filesystem::path& source_path,
                                      const std::string& entry_point) const
        -> std::filesystem::path;
    [[nodiscard]] auto compute_source_hash(const std::string& source) const -> std::string;
    [[nodiscard]] auto load_cached_spirv(const std::filesystem::path& cache_path,
                                         const std::string& expected_hash)
        -> Result<std::vector<uint32_t>>;
    [[nodiscard]] auto save_cached_spirv(const std::filesystem::path& cache_path,
                                         const std::string& source_hash,
                                         const std::vector<uint32_t>& spirv) -> Result<void>;
    [[nodiscard]] auto compile_slang(const std::string& module_name, const std::string& source,
                                     const std::string& entry_point)
        -> Result<std::vector<uint32_t>>;
    [[nodiscard]] auto compile_glsl(const std::string& module_name, const std::string& source,
                                    const std::string& entry_point, ShaderStage stage)
        -> Result<std::vector<uint32_t>>;

    // Internal result struct for GLSL compilation with reflection
    struct GlslCompileResult;
    [[nodiscard]] auto compile_glsl_with_reflection(const std::string& module_name,
                                                    const std::string& source,
                                                    const std::string& entry_point,
                                                    ShaderStage stage) -> Result<GlslCompileResult>;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::filesystem::path m_cache_dir;
};

} // namespace goggles::render
