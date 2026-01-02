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
#include <util/profiling.hpp>
#include <util/serializer.hpp>

namespace goggles::render {

namespace {

constexpr std::string_view CACHE_SUBDIR = "goggles/shaders";
constexpr std::string_view CACHE_MAGIC = "GSPV";
constexpr std::string_view RETROARCH_CACHE_MAGIC = "GRAC";
constexpr uint32_t CACHE_VERSION = 1;

struct CacheHeader {
    std::array<char, 4> magic;
    uint32_t version;
    uint32_t hash_length;
    uint32_t spirv_size;
};

// Serialization helpers
using util::BinaryReader;
using util::BinaryWriter;

Result<void> write_uniform_member(BinaryWriter& writer, const UniformMember& member) {
    GOGGLES_TRY(writer.write_str(member.name));
    writer.write_pod(member.offset);
    writer.write_pod(member.size);
    return {};
}

bool read_uniform_member(BinaryReader& reader, UniformMember& member) {
    return reader.read_str(member.name) && reader.read_pod(member.offset) &&
           reader.read_pod(member.size);
}

Result<void> write_uniform_layout(BinaryWriter& writer, const UniformBufferLayout& layout) {
    writer.write_pod(layout.binding);
    writer.write_pod(layout.set);
    writer.write_pod(layout.total_size);
    writer.write_pod(layout.stage_flags);
    return writer.write_vec(layout.members, write_uniform_member);
}

bool read_uniform_layout(BinaryReader& reader, UniformBufferLayout& layout) {
    return reader.read_pod(layout.binding) && reader.read_pod(layout.set) &&
           reader.read_pod(layout.total_size) && reader.read_pod(layout.stage_flags) &&
           reader.read_vec(layout.members, read_uniform_member);
}

Result<void> write_push_layout(BinaryWriter& writer, const PushConstantLayout& layout) {
    writer.write_pod(layout.total_size);
    writer.write_pod(layout.stage_flags);
    return writer.write_vec(layout.members, write_uniform_member);
}

bool read_push_layout(BinaryReader& reader, PushConstantLayout& layout) {
    return reader.read_pod(layout.total_size) && reader.read_pod(layout.stage_flags) &&
           reader.read_vec(layout.members, read_uniform_member);
}

Result<void> write_texture_binding(BinaryWriter& writer, const TextureBinding& binding) {
    GOGGLES_TRY(writer.write_str(binding.name));
    writer.write_pod(binding.binding);
    writer.write_pod(binding.set);
    writer.write_pod(binding.stage_flags);
    return {};
}

bool read_texture_binding(BinaryReader& reader, TextureBinding& binding) {
    return reader.read_str(binding.name) && reader.read_pod(binding.binding) &&
           reader.read_pod(binding.set) && reader.read_pod(binding.stage_flags);
}

Result<void> write_vertex_input(BinaryWriter& writer, const VertexInput& input) {
    GOGGLES_TRY(writer.write_str(input.name));
    writer.write_pod(input.location);
    writer.write_pod(input.format);
    writer.write_pod(input.offset);
    return {};
}

bool read_vertex_input(BinaryReader& reader, VertexInput& input) {
    return reader.read_str(input.name) && reader.read_pod(input.location) &&
           reader.read_pod(input.format) && reader.read_pod(input.offset);
}

template <typename T, typename Func>
Result<void> write_optional(BinaryWriter& writer, const std::optional<T>& opt, Func func) {
    bool has_value = opt.has_value();
    writer.write_pod(has_value);
    if (has_value) {
        return func(writer, *opt);
    }
    return {};
}

template <typename T, typename Func>
bool read_optional(BinaryReader& reader, std::optional<T>& opt, Func func) {
    bool has_value = false;
    if (!reader.read_pod(has_value)) {
        return false;
    }
    if (has_value) {
        T val;
        if (!func(reader, val)) {
            return false;
        }
        opt = std::move(val);
    } else {
        opt = std::nullopt;
    }
    return true;
}

Result<void> write_reflection(BinaryWriter& writer, const ReflectionData& reflection) {
    GOGGLES_TRY(write_optional(writer, reflection.ubo, write_uniform_layout));
    GOGGLES_TRY(write_optional(writer, reflection.push_constants, write_push_layout));
    GOGGLES_TRY(writer.write_vec(reflection.textures, write_texture_binding));
    GOGGLES_TRY(writer.write_vec(reflection.vertex_inputs, write_vertex_input));
    return {};
}

bool read_reflection(BinaryReader& reader, ReflectionData& reflection) {
    return read_optional(reader, reflection.ubo, read_uniform_layout) &&
           read_optional(reader, reflection.push_constants, read_push_layout) &&
           reader.read_vec(reflection.textures, read_texture_binding) &&
           reader.read_vec(reflection.vertex_inputs, read_vertex_input);
}

Result<void> write_spirv(BinaryWriter& writer, const std::vector<uint32_t>& spirv) {
    // Write SPIR-V as generic vector of u32
    writer.write_pod(static_cast<uint32_t>(spirv.size()));
    for (auto word : spirv) {
        writer.write_pod(word);
    }
    return {};
}

bool read_spirv(BinaryReader& reader, std::vector<uint32_t>& spirv) {
    uint32_t size = 0;
    if (!reader.read_pod(size)) {
        return false;
    }
    spirv.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!reader.read_pod(spirv[i])) {
            return false;
        }
    }
    return true;
}

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

auto ShaderRuntime::create() -> ResultPtr<ShaderRuntime> {
    GOGGLES_PROFILE_FUNCTION();

    auto runtime = std::unique_ptr<ShaderRuntime>(new ShaderRuntime());

    SlangGlobalSessionDesc global_desc = {};
    global_desc.enableGLSL = true;

    if (SLANG_FAILED(
            slang::createGlobalSession(&global_desc, runtime->m_impl->global_session.writeRef()))) {
        return make_result_ptr_error<ShaderRuntime>(ErrorCode::shader_compile_failed,
                                                    "Failed to create Slang global session");
    }

    slang::TargetDesc target_desc = {};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = runtime->m_impl->global_session->findProfile("spirv_1_3");

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

    slang::SessionDesc hlsl_session_desc = {};
    hlsl_session_desc.targets = &target_desc;
    hlsl_session_desc.targetCount = 1;
    hlsl_session_desc.compilerOptionEntries = options.data();
    hlsl_session_desc.compilerOptionEntryCount = options.size();

    if (SLANG_FAILED(runtime->m_impl->global_session->createSession(
            hlsl_session_desc, runtime->m_impl->hlsl_session.writeRef()))) {
        return make_result_ptr_error<ShaderRuntime>(ErrorCode::shader_compile_failed,
                                                    "Failed to create Slang HLSL session");
    }

    slang::SessionDesc glsl_session_desc = {};
    glsl_session_desc.targets = &target_desc;
    glsl_session_desc.targetCount = 1;
    glsl_session_desc.compilerOptionEntries = options.data();
    glsl_session_desc.compilerOptionEntryCount = options.size();
    glsl_session_desc.allowGLSLSyntax = true;

    if (SLANG_FAILED(runtime->m_impl->global_session->createSession(
            glsl_session_desc, runtime->m_impl->glsl_session.writeRef()))) {
        return make_result_ptr_error<ShaderRuntime>(ErrorCode::shader_compile_failed,
                                                    "Failed to create Slang GLSL session");
    }

    auto cache_dir = runtime->get_cache_dir();
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) {
        GOGGLES_LOG_WARN("Failed to create shader cache directory: {}", ec.message());
    }

    GOGGLES_LOG_INFO("ShaderRuntime initialized (dual session: HLSL + GLSL), cache: {}",
                     cache_dir.string());
    return make_result_ptr(std::move(runtime));
}

void ShaderRuntime::shutdown() {
    m_impl->glsl_session = nullptr;
    m_impl->hlsl_session = nullptr;
    m_impl->global_session = nullptr;

    GOGGLES_LOG_DEBUG("ShaderRuntime shutdown");
}

auto ShaderRuntime::compile_shader(const std::filesystem::path& source_path,
                                   const std::string& entry_point) -> Result<CompiledShader> {
    GOGGLES_PROFILE_FUNCTION();

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

// Helpers for RetroArch caching
auto load_cached_retroarch(const std::filesystem::path& cache_path,
                           const std::string& expected_hash) -> Result<RetroArchCompiledShader> {
    auto file_data = util::read_file_binary(cache_path);
    if (!file_data) {
        return nonstd::make_unexpected(file_data.error());
    }

    BinaryReader reader(file_data->data(), file_data->size());

    // Read magic and version
    std::array<char, 4> magic;
    if (!reader.read(magic.data(), 4) ||
        std::string_view(magic.data(), 4) != RETROARCH_CACHE_MAGIC) {
        return make_error<RetroArchCompiledShader>(ErrorCode::parse_error, "Invalid cache magic");
    }

    uint32_t version;
    if (!reader.read_pod(version) || version != CACHE_VERSION) {
        return make_error<RetroArchCompiledShader>(ErrorCode::parse_error,
                                                   "Cache version mismatch");
    }

    // Check hash
    std::string stored_hash;
    if (!reader.read_str(stored_hash) || stored_hash != expected_hash) {
        return make_error<RetroArchCompiledShader>(ErrorCode::parse_error, "Source hash mismatch");
    }

    RetroArchCompiledShader result;

    if (!read_spirv(reader, result.vertex_spirv) ||
        !read_reflection(reader, result.vertex_reflection) ||
        !read_spirv(reader, result.fragment_spirv) ||
        !read_reflection(reader, result.fragment_reflection)) {
        return make_error<RetroArchCompiledShader>(ErrorCode::parse_error, "Failed to read data");
    }

    return result;
}

auto save_cached_retroarch(const std::filesystem::path& cache_path, const std::string& source_hash,
                           const RetroArchCompiledShader& shader) -> Result<void> {
    BinaryWriter writer;

    writer.write(RETROARCH_CACHE_MAGIC.data(), 4);
    writer.write_pod(CACHE_VERSION);
    GOGGLES_TRY(writer.write_str(source_hash));

    GOGGLES_TRY(write_spirv(writer, shader.vertex_spirv));
    GOGGLES_TRY(write_reflection(writer, shader.vertex_reflection));
    GOGGLES_TRY(write_spirv(writer, shader.fragment_spirv));
    GOGGLES_TRY(write_reflection(writer, shader.fragment_reflection));

    std::ofstream file(cache_path, std::ios::binary);
    if (!file) {
        return make_error<void>(ErrorCode::file_write_failed,
                                "Failed to create cache file: " + cache_path.string());
    }

    file.write(writer.buffer.data(), static_cast<std::streamsize>(writer.buffer.size()));
    if (!file) {
        return make_error<void>(ErrorCode::file_write_failed, "Failed to write cache file");
    }

    return {};
}

auto ShaderRuntime::compile_slang(const std::string& module_name, const std::string& source,
                                  const std::string& entry_point) -> Result<std::vector<uint32_t>> {
    GOGGLES_PROFILE_SCOPE("CompileSlang");

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
    GOGGLES_PROFILE_SCOPE("CompileGlsl");

    auto result =
        GOGGLES_TRY(compile_glsl_with_reflection(module_name, source, entry_point, stage));
    return std::move(result.spirv);
}

auto ShaderRuntime::compile_glsl_with_reflection(const std::string& module_name,
                                                 const std::string& source,
                                                 const std::string& entry_point, ShaderStage stage)
    -> Result<GlslCompileResult> {
    GOGGLES_PROFILE_SCOPE("CompileGlslWithReflection");

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
    GOGGLES_PROFILE_FUNCTION();

    auto source_hash = compute_source_hash(vertex_source + fragment_source);
    auto cache_path = get_cache_dir() / (module_name + "_ra.cache");

    auto cached = load_cached_retroarch(cache_path, source_hash);
    if (cached) {
        GOGGLES_LOG_DEBUG("Loaded cached RetroArch shader: {}", cache_path.filename().string());
        return std::move(cached.value());
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

    RetroArchCompiledShader result{.vertex_spirv = std::move(vertex_result->spirv),
                                   .fragment_spirv = std::move(fragment_result->spirv),
                                   .vertex_reflection = std::move(vertex_result->reflection),
                                   .fragment_reflection = std::move(fragment_result->reflection)};

    // Save to cache
    auto save_result = save_cached_retroarch(cache_path, source_hash, result);
    if (!save_result) {
        GOGGLES_LOG_WARN("Failed to cache RetroArch shader: {}", save_result.error().message);
    }

    GOGGLES_LOG_INFO("Compiled RetroArch shader: {}", module_name);
    return result;
}

} // namespace goggles::render
