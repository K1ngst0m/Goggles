#pragma once

#include "slang_reflect.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <util/error.hpp>
#include <vector>

namespace goggles::render {

enum class ShaderStage : std::uint8_t { vertex, fragment };
;

struct CompiledShader {
    std::vector<uint32_t> spirv;
    std::string entry_point;
};

struct RetroArchCompiledShader {
    std::vector<uint32_t> vertex_spirv;
    std::vector<uint32_t> fragment_spirv;
    ReflectionData vertex_reflection;
    ReflectionData fragment_reflection;
};

class ShaderRuntime {
public:
    ShaderRuntime();
    ~ShaderRuntime();

    ShaderRuntime(const ShaderRuntime&) = delete;
    ShaderRuntime& operator=(const ShaderRuntime&) = delete;
    ShaderRuntime(ShaderRuntime&&) noexcept;
    ShaderRuntime& operator=(ShaderRuntime&&) noexcept;

    [[nodiscard]] auto init() -> Result<void>;
    void shutdown();

    [[nodiscard]] auto compile_shader(const std::filesystem::path& source_path,
                                      const std::string& entry_point = "main")
        -> Result<CompiledShader>;

    [[nodiscard]] auto compile_retroarch_shader(const std::string& vertex_source,
                                                const std::string& fragment_source,
                                                const std::string& module_name)
        -> Result<RetroArchCompiledShader>;

    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

private:
    [[nodiscard]] auto get_cache_dir() const -> std::filesystem::path;
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
    bool m_initialized = false;
};

} // namespace goggles::render
