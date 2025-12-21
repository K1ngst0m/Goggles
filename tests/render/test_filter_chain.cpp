#include "render/chain/filter_chain.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace goggles::render;

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
