#pragma once

#include <cstdint>

#ifndef GOGGLES_SCALE_MODE_DEFINED
#define GOGGLES_SCALE_MODE_DEFINED

namespace goggles {

enum class ScaleMode : std::uint8_t {
    fit,
    fill,
    stretch,
    integer,
    dynamic,
};

[[nodiscard]] constexpr auto to_string(ScaleMode mode) -> const char* {
    switch (mode) {
    case ScaleMode::fit:
        return "fit";
    case ScaleMode::fill:
        return "fill";
    case ScaleMode::stretch:
        return "stretch";
    case ScaleMode::integer:
        return "integer";
    case ScaleMode::dynamic:
        return "dynamic";
    }
    return "unknown";
}

} // namespace goggles

#endif // GOGGLES_SCALE_MODE_DEFINED
