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
        REQUIRE(result->passes[0].filter_mode == FilterMode::LINEAR);
        REQUIRE(result->passes[0].scale_type_x == ScaleType::VIEWPORT);
        REQUIRE(result->passes[0].scale_type_y == ScaleType::VIEWPORT);

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
        REQUIRE(result->passes[0].scale_type_x == ScaleType::SOURCE);
        REQUIRE(result->passes[0].scale_x == 2.0F);
        REQUIRE(result->passes[0].filter_mode == FilterMode::NEAREST);

        // Pass 1
        REQUIRE(result->passes[1].shader_path.filename() == "pass2.slang");
        REQUIRE(result->passes[1].scale_type_x == ScaleType::VIEWPORT);
        REQUIRE(result->passes[1].filter_mode == FilterMode::LINEAR);
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
    REQUIRE(result->passes[0].filter_mode == FilterMode::LINEAR);
    REQUIRE(result->passes[0].scale_type_x == ScaleType::VIEWPORT);
}

TEST_CASE("Preset Parser error handling", "[preset][error]") {
    PresetParser parser;

    SECTION("Missing file") {
        auto result = parser.load("/nonexistent/path.slangp");
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::FILE_NOT_FOUND);
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
        REQUIRE(result.error().code == goggles::ErrorCode::PARSE_ERROR);

        std::filesystem::remove_all(temp_dir);
    }
}
