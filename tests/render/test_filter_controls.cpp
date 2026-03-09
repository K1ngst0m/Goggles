#include "render/chain/filter_controls.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace goggles::render;

TEST_CASE("Filter control stage domain", "[filter_chain][controls]") {
    REQUIRE(std::string_view{to_string(FilterControlStage::prechain)} == "prechain");
    REQUIRE(std::string_view{to_string(FilterControlStage::effect)} == "effect");
}

TEST_CASE("Filter control id semantics", "[filter_chain][controls]") {
    const auto effect_id = make_filter_control_id(FilterControlStage::effect, "SCAN_BLUR");
    const auto effect_id_repeat = make_filter_control_id(FilterControlStage::effect, "SCAN_BLUR");
    const auto prechain_id = make_filter_control_id(FilterControlStage::prechain, "SCAN_BLUR");

    REQUIRE(effect_id == effect_id_repeat);
    REQUIRE(effect_id != prechain_id);
}

TEST_CASE("Filter control ids are unique within a preset", "[filter_chain][controls]") {
    const std::vector<std::pair<FilterControlStage, std::string_view>> controls = {
        {FilterControlStage::prechain, "filter_type"},
        {FilterControlStage::effect, "SCAN_BLUR"},
        {FilterControlStage::effect, "MASK_DARK"},
        {FilterControlStage::effect, "CURVATURE"},
    };

    std::unordered_set<FilterControlId> ids;
    for (const auto& [stage, name] : controls) {
        REQUIRE(ids.insert(make_filter_control_id(stage, name)).second);
    }
}

TEST_CASE("Filter control set-value clamp behavior", "[filter_chain][controls]") {
    FilterControlDescriptor descriptor{};
    descriptor.min_value = 0.0F;
    descriptor.max_value = 1.0F;

    REQUIRE(clamp_filter_control_value(descriptor, -2.0F) == Catch::Approx(0.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 0.5F) == Catch::Approx(0.5F));
    REQUIRE(clamp_filter_control_value(descriptor, 5.0F) == Catch::Approx(1.0F));

    descriptor.min_value = 10.0F;
    descriptor.max_value = 5.0F;
    REQUIRE(clamp_filter_control_value(descriptor, 8.0F) == Catch::Approx(8.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 2.0F) == Catch::Approx(5.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 20.0F) == Catch::Approx(10.0F));
}

TEST_CASE("Pre-chain filter_type range remains backward-compatible", "[filter_chain][controls]") {
    FilterControlDescriptor descriptor{};
    descriptor.stage = FilterControlStage::prechain;
    descriptor.name = "filter_type";
    descriptor.default_value = 0.0F;
    descriptor.min_value = 0.0F;
    descriptor.max_value = 2.0F;
    descriptor.step = 1.0F;

    REQUIRE(clamp_filter_control_value(descriptor, -1.0F) == Catch::Approx(0.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 0.0F) == Catch::Approx(0.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 1.0F) == Catch::Approx(1.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 2.0F) == Catch::Approx(2.0F));
    REQUIRE(clamp_filter_control_value(descriptor, 4.0F) == Catch::Approx(2.0F));
}
