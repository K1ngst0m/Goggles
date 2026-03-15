#pragma once

#include <cstdint>

namespace goggles::filter_chain {

/// @brief Log severity levels matching the C API `GOGGLES_FC_LOG_LEVEL_*` constants.
enum class LogLevel : uint32_t { // NOLINT(performance-enum-size) ABI-stable uint32_t
    trace = 0u,
    debug = 1u,
    info = 2u,
    warn = 3u,
    error = 4u,
    critical = 5u,
};

/// @brief Preset source kind matching `GOGGLES_FC_PRESET_SOURCE_*` constants.
enum class PresetSourceKind : uint32_t { // NOLINT(performance-enum-size) ABI-stable uint32_t
    file = 0u,
    memory = 1u,
};

/// @brief Scale mode matching `GOGGLES_FC_SCALE_MODE_*` constants.
enum class ScaleMode : uint32_t { // NOLINT(performance-enum-size) ABI-stable uint32_t
    stretch = 0u,
    fit = 1u,
    integer = 2u,
    fill = 3u,
    dynamic = 4u,
};

/// @brief Pipeline stage identifier matching `GOGGLES_FC_STAGE_*` constants.
enum class Stage : uint32_t { // NOLINT(performance-enum-size) ABI-stable uint32_t
    prechain = 0u,
    effect = 1u,
    postchain = 2u,
};

/// @brief Source provenance matching `GOGGLES_FC_PROVENANCE_*` constants.
enum class Provenance : uint32_t { // NOLINT(performance-enum-size) ABI-stable uint32_t
    file = 0u,
    memory = 1u,
};

/// @brief 2D pixel extent.
struct Extent2D {
    uint32_t width = 0;
    uint32_t height = 0;
};

} // namespace goggles::filter_chain
