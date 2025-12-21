#include "shader_runtime.hpp"

#include "slang_reflect.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>
#include <sstream>
#include <util/logging.hpp>

namespace goggles::render {

namespace {

constexpr std::string_view CACHE_SUBDIR = "goggles/shaders";
constexpr std::string_view CACHE_MAGIC = "GSPV";
constexpr uint32_t CACHE_VERSION = 1;

struct CacheHeader {
    std::array<char, 4> magic;
    uint32_t version;
    uint32_t hash_length;
    uint32_t spirv_size;
};

} // namespace

// Internal result for GLSL compilation including reflection
struct ShaderRuntime::GlslCompileResult {
    std::vector<uint32_t> spirv;
    ReflectionData reflection;
};

struct ShaderRuntime::Impl {
    Slang::ComPtr<slang::IGlobalSession> global_session;
    Slang::ComPtr<slang::ISession> hlsl_session;
    Slang::ComPtr<slang::ISession> glsl_session;
};

ShaderRuntime::ShaderRuntime() : m_impl(std::make_unique<Impl>()) {}

ShaderRuntime::~ShaderRuntime() {
    shutdown();
}

ShaderRuntime::ShaderRuntime(ShaderRuntime&&) noexcept = default;
ShaderRuntime& ShaderRuntime::operator=(ShaderRuntime&&) noexcept = default;

auto ShaderRuntime::init() -> Result<void> {
    if (m_initialized) {
        return {};
    }

    SlangGlobalSessionDesc global_desc = {};
    global_desc.enableGLSL = true;

    if (SLANG_FAILED(slang::createGlobalSession(&global_desc, m_impl->global_session.writeRef()))) {
        return make_error<void>(ErrorCode::shader_compile_failed,
                                "Failed to create Slang global session");
    }

    slang::TargetDesc target_desc = {};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = m_impl->global_session->findProfile("spirv_1_3");

    std::array<slang::CompilerOptionEntry, 2> options = {
        {{.name = slang::CompilerOptionName::EmitSpirvDirectly,
          .value = {.kind = slang::CompilerOptionValueKind::Int,
                    .intValue0 = 1,
                    .intValue1 = 0,
                    .stringValue0 = nullptr,
                    .stringValue1 = nullptr}},
         {.name = slang::CompilerOptionName::Optimization,
          .value = {.kind = slang::CompilerOptionValueKind::Int,
                    .intValue0 = SLANG_OPTIMIZATION_LEVEL_HIGH,
                    .intValue1 = 0,
                    .stringValue0 = nullptr,
                    .stringValue1 = nullptr}}}};

    // HLSL session (existing behavior)
    slang::SessionDesc hlsl_session_desc = {};
    hlsl_session_desc.targets = &target_desc;
    hlsl_session_desc.targetCount = 1;
    hlsl_session_desc.compilerOptionEntries = options.data();
    hlsl_session_desc.compilerOptionEntryCount = options.size();

    if (SLANG_FAILED(m_impl->global_session->createSession(hlsl_session_desc,
                                                           m_impl->hlsl_session.writeRef()))) {
        return make_error<void>(ErrorCode::shader_compile_failed,
                                "Failed to create Slang HLSL session");
    }

    // GLSL session for RetroArch shaders
    slang::SessionDesc glsl_session_desc = {};
    glsl_session_desc.targets = &target_desc;
    glsl_session_desc.targetCount = 1;
    glsl_session_desc.compilerOptionEntries = options.data();
    glsl_session_desc.compilerOptionEntryCount = options.size();
    glsl_session_desc.allowGLSLSyntax = true;

    if (SLANG_FAILED(m_impl->global_session->createSession(glsl_session_desc,
                                                           m_impl->glsl_session.writeRef()))) {
        return make_error<void>(ErrorCode::shader_compile_failed,
                                "Failed to create Slang GLSL session");
    }

    auto cache_dir = get_cache_dir();
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) {
        GOGGLES_LOG_WARN("Failed to create shader cache directory: {}", ec.message());
    }

    m_initialized = true;
    GOGGLES_LOG_INFO("ShaderRuntime initialized (dual session: HLSL + GLSL), cache: {}",
                     cache_dir.string());
    return {};
}

void ShaderRuntime::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_impl->glsl_session = nullptr;
    m_impl->hlsl_session = nullptr;
    m_impl->global_session = nullptr;

    m_initialized = false;
    GOGGLES_LOG_DEBUG("ShaderRuntime shutdown");
}

auto ShaderRuntime::compile_shader(const std::filesystem::path& source_path,
                                   const std::string& entry_point) -> Result<CompiledShader> {
    if (!m_initialized) {
        return make_error<CompiledShader>(ErrorCode::shader_compile_failed,
                                          "ShaderRuntime not initialized");
    }

    std::ifstream file(source_path);
    if (!file) {
        return make_error<CompiledShader>(ErrorCode::file_not_found,
                                          "Shader file not found: " + source_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    auto source_hash = compute_source_hash(source);
    auto cache_path = get_cache_path(source_path, entry_point);

    auto cached = load_cached_spirv(cache_path, source_hash);
    if (cached) {
        GOGGLES_LOG_DEBUG("Loaded cached SPIR-V: {}", cache_path.filename().string());
        return CompiledShader{.spirv = std::move(cached.value()), .entry_point = entry_point};
    }

    auto module_name = source_path.stem().string();
    auto compiled = GOGGLES_TRY(compile_slang(module_name, source, entry_point));

    auto save_result = save_cached_spirv(cache_path, source_hash, compiled);
    if (!save_result) {
        GOGGLES_LOG_WARN("Failed to cache SPIR-V: {}", save_result.error().message);
    }

    GOGGLES_LOG_INFO("Compiled shader: {} ({})", source_path.filename().string(), entry_point);
    return CompiledShader{.spirv = std::move(compiled), .entry_point = entry_point};
}

auto ShaderRuntime::get_cache_dir() const -> std::filesystem::path {
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    if (xdg_cache != nullptr && xdg_cache[0] != '\0') {
        return std::filesystem::path(xdg_cache) / CACHE_SUBDIR;
    }

    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / ".cache" / CACHE_SUBDIR;
    }

    return std::filesystem::temp_directory_path() / CACHE_SUBDIR;
}

auto ShaderRuntime::get_cache_path(const std::filesystem::path& source_path,
                                   const std::string& entry_point) const -> std::filesystem::path {
    auto filename = source_path.stem().string() + "_" + entry_point + ".spv.cache";
    return get_cache_dir() / filename;
}

auto ShaderRuntime::compute_source_hash(const std::string& source) const -> std::string {
    std::hash<std::string> hasher;
    auto hash = hasher(source);
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

auto ShaderRuntime::load_cached_spirv(const std::filesystem::path& cache_path,
                                      const std::string& expected_hash)
    -> Result<std::vector<uint32_t>> {
    std::ifstream file(cache_path, std::ios::binary);
    if (!file) {
        return make_error<std::vector<uint32_t>>(ErrorCode::file_not_found, "Cache miss");
    }

    CacheHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file) {
        return make_error<std::vector<uint32_t>>(ErrorCode::file_read_failed,
                                                 "Invalid cache header");
    }

    if (std::string_view(header.magic.data(), 4) != CACHE_MAGIC ||
        header.version != CACHE_VERSION) {
        return make_error<std::vector<uint32_t>>(ErrorCode::parse_error, "Cache version mismatch");
    }

    std::string stored_hash(header.hash_length, '\0');
    file.read(stored_hash.data(), static_cast<std::streamsize>(header.hash_length));
    if (!file || stored_hash != expected_hash) {
        return make_error<std::vector<uint32_t>>(ErrorCode::parse_error, "Source hash mismatch");
    }

    std::vector<uint32_t> spirv(header.spirv_size);
    file.read(reinterpret_cast<char*>(spirv.data()),
              static_cast<std::streamsize>(header.spirv_size * sizeof(uint32_t)));
    if (!file) {
        return make_error<std::vector<uint32_t>>(ErrorCode::file_read_failed,
                                                 "Failed to read SPIR-V");
    }

    return spirv;
}

auto ShaderRuntime::save_cached_spirv(const std::filesystem::path& cache_path,
                                      const std::string& source_hash,
                                      const std::vector<uint32_t>& spirv) -> Result<void> {
    std::ofstream file(cache_path, std::ios::binary);
    if (!file) {
        return make_error<void>(ErrorCode::file_write_failed,
                                "Failed to create cache file: " + cache_path.string());
    }

    CacheHeader header{};
    std::copy(CACHE_MAGIC.begin(), CACHE_MAGIC.end(), header.magic.begin());
    header.version = CACHE_VERSION;
    header.hash_length = static_cast<uint32_t>(source_hash.size());
    header.spirv_size = static_cast<uint32_t>(spirv.size());

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(source_hash.data(), static_cast<std::streamsize>(source_hash.size()));
    file.write(reinterpret_cast<const char*>(spirv.data()),
               static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));

    if (!file) {
        return make_error<void>(ErrorCode::file_write_failed, "Failed to write cache file");
    }

    return {};
}

auto ShaderRuntime::compile_slang(const std::string& module_name, const std::string& source,
                                  const std::string& entry_point) -> Result<std::vector<uint32_t>> {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    std::string module_path = module_name + ".slang";
    slang::IModule* module_ptr = m_impl->hlsl_session->loadModuleFromSourceString(
        module_name.c_str(), module_path.c_str(), source.c_str(), diagnostics_blob.writeRef());
    Slang::ComPtr<slang::IModule> module(module_ptr);

    if (diagnostics_blob != nullptr) {
        GOGGLES_LOG_DEBUG("Slang diagnostics: {}",
                          static_cast<const char*>(diagnostics_blob->getBufferPointer()));
    }

    if (module_ptr == nullptr) {
        std::string error_msg = "Failed to load shader module";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<std::vector<uint32_t>>(ErrorCode::shader_compile_failed, error_msg);
    }

    Slang::ComPtr<slang::IEntryPoint> entry_point_obj;
    module->findEntryPointByName(entry_point.c_str(), entry_point_obj.writeRef());

    if (entry_point_obj == nullptr) {
        return make_error<std::vector<uint32_t>>(
            ErrorCode::shader_compile_failed,
            "Entry point '" + entry_point + "' not found. Ensure it has [shader(...)] attribute.");
    }

    std::array<slang::IComponentType*, 2> components = {module, entry_point_obj};
    Slang::ComPtr<slang::IComponentType> composed;
    SlangResult result = m_impl->hlsl_session->createCompositeComponentType(
        components.data(), components.size(), composed.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result)) {
        std::string error_msg = "Failed to compose shader program";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<std::vector<uint32_t>>(ErrorCode::shader_compile_failed, error_msg);
    }

    Slang::ComPtr<slang::IComponentType> linked;
    result = composed->link(linked.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result)) {
        std::string error_msg = "Failed to link shader program";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<std::vector<uint32_t>>(ErrorCode::shader_compile_failed, error_msg);
    }

    Slang::ComPtr<slang::IBlob> spirv_blob;
    result = linked->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result) || (spirv_blob == nullptr)) {
        std::string error_msg = "Failed to get compiled SPIR-V";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<std::vector<uint32_t>>(ErrorCode::shader_compile_failed, error_msg);
    }

    auto spirv_size = spirv_blob->getBufferSize() / sizeof(uint32_t);
    std::vector<uint32_t> spirv(spirv_size);
    std::memcpy(spirv.data(), spirv_blob->getBufferPointer(), spirv_blob->getBufferSize());

    return spirv;
}

auto ShaderRuntime::compile_glsl(const std::string& module_name, const std::string& source,
                                 const std::string& entry_point, ShaderStage stage)
    -> Result<std::vector<uint32_t>> {
    auto result =
        GOGGLES_TRY(compile_glsl_with_reflection(module_name, source, entry_point, stage));
    return std::move(result.spirv);
}

auto ShaderRuntime::compile_glsl_with_reflection(const std::string& module_name,
                                                 const std::string& source,
                                                 const std::string& entry_point, ShaderStage stage)
    -> Result<GlslCompileResult> {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    std::string module_path = module_name + ".glsl";
    slang::IModule* module_ptr = m_impl->glsl_session->loadModuleFromSourceString(
        module_name.c_str(), module_path.c_str(), source.c_str(), diagnostics_blob.writeRef());
    Slang::ComPtr<slang::IModule> module(module_ptr);

    if (diagnostics_blob != nullptr) {
        GOGGLES_LOG_DEBUG("GLSL Slang diagnostics: {}",
                          static_cast<const char*>(diagnostics_blob->getBufferPointer()));
    }

    if (module_ptr == nullptr) {
        std::string error_msg = "Failed to load GLSL shader module";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<GlslCompileResult>(ErrorCode::shader_compile_failed, error_msg);
    }

    // Convert our stage enum to Slang's stage enum
    SlangStage slang_stage =
        (stage == ShaderStage::vertex) ? SLANG_STAGE_VERTEX : SLANG_STAGE_FRAGMENT;

    // Use findAndCheckEntryPoint for GLSL shaders since they don't have [shader(...)] attributes
    Slang::ComPtr<slang::IEntryPoint> entry_point_obj;
    SlangResult result = module->findAndCheckEntryPoint(
        entry_point.c_str(), slang_stage, entry_point_obj.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result) || entry_point_obj == nullptr) {
        std::string error_msg = "Entry point '" + entry_point + "' not found in GLSL shader";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<GlslCompileResult>(ErrorCode::shader_compile_failed, error_msg);
    }

    std::array<slang::IComponentType*, 2> components = {module, entry_point_obj};
    Slang::ComPtr<slang::IComponentType> composed;
    result = m_impl->glsl_session->createCompositeComponentType(
        components.data(), components.size(), composed.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result)) {
        std::string error_msg = "Failed to compose GLSL shader program";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<GlslCompileResult>(ErrorCode::shader_compile_failed, error_msg);
    }

    Slang::ComPtr<slang::IComponentType> linked;
    result = composed->link(linked.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result)) {
        std::string error_msg = "Failed to link GLSL shader program";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<GlslCompileResult>(ErrorCode::shader_compile_failed, error_msg);
    }

    // Get reflection data from linked program
    auto reflection_result = reflect_program(linked.get());
    if (!reflection_result) {
        GOGGLES_LOG_WARN("Failed to get reflection data: {}", reflection_result.error().message);
    }

    Slang::ComPtr<slang::IBlob> spirv_blob;
    result = linked->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics_blob.writeRef());

    if (SLANG_FAILED(result) || (spirv_blob == nullptr)) {
        std::string error_msg = "Failed to get GLSL compiled SPIR-V";
        if (diagnostics_blob != nullptr) {
            error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        }
        return make_error<GlslCompileResult>(ErrorCode::shader_compile_failed, error_msg);
    }

    auto glsl_spirv_size = spirv_blob->getBufferSize() / sizeof(uint32_t);
    std::vector<uint32_t> glsl_spirv(glsl_spirv_size);
    std::memcpy(glsl_spirv.data(), spirv_blob->getBufferPointer(), spirv_blob->getBufferSize());

    return GlslCompileResult{.spirv = std::move(glsl_spirv),
                             .reflection = reflection_result ? std::move(reflection_result.value())
                                                             : ReflectionData{}};
}

auto ShaderRuntime::compile_retroarch_shader(const std::string& vertex_source,
                                             const std::string& fragment_source,
                                             const std::string& module_name)
    -> Result<RetroArchCompiledShader> {
    if (!m_initialized) {
        return make_error<RetroArchCompiledShader>(ErrorCode::shader_compile_failed,
                                                   "ShaderRuntime not initialized");
    }

    auto vertex_result = compile_glsl_with_reflection(module_name + "_vert", vertex_source, "main",
                                                      ShaderStage::vertex);
    if (!vertex_result) {
        return make_error<RetroArchCompiledShader>(ErrorCode::shader_compile_failed,
                                                   "Vertex shader compile failed: " +
                                                       vertex_result.error().message);
    }

    auto fragment_result = compile_glsl_with_reflection(module_name + "_frag", fragment_source,
                                                        "main", ShaderStage::fragment);
    if (!fragment_result) {
        return make_error<RetroArchCompiledShader>(ErrorCode::shader_compile_failed,
                                                   "Fragment shader compile failed: " +
                                                       fragment_result.error().message);
    }

    GOGGLES_LOG_INFO("Compiled RetroArch shader: {}", module_name);
    return RetroArchCompiledShader{.vertex_spirv = std::move(vertex_result->spirv),
                                   .fragment_spirv = std::move(fragment_result->spirv),
                                   .vertex_reflection = std::move(vertex_result->reflection),
                                   .fragment_reflection = std::move(fragment_result->reflection)};
}

} // namespace goggles::render
