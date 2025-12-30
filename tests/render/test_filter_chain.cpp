#include "render/chain/filter_chain.hpp"
#include "render/chain/semantic_binder.hpp"

#include <catch2/catch_approx.hpp>
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

auto parse_pass_output_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "PassOutput";
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

auto parse_pass_feedback_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "PassFeedback";
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
} // namespace

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

TEST_CASE("PassOutput# sampler name parsing", "[filter_chain][shader_spec]") {
    SECTION("Valid PassOutput names") {
        REQUIRE(parse_pass_output_index("PassOutput0").value() == 0);
        REQUIRE(parse_pass_output_index("PassOutput1").value() == 1);
        REQUIRE(parse_pass_output_index("PassOutput12").value() == 12);
    }

    SECTION("Invalid PassOutput names") {
        REQUIRE(!parse_pass_output_index("PassOutput").has_value());
        REQUIRE(!parse_pass_output_index("PassOutputX").has_value());
        REQUIRE(!parse_pass_output_index("OriginalHistory0").has_value());
        REQUIRE(!parse_pass_output_index("Source").has_value());
    }
}

TEST_CASE("PassFeedback# sampler name parsing", "[filter_chain][shader_spec]") {
    SECTION("Valid PassFeedback names") {
        REQUIRE(parse_pass_feedback_index("PassFeedback0").value() == 0);
        REQUIRE(parse_pass_feedback_index("PassFeedback5").value() == 5);
        REQUIRE(parse_pass_feedback_index("PassFeedback13").value() == 13);
    }

    SECTION("Invalid PassFeedback names") {
        REQUIRE(!parse_pass_feedback_index("PassFeedback").has_value());
        REQUIRE(!parse_pass_feedback_index("PassOutput0").has_value());
        REQUIRE(!parse_pass_feedback_index("DerezedPassFeedback").has_value());
    }
}

TEST_CASE("SizeVec4 format per spec", "[filter_chain][shader_spec]") {
    SECTION("make_size_vec4 produces correct vec4") {
        auto size = make_size_vec4(1920, 1080);
        REQUIRE(size.width == 1920.0F);
        REQUIRE(size.height == 1080.0F);
        REQUIRE(size.inv_width == Catch::Approx(1.0F / 1920.0F));
        REQUIRE(size.inv_height == Catch::Approx(1.0F / 1080.0F));
    }

    SECTION("Size vec4 data() returns contiguous floats") {
        auto size = make_size_vec4(256, 224);
        const float* data = size.data();
        REQUIRE(data[0] == 256.0F);
        REQUIRE(data[1] == 224.0F);
        REQUIRE(data[2] == Catch::Approx(1.0F / 256.0F));
        REQUIRE(data[3] == Catch::Approx(1.0F / 224.0F));
    }
}

TEST_CASE("SemanticBinder alias sizes", "[filter_chain][shader_spec]") {
    SemanticBinder binder;

    SECTION("Alias sizes are stored and retrieved") {
        binder.set_alias_size("DerezedPass", 320, 240);
        auto size = binder.get_alias_size("DerezedPass");
        REQUIRE(size.has_value());
        REQUIRE(size->width == 320.0F);
        REQUIRE(size->height == 240.0F);
    }

    SECTION("Unknown alias returns nullopt") {
        REQUIRE(!binder.get_alias_size("NonExistent").has_value());
    }

    SECTION("clear_alias_sizes removes all aliases") {
        binder.set_alias_size("Pass0", 100, 100);
        binder.set_alias_size("Pass1", 200, 200);
        binder.clear_alias_sizes();
        REQUIRE(!binder.get_alias_size("Pass0").has_value());
        REQUIRE(!binder.get_alias_size("Pass1").has_value());
    }
}
