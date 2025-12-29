#include "render/chain/preset_parser.hpp"
#include "render/shader/retroarch_preprocessor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace goggles::render;

namespace {
auto get_shader_dir() -> std::filesystem::path {
    return std::filesystem::path("shaders/retroarch");
}
}  // namespace

TEST_CASE("Shader validation - CRT presets parse", "[shader][validation][crt]") {
    PresetParser parser;
    auto shader_dir = get_shader_dir();
    auto crt_dir = shader_dir / "crt";

    if (!std::filesystem::exists(crt_dir)) {
        SKIP("CRT shader directory not found");
    }

    size_t tested = 0;
    size_t passed = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(crt_dir)) {
        if (entry.path().extension() != ".slangp") continue;

        tested++;
        auto result = parser.load(entry.path());
        if (result.has_value()) {
            passed++;
        } else {
            WARN("Failed: " << entry.path().filename().string() << " - " << result.error().message);
        }
    }

    INFO("CRT presets: " << passed << "/" << tested << " parsed successfully");
    REQUIRE(passed > 0);
}

TEST_CASE("Shader validation - CRT shaders compile", "[shader][validation][crt][compile]") {
    PresetParser parser;
    RetroArchPreprocessor preprocessor;
    auto shader_dir = get_shader_dir();

    std::vector<std::filesystem::path> test_presets = {
        shader_dir / "crt/crt-lottes.slangp",
        shader_dir / "crt/crt-geom.slangp",
        shader_dir / "crt/zfast-crt.slangp",
    };

    for (const auto& preset_path : test_presets) {
        if (!std::filesystem::exists(preset_path)) continue;

        DYNAMIC_SECTION("Compile " << preset_path.filename().string()) {
            auto preset = parser.load(preset_path);
            REQUIRE(preset.has_value());

            for (const auto& pass : preset->passes) {
                auto result = preprocessor.preprocess(pass.shader_path);
                if (!result.has_value()) {
                    FAIL("Preprocess failed: " << pass.shader_path.filename().string()
                         << " - " << result.error().message);
                }
                REQUIRE(!result->vertex_source.empty());
                REQUIRE(!result->fragment_source.empty());
            }
        }
    }
}