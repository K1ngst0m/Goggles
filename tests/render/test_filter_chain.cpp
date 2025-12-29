#include "render/chain/filter_chain.hpp"

#include <catch2/catch_test_macros.hpp>
#include <charconv>
#include <optional>

using namespace goggles::render;

namespace {

auto parse_original_history_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "OriginalHistory";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    uint32_t index = 0;
    auto [ptr, ec] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
    if (ec != std::errc{} || ptr != suffix.data() + suffix.size()) {
        return std::nullopt;
    }
    return index;
}
}  // namespace

TEST_CASE("OriginalHistory sampler name parsing", "[filter_chain][history]") {
    SECTION("Valid OriginalHistory names") {
        auto idx0 = parse_original_history_index("OriginalHistory0");
        REQUIRE(idx0.has_value());
        REQUIRE(*idx0 == 0);

        auto idx3 = parse_original_history_index("OriginalHistory3");
        REQUIRE(idx3.has_value());
        REQUIRE(*idx3 == 3);

        auto idx6 = parse_original_history_index("OriginalHistory6");
        REQUIRE(idx6.has_value());
        REQUIRE(*idx6 == 6);
    }

    SECTION("Invalid OriginalHistory names") {
        REQUIRE(!parse_original_history_index("OriginalHistory").has_value());
        REQUIRE(!parse_original_history_index("OriginalHistoryX").has_value());
        REQUIRE(!parse_original_history_index("OriginalHistory-1").has_value());
        REQUIRE(!parse_original_history_index("Original").has_value());
        REQUIRE(!parse_original_history_index("Source").has_value());
        REQUIRE(!parse_original_history_index("PassOutput0").has_value());
    }

    SECTION("Multi-digit indices") {
        auto idx10 = parse_original_history_index("OriginalHistory10");
        REQUIRE(idx10.has_value());
        REQUIRE(*idx10 == 10);

        auto idx99 = parse_original_history_index("OriginalHistory99");
        REQUIRE(idx99.has_value());
        REQUIRE(*idx99 == 99);
    }
}

TEST_CASE("FilterChain size calculation", "[filter_chain]") {
    ShaderPassConfig config{};
    vk::Extent2D source{256, 224};
    vk::Extent2D viewport{1920, 1080};

    SECTION("SOURCE scale type multiplies source size") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::source;
        config.scale_x = 2.0F;
        config.scale_y = 2.0F;

        auto result = FilterChain::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 512);
        REQUIRE(result.height == 448);
    }

    SECTION("VIEWPORT scale type multiplies viewport size") {
        config.scale_type_x = ScaleType::viewport;
        config.scale_type_y = ScaleType::viewport;
        config.scale_x = 0.5F;
        config.scale_y = 0.5F;

        auto result = FilterChain::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 960);
        REQUIRE(result.height == 540);
    }

    SECTION("ABSOLUTE scale type uses pixel dimensions") {
        config.scale_type_x = ScaleType::absolute;
        config.scale_type_y = ScaleType::absolute;
        config.scale_x = 640.0F;
        config.scale_y = 480.0F;

        auto result = FilterChain::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 640);
        REQUIRE(result.height == 480);
    }

    SECTION("Mixed scale types work independently") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::viewport;
        config.scale_x = 4.0F;
        config.scale_y = 1.0F;

        auto result = FilterChain::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 1024);
        REQUIRE(result.height == 1080);
    }

    SECTION("Minimum size is 1x1") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::source;
        config.scale_x = 0.0F;
        config.scale_y = 0.0F;

        auto result = FilterChain::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 1);
        REQUIRE(result.height == 1);
    }

    SECTION("Fractional scaling rounds correctly") {
        config.scale_type_x = ScaleType::source;
        config.scale_type_y = ScaleType::source;
        config.scale_x = 1.5F;
        config.scale_y = 1.5F;

        auto result = FilterChain::calculate_pass_output_size(config, source, viewport);
        REQUIRE(result.width == 384);
        REQUIRE(result.height == 336);
    }
}
