#include "render/chain/preset_parser.hpp"
#include "render/shader/retroarch_preprocessor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace goggles::render;

namespace {

auto get_shader_dir() -> std::filesystem::path { return "shaders/retroarch"; }

auto discover_presets(const std::filesystem::path& dir) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> presets;
    if (!std::filesystem::exists(dir)) return presets;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.path().extension() == ".slangp") {
            presets.push_back(entry.path());
        }
    }
    std::sort(presets.begin(), presets.end());
    return presets;
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
            result.error = "Compile " + pass.shader_path.filename().string() + ": " +
                           compiled.error().message;
            return result;
        }
    }
    result.compile_ok = true;
    return result;
}

}  // namespace

TEST_CASE("Batch shader validation", "[shader][validation][batch]") {
    auto shader_dir = get_shader_dir();

    SECTION("CRT category") {
        auto presets = discover_presets(shader_dir / "crt");
        if (presets.empty()) SKIP("No CRT presets found");

        size_t passed = 0, failed = 0;
        for (const auto& p : presets) {
            auto r = test_preset(p);
            if (r.compile_ok) {
                passed++;
            } else {
                failed++;
                WARN(p.filename().string() << ": " << r.error);
            }
        }
        INFO("CRT: " << passed << "/" << presets.size() << " passed");
        CHECK(passed > 0);
    }

    SECTION("Handheld category") {
        auto presets = discover_presets(shader_dir / "handheld");
        if (presets.empty()) SKIP("No handheld presets found");

        size_t passed = 0;
        for (const auto& p : presets) {
            if (test_preset(p).compile_ok) passed++;
        }
        INFO("Handheld: " << passed << "/" << presets.size() << " passed");
        CHECK(passed > 0);
    }

    SECTION("Anti-aliasing category") {
        auto presets = discover_presets(shader_dir / "anti-aliasing");
        if (presets.empty()) SKIP("No anti-aliasing presets found");

        size_t passed = 0;
        for (const auto& p : presets) {
            if (test_preset(p).compile_ok) passed++;
        }
        INFO("Anti-aliasing: " << passed << "/" << presets.size() << " passed");
        CHECK(passed > 0);
    }
}

TEST_CASE("Full shader scan", "[shader][validation][full][!mayfail]") {
    auto shader_dir = get_shader_dir();
    auto all_presets = discover_presets(shader_dir);

    if (all_presets.empty()) SKIP("No presets found");

    size_t parse_ok = 0, compile_ok = 0;
    std::vector<TestResult> failures;

    for (const auto& p : all_presets) {
        auto r = test_preset(p);
        if (r.parse_ok) parse_ok++;
        if (r.compile_ok) {
            compile_ok++;
        } else {
            failures.push_back(r);
        }
    }

    INFO("Total: " << all_presets.size());
    INFO("Parse OK: " << parse_ok);
    INFO("Compile OK: " << compile_ok);

    if (!failures.empty() && failures.size() <= 20) {
        for (const auto& f : failures) {
            WARN(f.path.string() << ": " << f.error);
        }
    }
}