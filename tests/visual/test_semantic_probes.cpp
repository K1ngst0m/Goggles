#include "image_compare.hpp"
#include "runtime_capture.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace {

auto pixel_channel(const goggles::test::Image& image, int x, int y, int channel) -> std::uint8_t {
    const auto index = (y * image.width + x) * image.channels + channel;
    return image.data[static_cast<size_t>(index)];
}

} // namespace

TEST_CASE("semantic probe validates size semantics", "[visual][diagnostics][semantic_probe]") {
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) /
        "tests/util/test_data/filter_chain_diagnostics/semantic_probes/size_probe.slangp";
    auto capture = goggles::test::capture_runtime_outputs({
        .preset_path = preset_path,
        .preset_name = "size_probe",
        .frame_indices = {0U},
        .intermediate_pass_ordinals = {0U},
        .control_overrides = {},
        .source_extent = {32U, 48U},
        .target_extent = {64U, 40U},
    });
    if (!capture) {
        SKIP(capture.error().message);
    }

    const auto image = goggles::test::load_png(
        capture->intermediate_frames.at(goggles::test::pass_frame_key(0U, 0U)));
    REQUIRE(image);
    CHECK(pixel_channel(*image, 0, 0, 0) == 32U);
    CHECK(pixel_channel(*image, 0, 0, 1) == 48U);
    CHECK(pixel_channel(*image, 0, 0, 2) == 27U);
    CHECK(pixel_channel(*image, 0, 0, 3) == 40U);
}

TEST_CASE("semantic probe validates frame counter progression",
          "[visual][diagnostics][semantic_probe]") {
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) /
        "tests/util/test_data/filter_chain_diagnostics/semantic_probes/frame_counter_probe.slangp";
    goggles::test::RuntimeCapturePlan plan{};
    plan.preset_path = preset_path;
    plan.preset_name = "frame_counter_probe";
    plan.frame_indices = {0U, 1U, 2U, 3U};

    auto capture = goggles::test::capture_runtime_outputs(plan);
    if (!capture) {
        SKIP(capture.error().message);
    }

    for (uint32_t frame = 0; frame < 4; ++frame) {
        const auto image = goggles::test::load_png(capture->final_frames.at(frame));
        REQUIRE(image);
        const auto expected = static_cast<double>(frame) / 3.0 * 255.0;
        CHECK(pixel_channel(*image, 0, 0, 0) == Catch::Approx(expected).margin(1.0));
    }
}

TEST_CASE("semantic probe validates parameter isolation", "[visual][diagnostics][semantic_probe]") {
    const auto preset_path = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                             "tests/util/test_data/filter_chain_diagnostics/semantic_probes/"
                             "parameter_isolation_probe.slangp";
    goggles::test::RuntimeCapturePlan dim_plan{};
    dim_plan.preset_path = preset_path;
    dim_plan.preset_name = "parameter_isolation_dim";
    dim_plan.frame_indices = {0U};
    dim_plan.control_overrides = {{"BRIGHTNESS", 0.0F}};

    auto dim_capture = goggles::test::capture_runtime_outputs(dim_plan);
    if (!dim_capture) {
        SKIP(dim_capture.error().message);
    }

    goggles::test::RuntimeCapturePlan bright_plan{};
    bright_plan.preset_path = preset_path;
    bright_plan.preset_name = "parameter_isolation_bright";
    bright_plan.frame_indices = {0U};
    bright_plan.control_overrides = {{"BRIGHTNESS", 1.0F}};

    auto bright_capture = goggles::test::capture_runtime_outputs(bright_plan);
    if (!bright_capture) {
        SKIP(bright_capture.error().message);
    }

    const auto dim_image = goggles::test::load_png(dim_capture->final_frames.at(0U));
    const auto bright_image = goggles::test::load_png(bright_capture->final_frames.at(0U));
    REQUIRE(dim_image);
    REQUIRE(bright_image);

    const goggles::test::Rect left_half{
        .x = 0, .y = 0, .width = dim_image->width / 2, .height = dim_image->height};
    const goggles::test::Rect right_half{.x = dim_image->width / 2,
                                         .y = 0,
                                         .width = dim_image->width / 2,
                                         .height = dim_image->height};
    const auto changed = goggles::test::compare_images(*bright_image, *dim_image, 0.01, left_half);
    const auto unchanged =
        goggles::test::compare_images(*bright_image, *dim_image, 0.01, right_half);

    CHECK_FALSE(changed.passed);
    CHECK(changed.failing_pixels > 0U);
    CHECK(unchanged.passed);
}
