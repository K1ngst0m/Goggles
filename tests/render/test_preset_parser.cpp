#include "render/chain/preset_parser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <fstream>

using namespace goggles::render;

TEST_CASE("Preset Parser basic parsing", "[preset]") {
    PresetParser parser;

    SECTION("Parse minimal preset string") {
        // Create a temporary preset content
        std::string preset_content = R"(
shaders = 1

shader0 = shaders/test.slang
filter_linear0 = true
scale_type0 = viewport
)";

        // Write to a temp file and test
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());

        REQUIRE(result->passes.size() == 1);
        REQUIRE(result->passes[0].shader_path.filename() == "test.slang");
        REQUIRE(result->passes[0].filter_mode == FilterMode::linear);
        REQUIRE(result->passes[0].scale_type_x == ScaleType::viewport);
        REQUIRE(result->passes[0].scale_type_y == ScaleType::viewport);

        std::filesystem::remove_all(temp_dir);
    }

    SECTION("Parse multi-pass preset") {
        std::string preset_content = R"(
shaders = 2

shader0 = pass1.slang
scale_type0 = source
scale0 = 2.0
filter_linear0 = false

shader1 = pass2.slang
scale_type1 = viewport
filter_linear1 = true
float_framebuffer1 = true
)";

        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "multipass.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());

        REQUIRE(result->passes.size() == 2);

        // Pass 0
        REQUIRE(result->passes[0].shader_path.filename() == "pass1.slang");
        REQUIRE(result->passes[0].scale_type_x == ScaleType::source);
        REQUIRE(result->passes[0].scale_x == 2.0F);
        REQUIRE(result->passes[0].filter_mode == FilterMode::nearest);

        // Pass 1
        REQUIRE(result->passes[1].shader_path.filename() == "pass2.slang");
        REQUIRE(result->passes[1].scale_type_x == ScaleType::viewport);
        REQUIRE(result->passes[1].filter_mode == FilterMode::linear);
        REQUIRE(result->passes[1].framebuffer_format == vk::Format::eR16G16B16A16Sfloat);

        std::filesystem::remove_all(temp_dir);
    }

    SECTION("Parse sRGB framebuffer") {
        std::string preset_content = R"(
shaders = 1
shader0 = test.slang
srgb_framebuffer0 = true
)";

        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "srgb.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->passes[0].framebuffer_format == vk::Format::eR8G8B8A8Srgb);

        std::filesystem::remove_all(temp_dir);
    }
}

TEST_CASE("Preset Parser zfast-crt-composite", "[preset][integration]") {
    PresetParser parser;

    auto preset_path = std::filesystem::path(
        "/home/kingstom/workspaces/goggles/research/slang-shaders/crt/zfast-crt-composite.slangp");

    if (!std::filesystem::exists(preset_path)) {
        SKIP("zfast-crt-composite.slangp not found in research directory");
    }

    auto result = parser.load(preset_path);
    REQUIRE(result.has_value());

    // Verify expected structure for zfast-crt-composite
    REQUIRE(result->passes.size() == 1);
    REQUIRE(result->passes[0].shader_path.filename() == "zfast_crt_composite.slang");
    REQUIRE(result->passes[0].filter_mode == FilterMode::linear);
    REQUIRE(result->passes[0].scale_type_x == ScaleType::viewport);
}

TEST_CASE("Preset Parser texture wrap_mode parsing", "[preset][texture]") {
    PresetParser parser;

    SECTION("Parse wrap_mode clamp_to_border (default)") {
        std::string preset_content = R"(
shaders = 1
shader0 = test.slang

textures = lut
lut = textures/lut.png
)";
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "wrap_test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->textures.size() == 1);
        REQUIRE(result->textures[0].wrap_mode == WrapMode::clamp_to_border);

        std::filesystem::remove_all(temp_dir);
    }

    SECTION("Parse wrap_mode clamp_to_edge") {
        std::string preset_content = R"(
shaders = 1
shader0 = test.slang

textures = lut
lut = textures/lut.png
lut_wrap_mode = clamp_to_edge
)";
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "wrap_test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->textures.size() == 1);
        REQUIRE(result->textures[0].wrap_mode == WrapMode::clamp_to_edge);

        std::filesystem::remove_all(temp_dir);
    }

    SECTION("Parse wrap_mode repeat") {
        std::string preset_content = R"(
shaders = 1
shader0 = test.slang

textures = lut
lut = textures/lut.png
lut_wrap_mode = repeat
)";
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "wrap_test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->textures.size() == 1);
        REQUIRE(result->textures[0].wrap_mode == WrapMode::repeat);

        std::filesystem::remove_all(temp_dir);
    }

    SECTION("Parse wrap_mode mirrored_repeat") {
        std::string preset_content = R"(
shaders = 1
shader0 = test.slang

textures = lut
lut = textures/lut.png
lut_wrap_mode = mirrored_repeat
)";
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "wrap_test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->textures.size() == 1);
        REQUIRE(result->textures[0].wrap_mode == WrapMode::mirrored_repeat);

        std::filesystem::remove_all(temp_dir);
    }

    SECTION("Parse multiple textures with different wrap_modes") {
        std::string preset_content = R"(
shaders = 1
shader0 = test.slang

textures = "lut1;lut2"
lut1 = textures/lut1.png
lut1_wrap_mode = repeat
lut2 = textures/lut2.png
lut2_wrap_mode = clamp_to_edge
)";
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "wrap_test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->textures.size() == 2);
        REQUIRE(result->textures[0].name == "lut1");
        REQUIRE(result->textures[0].wrap_mode == WrapMode::repeat);
        REQUIRE(result->textures[1].name == "lut2");
        REQUIRE(result->textures[1].wrap_mode == WrapMode::clamp_to_edge);

        std::filesystem::remove_all(temp_dir);
    }
}

TEST_CASE("Preset Parser pass alias parsing", "[preset][alias]") {
    PresetParser parser;

    SECTION("Parse alias for shader pass") {
        std::string preset_content = R"(
shaders = 2
shader0 = pass0.slang
alias0 = BLOOM_PASS
shader1 = pass1.slang
)";
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "alias_test.slangp";

        {
            std::ofstream file(preset_path);
            file << preset_content;
        }

        auto result = parser.load(preset_path);
        REQUIRE(result.has_value());
        REQUIRE(result->passes.size() == 2);
        REQUIRE(result->passes[0].alias.has_value());
        REQUIRE(result->passes[0].alias.value() == "BLOOM_PASS");
        REQUIRE_FALSE(result->passes[1].alias.has_value());

        std::filesystem::remove_all(temp_dir);
    }
}

TEST_CASE("Preset Parser error handling", "[preset][error]") {
    PresetParser parser;

    SECTION("Missing file") {
        auto result = parser.load("/nonexistent/path.slangp");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::file_not_found);
    }

    SECTION("Missing shaders count") {
        auto temp_dir = std::filesystem::temp_directory_path() / "goggles_test";
        std::filesystem::create_directories(temp_dir);
        auto preset_path = temp_dir / "invalid.slangp";

        {
            std::ofstream file(preset_path);
            file << "shader0 = test.slang\n"; // Missing shaders = X
        }

        auto result = parser.load(preset_path);
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::parse_error);

        std::filesystem::remove_all(temp_dir);
    }
}
