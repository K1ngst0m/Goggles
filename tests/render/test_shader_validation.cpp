#include "render/chain/preset_parser.hpp"
#include "render/shader/retroarch_preprocessor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <vector>

using namespace goggles::render;

namespace {

auto get_shader_dir() -> std::filesystem::path {
    return "shaders/retroarch";
}

auto discover_presets(const std::filesystem::path& dir) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> presets;
    if (!std::filesystem::exists(dir))
        return presets;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.path().extension() == ".slangp") {
            presets.push_back(entry.path());
        }
    }
    std::sort(presets.begin(), presets.end());
    return presets;
}

auto discover_categories(const std::filesystem::path& shader_dir) -> std::vector<std::string> {
    std::vector<std::string> cats;
    if (!std::filesystem::exists(shader_dir))
        return cats;
    for (const auto& entry : std::filesystem::directory_iterator(shader_dir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name != "include" && name != "spec" && name != "test") {
                cats.push_back(name);
            }
        }
    }
    std::sort(cats.begin(), cats.end());
    return cats;
}

struct TestResult {
    std::filesystem::path path;
    bool parse_ok = false;
    bool compile_ok = false;
    std::string error;
};

auto test_preset(const std::filesystem::path& preset_path) -> TestResult {
    TestResult result{.path = preset_path};
    PresetParser parser;
    RetroArchPreprocessor preprocessor;

    auto preset = parser.load(preset_path);
    if (!preset) {
        result.error = "Parse: " + preset.error().message;
        return result;
    }
    result.parse_ok = true;

    for (const auto& pass : preset->passes) {
        auto compiled = preprocessor.preprocess(pass.shader_path);
        if (!compiled) {
            result.error = pass.shader_path.filename().string() + ": " + compiled.error().message;
            return result;
        }
    }
    result.compile_ok = true;
    return result;
}

} // namespace

TEST_CASE("Shader validation - all categories", "[shader][validation][batch]") {
    auto shader_dir = get_shader_dir();
    auto categories = discover_categories(shader_dir);

    if (categories.empty()) {
        SKIP("No shader categories found");
    }

    for (const auto& cat : categories) {
        DYNAMIC_SECTION(cat) {
            auto presets = discover_presets(shader_dir / cat);
            if (presets.empty()) {
                SKIP("No presets in " + cat);
            }

            size_t passed = 0;
            for (const auto& p : presets) {
                auto r = test_preset(p);
                if (r.compile_ok) {
                    passed++;
                } else {
                    UNSCOPED_INFO(p.filename().string() << ": " << r.error);
                }
            }
            INFO(cat << ": " << passed << "/" << presets.size() << " passed");
            CHECK(passed > 0);
        }
    }
}