#pragma once

#include <SDL3/SDL_events.h>
#include <cstdint>
#include <goggles/error.hpp>
#include <memory>
#include <optional>
#include <string>
#include <util/external_image.hpp>
#include <util/runtime_metrics.hpp>
#include <vector>

namespace goggles::compositor {

enum class InputEventType : std::uint8_t { key, pointer_motion, pointer_button, pointer_axis };

struct SurfaceInfo {
    uint32_t id;
    std::string title;
    std::string class_name;
    int width;
    int height;
    bool is_xwayland;
    bool is_input_target;
    bool filter_chain_enabled = false;
};

struct SurfaceResizeInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    bool maximized = false;
};

/// Normalized from SDL events into compositor-native units.
struct InputEvent {
    InputEventType type;
    uint32_t code;
    bool pressed;
    double dx, dy;
    double value;
    bool horizontal;
};

/// @brief Runs a headless Wayland/XWayland compositor for input forwarding and surface capture.
///
/// `start()` spawns a compositor thread. Input injection methods queue events for that thread.
class CompositorServer {
public:
    CompositorServer();
    ~CompositorServer();

    CompositorServer(const CompositorServer&) = delete;
    CompositorServer& operator=(const CompositorServer&) = delete;
    CompositorServer(CompositorServer&&) = delete;
    CompositorServer& operator=(CompositorServer&&) = delete;

    [[nodiscard]] static auto create() -> ResultPtr<CompositorServer>;
    [[nodiscard]] auto start() -> Result<void>;
    void stop();
    [[nodiscard]] auto x11_display() const -> std::string;
    [[nodiscard]] auto wayland_display() const -> std::string;
    [[nodiscard]] auto target_fps() const -> uint32_t;
    void set_target_fps(uint32_t target_fps);

    /// Events may be silently dropped if the internal queue is full.
    [[nodiscard]] auto forward_key(const SDL_KeyboardEvent& event) -> Result<void>;
    /// Events may be silently dropped if the internal queue is full.
    [[nodiscard]] auto forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void>;
    /// Events may be silently dropped if the internal queue is full.
    [[nodiscard]] auto forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void>;
    /// Events may be silently dropped if the internal queue is full.
    [[nodiscard]] auto forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void>;

    /// @return True if the event was queued and the compositor was notified.
    [[nodiscard]] auto inject_event(const InputEvent& event) -> bool;
    /// Locked (not confined) by the target app's pointer lock request.
    [[nodiscard]] auto is_pointer_locked() const -> bool;
    void set_cursor_visible(bool visible);

    [[nodiscard]] auto get_presented_frame(uint64_t after_frame_number) const
        -> std::optional<util::ExternalImageFrame>;
    [[nodiscard]] auto get_runtime_metrics_snapshot() const
        -> util::CompositorRuntimeMetricsSnapshot;

    [[nodiscard]] auto get_surfaces() const -> std::vector<SurfaceInfo>;
    void set_input_target(uint32_t surface_id);
    void request_surface_resize(uint32_t surface_id, const SurfaceResizeInfo& resize);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::compositor
