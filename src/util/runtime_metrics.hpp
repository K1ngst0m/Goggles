#pragma once

#include <array>
#include <cstddef>

namespace goggles::util {

struct CompositorRuntimeMetricsSnapshot {
    static constexpr std::size_t K_HISTORY_WINDOW = 120;

    float game_fps = 0.0F;
    float compositor_latency_ms = 0.0F;
    std::array<float, K_HISTORY_WINDOW> game_fps_history{};
    std::array<float, K_HISTORY_WINDOW> compositor_latency_history_ms{};
    std::size_t game_fps_history_count = 0;
    std::size_t compositor_latency_history_count = 0;
};

} // namespace goggles::util
