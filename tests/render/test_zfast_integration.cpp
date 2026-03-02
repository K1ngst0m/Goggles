#include "render/chain/filter_pass.hpp"
#include "render/chain/preset_parser.hpp"
#include "render/shader/retroarch_preprocessor.hpp"
#include "render/shader/shader_runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace goggles::render;

namespace {
auto test_cache_dir() -> std::filesystem::path {
    auto dir = std::filesystem::temp_directory_path() / "goggles_test_cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}
} // namespace

// Path to zfast-crt preset (relative to project root, set via CMake)
static const std::filesystem::path zfast_crt_preset =
    std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/crt/crt-lottes-fast.slangp";

TEST_CASE("zfast-crt integration - preset loading", "[integration][zfast]") {
    // Skip if shader files not available
    if (!std::filesystem::exists(zfast_crt_preset)) {
        SKIP("crt-lottes-fast.slangp not found in shaders/retroarch/crt/");
    }

    PresetParser parser;
    auto preset_result = parser.load(zfast_crt_preset);
    REQUIRE(preset_result.has_value());

    auto& preset = preset_result.value();
    REQUIRE(preset.passes.size() == 1);
    REQUIRE(preset.passes[0].filter_mode == FilterMode::linear);
    REQUIRE(preset.passes[0].scale_type_x == ScaleType::viewport);
    REQUIRE(preset.passes[0].shader_path.filename() == "crt-lottes-fast.slang");
}

TEST_CASE("zfast-crt integration - preprocessing", "[integration][zfast]") {
    if (!std::filesystem::exists(zfast_crt_preset)) {
        SKIP("crt-lottes-fast.slangp not found in shaders/retroarch/crt/");
    }

    // Load preset to get shader path
    PresetParser parser;
    auto preset_result = parser.load(zfast_crt_preset);
    REQUIRE(preset_result.has_value());

    auto shader_path = preset_result->passes[0].shader_path;
    REQUIRE(std::filesystem::exists(shader_path));

    // Preprocess shader
    RetroArchPreprocessor preprocessor;
    auto preprocess_result = preprocessor.preprocess(shader_path);
    REQUIRE(preprocess_result.has_value());

    auto& preprocessed = preprocess_result.value();

    // Verify stage splitting
    REQUIRE(!preprocessed.vertex_source.empty());
    REQUIRE(!preprocessed.fragment_source.empty());
    REQUIRE(preprocessed.vertex_source.find("#version 450") != std::string::npos);
    REQUIRE(preprocessed.fragment_source.find("#version 450") != std::string::npos);

    REQUIRE(preprocessed.parameters.size() == 8);

    // Check specific known parameters
    bool found_scan_blur = false;
    bool found_curvature = false;
    for (const auto& param : preprocessed.parameters) {
        if (param.name == "SCAN_BLUR") {
            found_scan_blur = true;
            REQUIRE(param.default_value == 2.5F);
        }
        if (param.name == "CURVATURE") {
            found_curvature = true;
            REQUIRE(param.default_value == 0.02F);
        }
    }
    REQUIRE(found_scan_blur);
    REQUIRE(found_curvature);
}

TEST_CASE("zfast-crt integration - compilation", "[integration][zfast]") {
    if (!std::filesystem::exists(zfast_crt_preset)) {
        SKIP("crt-lottes-fast.slangp not found in shaders/retroarch/crt/");
    }

    // Load preset
    PresetParser parser;
    auto preset_result = parser.load(zfast_crt_preset);
    REQUIRE(preset_result.has_value());

    auto shader_path = preset_result->passes[0].shader_path;

    // Preprocess
    RetroArchPreprocessor preprocessor;
    auto preprocess_result = preprocessor.preprocess(shader_path);
    REQUIRE(preprocess_result.has_value());

    // Compile
    auto runtime = ShaderRuntime::create(test_cache_dir());
    REQUIRE(runtime.has_value());

    auto compile_result = runtime.value()->compile_retroarch_shader(
        preprocess_result->vertex_source, preprocess_result->fragment_source, "zfast_crt");

    REQUIRE(compile_result.has_value());

    // Verify SPIR-V generated
    REQUIRE(!compile_result->vertex_spirv.empty());
    REQUIRE(!compile_result->fragment_spirv.empty());

    // Verify reflection data
    // Vertex shader should have push constants (for SourceSize, etc.)
    REQUIRE(compile_result->vertex_reflection.push_constants.has_value());

    // Fragment shader should have push constants and Source texture
    REQUIRE(compile_result->fragment_reflection.push_constants.has_value());
    REQUIRE(!compile_result->fragment_reflection.textures.empty());

    // Find Source texture binding
    bool found_source = false;
    for (const auto& tex : compile_result->fragment_reflection.textures) {
        if (tex.name == "Source") {
            found_source = true;
            // zfast-crt uses binding 2 for Source
            REQUIRE(tex.binding == 2);
        }
    }
    REQUIRE(found_source);
}

TEST_CASE("zfast-crt integration - full pipeline", "[integration][zfast]") {
    if (!std::filesystem::exists(zfast_crt_preset)) {
        SKIP("crt-lottes-fast.slangp not found in shaders/retroarch/crt/");
    }

    // This test verifies the complete pipeline from preset to compiled shader
    // without requiring a Vulkan device (so we can't create FilterPass)

    PresetParser parser;
    auto preset_result = parser.load(zfast_crt_preset);
    REQUIRE(preset_result.has_value());

    RetroArchPreprocessor preprocessor;
    auto preprocess_result = preprocessor.preprocess(preset_result->passes[0].shader_path);
    REQUIRE(preprocess_result.has_value());

    auto runtime = ShaderRuntime::create(test_cache_dir());
    REQUIRE(runtime.has_value());

    auto compile_result = runtime.value()->compile_retroarch_shader(
        preprocess_result->vertex_source, preprocess_result->fragment_source, "zfast_crt");
    REQUIRE(compile_result.has_value());

    // Log success metrics
    INFO("zfast-crt compiled successfully:");
    INFO("  Vertex SPIR-V size: " << compile_result->vertex_spirv.size() << " words");
    INFO("  Fragment SPIR-V size: " << compile_result->fragment_spirv.size() << " words");
    INFO("  Parameters extracted: " << preprocess_result->parameters.size());
    INFO("  Textures bound: " << compile_result->fragment_reflection.textures.size());

    SUCCEED("Full pipeline verification complete");
}
