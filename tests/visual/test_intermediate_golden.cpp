#include "image_compare.hpp"
#include "runtime_capture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr double INTERMEDIATE_TOLERANCE = 0.05;

auto intermediate_golden_path(std::string_view preset_name, uint32_t pass_ordinal)
    -> std::filesystem::path {
    auto preferred = std::filesystem::path(GOLDEN_DIR) /
                     (std::string(preset_name) + "_pass" + std::to_string(pass_ordinal) + ".png");
    if (std::filesystem::exists(preferred)) {
        return preferred;
    }

    return std::filesystem::path(GOLDEN_DIR) /
           (std::string(preset_name) + "_pass" + std::to_string(pass_ordinal) + "_frame0.png");
}

} // namespace

TEST_CASE("intermediate goldens compare per pass when present", "[visual][diagnostics]") {
    const std::string preset_name = "runtime_format";
    const std::vector<uint32_t> pass_ordinals = {0U, 1U};
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";

    bool any_golden = false;
    for (const auto pass_ordinal : pass_ordinals) {
        const auto golden_path = intermediate_golden_path(preset_name, pass_ordinal);
        any_golden = any_golden || std::filesystem::exists(golden_path);
    }
    if (!any_golden) {
        SKIP("Intermediate goldens not found - generate files like tests/golden/" + preset_name +
             "_pass0.png");
    }

    goggles::test::RuntimeCapturePlan plan{};
    plan.preset_path = preset_path;
    plan.preset_name = preset_name;
    plan.frame_indices = {0U};
    plan.intermediate_pass_ordinals = pass_ordinals;

    auto capture = goggles::test::capture_runtime_outputs(plan);
    if (!capture) {
        SKIP(capture.error().message);
    }

    REQUIRE(capture->sink != nullptr);
    CHECK(capture->sink->events_by_category(goggles::diagnostics::Category::capture).size() >=
          pass_ordinals.size());

    std::unordered_map<uint32_t, goggles::test::CompareResult> comparisons;

    for (const auto pass_ordinal : pass_ordinals) {
        const auto key = goggles::test::pass_frame_key(pass_ordinal, 0U);
        const auto golden_path = intermediate_golden_path(preset_name, pass_ordinal);
        if (!std::filesystem::exists(golden_path)) {
            SKIP("Missing intermediate golden for pass " + std::to_string(pass_ordinal));
        }

        INFO("Pass ordinal: " << pass_ordinal);
        const auto actual = goggles::test::load_png(capture->intermediate_frames.at(key));
        REQUIRE(actual);
        const auto golden = goggles::test::load_png(golden_path);
        REQUIRE(golden);
        const auto heatmap_path = std::filesystem::path("intermediate_heatmap_pass" +
                                                        std::to_string(pass_ordinal) + ".png");
        const auto comparison =
            goggles::test::compare_images(*actual, *golden, INTERMEDIATE_TOLERANCE, heatmap_path);
        comparisons.emplace(pass_ordinal, comparison);
        CHECK(comparison.passed);
    }

    const auto localization =
        goggles::test::localize_earliest_divergence(pass_ordinals, comparisons);
    INFO(localization.summary);
}
