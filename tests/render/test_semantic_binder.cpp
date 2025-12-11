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
    std::array<float, 16> custom_mvp = {
        2.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 2.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
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
