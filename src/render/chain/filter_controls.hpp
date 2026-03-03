#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace goggles::render {

using FilterControlId = std::uint64_t;

enum class FilterControlStage : std::uint8_t { prechain, effect };

/// @brief Boundary-safe shader control descriptor used by backend/app/UI.
struct FilterControlDescriptor {
    FilterControlId control_id = 0;
    FilterControlStage stage = FilterControlStage::effect;
    std::string name;
    std::optional<std::string> description;
    float current_value = 0.0F;
    float default_value = 0.0F;
    float min_value = 0.0F;
    float max_value = 0.0F;
    float step = 0.0F;
};

[[nodiscard]] inline auto to_string(FilterControlStage stage) -> const char* {
    switch (stage) {
    case FilterControlStage::prechain:
        return "prechain";
    case FilterControlStage::effect:
        return "effect";
    }
    return "effect";
}

[[nodiscard]] auto make_filter_control_id(FilterControlStage stage, std::string_view name)
    -> FilterControlId;

[[nodiscard]] auto clamp_filter_control_value(const FilterControlDescriptor& descriptor,
                                              float value) -> float;

} // namespace goggles::render
