#pragma once

namespace goggles::util {

struct CompositorRuntimeMetricsSnapshot {
    float game_fps = 0.0F;
    float compositor_latency_ms = 0.0F;
};

} // namespace goggles::util
