#include "render/chain/filter_pass.hpp"
#include "render/chain/preset_parser.hpp"
#include "render/shader/retroarch_preprocessor.hpp"
#include "render/shader/shader_runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <fstream>

using namespace goggles::render;

// Path to zfast-crt preset (relative to project root, set via CMake)
static const std::filesystem::path ZFAST_CRT_PRESET =
    std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/crt/zfast-crt.slangp";

TEST_CASE("zfast-crt integration - preset loading", "[integration][zfast]") {
    // Skip if shader files not available
    if (!std::filesystem::exists(ZFAST_CRT_PRESET)) {
        SKIP("zfast-crt.slangp not found in shaders/retroarch/crt/");
    }

    PresetParser parser;
    auto preset_result = parser.load(ZFAST_CRT_PRESET);
    REQUIRE(preset_result.has_value());

    auto& preset = preset_result.value();
    REQUIRE(preset.passes.size() == 1);
    REQUIRE(preset.passes[0].filter_mode == FilterMode::LINEAR);
    REQUIRE(preset.passes[0].scale_type_x == ScaleType::VIEWPORT);
    REQUIRE(preset.passes[0].shader_path.filename() == "zfast_crt_finemask.slang");
}

TEST_CASE("zfast-crt integration - preprocessing", "[integration][zfast]") {
    if (!std::filesystem::exists(ZFAST_CRT_PRESET)) {
        SKIP("zfast-crt.slangp not found in shaders/retroarch/crt/");
    }

    // Load preset to get shader path
    PresetParser parser;
    auto preset_result = parser.load(ZFAST_CRT_PRESET);
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

    // Verify parameters extracted (6 params: BLURSCALEX, LOWLUMSCAN, HILUMSCAN, BRIGHTBOOST,
    // MASK_DARK, MASK_FADE)
    REQUIRE(preprocessed.parameters.size() == 6);

    // Check specific known parameters
    bool found_blurscalex = false;
    bool found_mask_dark = false;
    for (const auto& param : preprocessed.parameters) {
        if (param.name == "BLURSCALEX") {
            found_blurscalex = true;
            REQUIRE(param.default_value == 0.30F);
        }
        if (param.name == "MASK_DARK") {
            found_mask_dark = true;
            REQUIRE(param.default_value == 0.25F);
        }
    }
    REQUIRE(found_blurscalex);
    REQUIRE(found_mask_dark);
}

TEST_CASE("zfast-crt integration - compilation", "[integration][zfast]") {
    if (!std::filesystem::exists(ZFAST_CRT_PRESET)) {
        SKIP("zfast-crt.slangp not found in shaders/retroarch/crt/");
    }

    // Load preset
    PresetParser parser;
    auto preset_result = parser.load(ZFAST_CRT_PRESET);
    REQUIRE(preset_result.has_value());

    auto shader_path = preset_result->passes[0].shader_path;

    // Preprocess
    RetroArchPreprocessor preprocessor;
    auto preprocess_result = preprocessor.preprocess(shader_path);
    REQUIRE(preprocess_result.has_value());

    // Compile
    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    auto compile_result = runtime.compile_retroarch_shader(
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
    if (!std::filesystem::exists(ZFAST_CRT_PRESET)) {
        SKIP("zfast-crt.slangp not found in shaders/retroarch/crt/");
    }

    // This test verifies the complete pipeline from preset to compiled shader
    // without requiring a Vulkan device (so we can't create FilterPass)

    PresetParser parser;
    auto preset_result = parser.load(ZFAST_CRT_PRESET);
    REQUIRE(preset_result.has_value());

    RetroArchPreprocessor preprocessor;
    auto preprocess_result = preprocessor.preprocess(preset_result->passes[0].shader_path);
    REQUIRE(preprocess_result.has_value());

    ShaderRuntime runtime;
    REQUIRE(runtime.init().has_value());

    auto compile_result = runtime.compile_retroarch_shader(
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
