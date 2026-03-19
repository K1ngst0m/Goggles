#pragma once

#include <array>
#include <chrono>

extern "C" {
// NOLINTBEGIN(readability-identifier-naming)
struct wlr_surface;
// NOLINTEND(readability-identifier-naming)
}

#include <util/runtime_metrics.hpp>

namespace goggles::compositor {

using ::wlr_surface;

struct RuntimeMetricsState {
    struct CaptureTarget {
        wlr_surface* root_surface = nullptr;
        wlr_surface* surface = nullptr;
    };

    static constexpr size_t K_SAMPLE_WINDOW =
        util::CompositorRuntimeMetricsSnapshot::K_HISTORY_WINDOW;

    wlr_surface* capture_target_root_surface = nullptr;
    wlr_surface* capture_target_surface = nullptr;
    std::array<float, K_SAMPLE_WINDOW> game_frame_intervals_ms{};
    std::array<float, K_SAMPLE_WINDOW> compositor_latency_samples_ms{};
    size_t game_frame_interval_index = 0;
    size_t compositor_latency_index = 0;
    size_t game_frame_interval_count = 0;
    size_t compositor_latency_count = 0;
    std::chrono::steady_clock::time_point last_game_commit_time;
    std::chrono::steady_clock::time_point pending_capture_commit_time;
    bool has_last_game_commit_time = false;
    bool has_pending_capture_commit_time = false;
    util::CompositorRuntimeMetricsSnapshot snapshot;

    void clear_samples() {
        game_frame_intervals_ms.fill(0.0F);
        compositor_latency_samples_ms.fill(0.0F);
        game_frame_interval_index = 0;
        compositor_latency_index = 0;
        game_frame_interval_count = 0;
        compositor_latency_count = 0;
        has_last_game_commit_time = false;
        has_pending_capture_commit_time = false;
        snapshot = {};
    }

    void reset_for_capture_target(const CaptureTarget& capture_target) {
        capture_target_root_surface = capture_target.root_surface;
        capture_target_surface = capture_target.surface;
        clear_samples();
    }

    [[nodiscard]] bool should_reset_for_capture_target(const CaptureTarget& capture_target) const {
        return capture_target_root_surface != capture_target.root_surface ||
               capture_target_surface != capture_target.surface;
    }

    [[nodiscard]] bool should_track_surface_commit(wlr_surface* committed_surface) const {
        return capture_target_root_surface != nullptr && capture_target_surface != nullptr &&
               committed_surface != nullptr && committed_surface == capture_target_surface;
    }
};

} // namespace goggles::compositor
