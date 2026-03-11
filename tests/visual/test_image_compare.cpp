#include "image_compare.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace {

struct Rgba {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

auto make_image(const int width, const int height, const Rgba color) -> goggles::test::Image {
    goggles::test::Image image;
    image.width = width;
    image.height = height;
    image.channels = 4;
    image.data.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);
    for (int pixel_index = 0; pixel_index < width * height; ++pixel_index) {
        const auto offset = static_cast<std::size_t>(pixel_index) * 4U;
        image.data[offset + 0U] = color.r;
        image.data[offset + 1U] = color.g;
        image.data[offset + 2U] = color.b;
        image.data[offset + 3U] = color.a;
    }
    return image;
}

} // namespace

TEST_CASE("identical images pass") {
    const auto actual = make_image(4, 4, {.r = 12U, .g = 34U, .b = 56U, .a = 255U});
    const auto reference = make_image(4, 4, {.r = 12U, .g = 34U, .b = 56U, .a = 255U});

    const auto result = goggles::test::compare_images(actual, reference, 0.0);
    CHECK(result.passed);
    CHECK(result.max_channel_diff == 0.0);
    CHECK(result.failing_pixels == 0U);
}

TEST_CASE("1-value diff fails at tolerance 0") {
    auto actual = make_image(4, 4, {.r = 0U, .g = 0U, .b = 0U, .a = 255U});
    auto reference = make_image(4, 4, {.r = 0U, .g = 0U, .b = 0U, .a = 255U});
    reference.data[0] = static_cast<std::uint8_t>(reference.data[0] + 1U);

    const auto result = goggles::test::compare_images(actual, reference, 0.0);
    CHECK_FALSE(result.passed);
    CHECK(result.failing_pixels >= 1U);
}

TEST_CASE("tolerance allows small diffs") {
    const auto actual = make_image(4, 4, {.r = 32U, .g = 64U, .b = 96U, .a = 128U});
    const auto reference = make_image(4, 4, {.r = 34U, .g = 66U, .b = 98U, .a = 130U});

    const auto result = goggles::test::compare_images(actual, reference, 2.0 / 255.0);
    CHECK(result.passed);
}

TEST_CASE("size mismatch fails") {
    const auto actual = make_image(4, 4, {.r = 1U, .g = 2U, .b = 3U, .a = 255U});
    const auto reference = make_image(8, 8, {.r = 1U, .g = 2U, .b = 3U, .a = 255U});

    const auto result = goggles::test::compare_images(actual, reference, 0.0);
    CHECK_FALSE(result.passed);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("diff image written on failure") {
    const auto actual = make_image(4, 4, {.r = 0U, .g = 0U, .b = 0U, .a = 255U});
    const auto reference = make_image(4, 4, {.r = 255U, .g = 255U, .b = 255U, .a = 255U});
    const std::filesystem::path diff_path = "test_diff_output.png";

    std::filesystem::remove(diff_path);
    const auto result = goggles::test::compare_images(actual, reference, 0.0, diff_path);

    CHECK_FALSE(result.passed);
    CHECK(std::filesystem::exists(diff_path));

    std::filesystem::remove(diff_path);
}

TEST_CASE("structural similarity for identical images is 1.0") {
    const auto actual = make_image(4, 4, {.r = 12U, .g = 34U, .b = 56U, .a = 255U});
    const auto reference = make_image(4, 4, {.r = 12U, .g = 34U, .b = 56U, .a = 255U});

    const auto result =
        goggles::test::compare_images(actual, reference, 0.0, std::filesystem::path{}, true);
    CHECK(result.structural_similarity == Catch::Approx(1.0));
}

TEST_CASE("structural similarity for opposite images is low") {
    const auto actual = make_image(4, 4, {.r = 0U, .g = 0U, .b = 0U, .a = 255U});
    const auto reference = make_image(4, 4, {.r = 255U, .g = 255U, .b = 255U, .a = 255U});

    const auto result =
        goggles::test::compare_images(actual, reference, 0.0, std::filesystem::path{}, true);
    CHECK(result.structural_similarity < 0.5);
}

TEST_CASE("roi comparison only counts pixels inside the region") {
    auto actual = make_image(4, 4, {.r = 0U, .g = 0U, .b = 0U, .a = 255U});
    auto reference = make_image(4, 4, {.r = 0U, .g = 0U, .b = 0U, .a = 255U});
    reference.data[0] = 255U;

    const goggles::test::Rect roi{.x = 2, .y = 2, .width = 2, .height = 2};
    const auto outside_result = goggles::test::compare_images(actual, reference, 0.0, roi);
    CHECK(outside_result.passed);
    CHECK(outside_result.failing_pixels == 0U);

    const auto inside_offset = (static_cast<std::size_t>(2) * 4U + 2U) * 4U;
    reference.data[inside_offset] = 255U;
    const auto inside_result = goggles::test::compare_images(actual, reference, 0.0, roi);
    CHECK_FALSE(inside_result.passed);
    CHECK(inside_result.failing_pixels == 1U);
}

TEST_CASE("diff heatmap written with expected dimensions") {
    const auto actual = make_image(4, 4, {.r = 0U, .g = 0U, .b = 255U, .a = 255U});
    const auto reference = make_image(4, 4, {.r = 255U, .g = 0U, .b = 0U, .a = 255U});
    const std::filesystem::path heatmap_path = "test_heatmap_output.png";

    std::filesystem::remove(heatmap_path);
    const auto write_result = goggles::test::generate_diff_heatmap(actual, reference, heatmap_path);
    REQUIRE(write_result);
    REQUIRE(std::filesystem::exists(heatmap_path));

    const auto heatmap_result = goggles::test::load_png(heatmap_path);
    REQUIRE(heatmap_result);
    CHECK(heatmap_result->width == actual.width);
    CHECK(heatmap_result->height == actual.height);

    std::filesystem::remove(heatmap_path);
}

TEST_CASE("earliest divergence localization reports first failing pass") {
    const std::vector<uint32_t> pass_ordinals = {0U, 1U, 2U, 3U, 4U};
    std::unordered_map<uint32_t, goggles::test::CompareResult> comparisons;
    auto passing = goggles::test::CompareResult{};
    passing.passed = true;
    auto failing = goggles::test::CompareResult{};
    comparisons.emplace(0U, passing);
    comparisons.emplace(1U, passing);
    comparisons.emplace(2U, failing);
    comparisons.emplace(3U, failing);
    comparisons.emplace(4U, failing);

    const auto localization =
        goggles::test::localize_earliest_divergence(pass_ordinals, comparisons);
    REQUIRE(localization.has_intermediate_goldens);
    REQUIRE(localization.earliest_pass.has_value());
    CHECK(*localization.earliest_pass == 2U);
    CHECK(localization.downstream_passes == std::vector<uint32_t>{3U, 4U});
    CHECK(localization.summary.find("Earliest divergent pass: 2") != std::string::npos);
}

TEST_CASE("earliest divergence localization falls back when no intermediates exist") {
    const auto localization = goggles::test::localize_earliest_divergence({}, {});
    CHECK_FALSE(localization.has_intermediate_goldens);
    CHECK_FALSE(localization.earliest_pass.has_value());
    CHECK(localization.summary.find("cannot be localized") != std::string::npos);
}
