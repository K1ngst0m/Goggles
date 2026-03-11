#include "image_compare.hpp"
#include "runtime_capture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr double TEMPORAL_TOLERANCE = 0.05;

} // namespace

TEST_CASE("temporal golden infrastructure captures requested frames", "[visual][diagnostics]") {
    const std::string preset_name = "runtime_history";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/history.slangp";
    const std::vector<uint32_t> frame_indices = {1U, 3U};
    const std::vector<uint32_t> pass_ordinals = {0U};

    bool any_golden = false;
    for (const auto frame_index : frame_indices) {
        const auto final_golden = std::filesystem::path(GOLDEN_DIR) /
                                  (preset_name + "_frame" + std::to_string(frame_index) + ".png");
        any_golden = any_golden || std::filesystem::exists(final_golden);
    }
    if (!any_golden) {
        SKIP("Temporal goldens not found - generate files like tests/golden/" + preset_name +
             "_frame1.png");
    }

    goggles::test::RuntimeCapturePlan plan{};
    plan.preset_path = preset_path;
    plan.preset_name = preset_name;
    plan.frame_indices = frame_indices;
    plan.intermediate_pass_ordinals = pass_ordinals;

    auto capture = goggles::test::capture_runtime_outputs(plan);
    if (!capture) {
        SKIP(capture.error().message);
    }

    REQUIRE(capture->sink != nullptr);
    CHECK(capture->sink->events_by_category(goggles::diagnostics::Category::capture).size() >=
          frame_indices.size());

    for (const auto frame_index : frame_indices) {
        const auto final_golden = std::filesystem::path(GOLDEN_DIR) /
                                  (preset_name + "_frame" + std::to_string(frame_index) + ".png");
        if (!std::filesystem::exists(final_golden)) {
            SKIP("Missing temporal final golden for frame " + std::to_string(frame_index));
        }

        const auto actual_final = goggles::test::load_png(capture->final_frames.at(frame_index));
        REQUIRE(actual_final);
        const auto golden_final = goggles::test::load_png(final_golden);
        REQUIRE(golden_final);
        const auto final_comparison =
            goggles::test::compare_images(*actual_final, *golden_final, TEMPORAL_TOLERANCE);
        CHECK(final_comparison.passed);

        std::unordered_map<uint32_t, goggles::test::CompareResult> comparisons;

        for (const auto pass_ordinal : pass_ordinals) {
            const auto intermediate_golden = std::filesystem::path(GOLDEN_DIR) /
                                             (preset_name + "_pass" + std::to_string(pass_ordinal) +
                                              "_frame" + std::to_string(frame_index) + ".png");
            if (!std::filesystem::exists(intermediate_golden)) {
                SKIP("Missing temporal intermediate golden for pass " +
                     std::to_string(pass_ordinal) + " frame " + std::to_string(frame_index));
            }

            const auto key = goggles::test::pass_frame_key(pass_ordinal, frame_index);
            const auto actual_intermediate =
                goggles::test::load_png(capture->intermediate_frames.at(key));
            REQUIRE(actual_intermediate);
            const auto golden_intermediate = goggles::test::load_png(intermediate_golden);
            REQUIRE(golden_intermediate);
            const auto intermediate_comparison = goggles::test::compare_images(
                *actual_intermediate, *golden_intermediate, TEMPORAL_TOLERANCE);
            comparisons.emplace(pass_ordinal, intermediate_comparison);
            CHECK(intermediate_comparison.passed);
        }

        const auto localization =
            goggles::test::localize_earliest_divergence(pass_ordinals, comparisons);
        INFO("Frame " << frame_index << ": " << localization.summary);
    }
}
