#include "render/chain/semantic_binder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace goggles::render;
using Catch::Matchers::WithinAbs;

TEST_CASE("SizeVec4 computation", "[semantic]") {
    auto size = make_size_vec4(1920, 1080);

    REQUIRE_THAT(size.width, WithinAbs(1920.0F, 0.001F));
    REQUIRE_THAT(size.height, WithinAbs(1080.0F, 0.001F));
    REQUIRE_THAT(size.inv_width, WithinAbs(1.0F / 1920.0F, 0.0001F));
    REQUIRE_THAT(size.inv_height, WithinAbs(1.0F / 1080.0F, 0.0001F));
}

TEST_CASE("SemanticBinder UBO population", "[semantic]") {
    SemanticBinder binder;

    // Default MVP is identity
    auto ubo = binder.get_ubo();
    REQUIRE(ubo.mvp[0] == 1.0F);  // m[0][0]
    REQUIRE(ubo.mvp[5] == 1.0F);  // m[1][1]
    REQUIRE(ubo.mvp[10] == 1.0F); // m[2][2]
    REQUIRE(ubo.mvp[15] == 1.0F); // m[3][3]

    // Set custom MVP
    std::array<float, 16> custom_mvp = {2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F,
                                        0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    binder.set_mvp(custom_mvp);
    ubo = binder.get_ubo();
    REQUIRE(ubo.mvp[0] == 2.0F);
    REQUIRE(ubo.mvp[5] == 2.0F);
}

TEST_CASE("SemanticBinder push constant population", "[semantic]") {
    SemanticBinder binder;

    binder.set_source_size(256, 224);
    binder.set_output_size(1920, 1080);
    binder.set_original_size(256, 224);
    binder.set_frame_count(42);

    auto push = binder.get_push_constants();

    REQUIRE_THAT(push.source_size.width, WithinAbs(256.0F, 0.001F));
    REQUIRE_THAT(push.source_size.height, WithinAbs(224.0F, 0.001F));
    REQUIRE_THAT(push.output_size.width, WithinAbs(1920.0F, 0.001F));
    REQUIRE_THAT(push.output_size.height, WithinAbs(1080.0F, 0.001F));
    REQUIRE_THAT(push.original_size.width, WithinAbs(256.0F, 0.001F));
    REQUIRE(push.frame_count == 42);
}

TEST_CASE("RetroArchPushConstants size", "[semantic]") {
    // Verify the push constant struct fits within Vulkan's 128-byte limit
    REQUIRE(RetroArchPushConstants::size_bytes() <= 128);
}

TEST_CASE("SemanticBinder alias size tracking", "[semantic][alias]") {
    SemanticBinder binder;

    SECTION("Set and get alias size") {
        binder.set_alias_size("BLOOM_PASS", 1280, 720);

        auto result = binder.get_alias_size("BLOOM_PASS");
        REQUIRE(result.has_value());
        REQUIRE_THAT(result->width, WithinAbs(1280.0F, 0.001F));
        REQUIRE_THAT(result->height, WithinAbs(720.0F, 0.001F));
        REQUIRE_THAT(result->inv_width, WithinAbs(1.0F / 1280.0F, 0.0001F));
        REQUIRE_THAT(result->inv_height, WithinAbs(1.0F / 720.0F, 0.0001F));
    }

    SECTION("Get non-existent alias returns nullopt") {
        auto result = binder.get_alias_size("NON_EXISTENT");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Clear alias sizes") {
        binder.set_alias_size("PASS_A", 640, 480);
        binder.set_alias_size("PASS_B", 1920, 1080);

        REQUIRE(binder.get_alias_size("PASS_A").has_value());
        REQUIRE(binder.get_alias_size("PASS_B").has_value());

        binder.clear_alias_sizes();

        REQUIRE_FALSE(binder.get_alias_size("PASS_A").has_value());
        REQUIRE_FALSE(binder.get_alias_size("PASS_B").has_value());
    }

    SECTION("Override alias size") {
        binder.set_alias_size("PASS", 320, 240);
        binder.set_alias_size("PASS", 640, 480);

        auto result = binder.get_alias_size("PASS");
        REQUIRE(result.has_value());
        REQUIRE_THAT(result->width, WithinAbs(640.0F, 0.001F));
        REQUIRE_THAT(result->height, WithinAbs(480.0F, 0.001F));
    }

    SECTION("Multiple alias sizes") {
        binder.set_alias_size("LinearizePass", 1024, 768);
        binder.set_alias_size("VERTICAL_SCANLINES", 512, 384);
        binder.set_alias_size("BLOOM_APPROX", 256, 192);

        auto result1 = binder.get_alias_size("LinearizePass");
        REQUIRE(result1.has_value());
        REQUIRE_THAT(result1->width, WithinAbs(1024.0F, 0.001F));

        auto result2 = binder.get_alias_size("VERTICAL_SCANLINES");
        REQUIRE(result2.has_value());
        REQUIRE_THAT(result2->width, WithinAbs(512.0F, 0.001F));

        auto result3 = binder.get_alias_size("BLOOM_APPROX");
        REQUIRE(result3.has_value());
        REQUIRE_THAT(result3->width, WithinAbs(256.0F, 0.001F));
    }
}

TEST_CASE("SemanticBinder final viewport size", "[semantic]") {
    SemanticBinder binder;

    binder.set_final_viewport_size(3840, 2160);
    auto size = binder.final_viewport_size();

    REQUIRE_THAT(size.width, WithinAbs(3840.0F, 0.001F));
    REQUIRE_THAT(size.height, WithinAbs(2160.0F, 0.001F));
    REQUIRE_THAT(size.inv_width, WithinAbs(1.0F / 3840.0F, 0.0001F));
    REQUIRE_THAT(size.inv_height, WithinAbs(1.0F / 2160.0F, 0.0001F));
}
