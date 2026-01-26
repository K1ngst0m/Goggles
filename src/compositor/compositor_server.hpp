#pragma once

#include <SDL3/SDL_events.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <util/error.hpp>
#include <util/external_image.hpp>
#include <vector>

namespace goggles::input {

/// @brief Identifies input events queued for dispatch on the compositor thread.
enum class InputEventType : std::uint8_t { key, pointer_motion, pointer_button, pointer_axis };

/// @brief Metadata for a connected surface.
struct SurfaceInfo {
    uint32_t id;
    std::string title;
    std::string class_name;
    int width;
    int height;
    bool is_xwayland;
    bool is_input_target;
};

/// @brief Normalized input event for compositor injection.
struct InputEvent {
    InputEventType type;
    uint32_t code;
    bool pressed;
    double x, y;
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

    /// @brief Creates and starts a compositor server.
    /// @return A ready compositor server or an error.
    [[nodiscard]] static auto create() -> ResultPtr<CompositorServer>;
    /// @brief Starts the compositor thread and binds a Wayland socket.
    /// @return Success or an error.
    [[nodiscard]] auto start() -> Result<void>;
    /// @brief Stops the compositor thread and releases Wayland/XWayland resources.
    void stop();
    /// @brief Returns the X11 display name, or an empty string if unavailable.
    [[nodiscard]] auto x11_display() const -> std::string;
    /// @brief Returns the Wayland socket name, or an empty string if not started.
    [[nodiscard]] auto wayland_display() const -> std::string;

    /// @brief Forwards an SDL keyboard event.
    /// @param event SDL keyboard event.
    /// @return Success. The event may be dropped if the internal queue is full.
    [[nodiscard]] auto forward_key(const SDL_KeyboardEvent& event) -> Result<void>;
    /// @brief Forwards an SDL mouse button event.
    /// @param event SDL mouse button event.
    /// @return Success. The event may be dropped if the internal queue is full.
    [[nodiscard]] auto forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void>;
    /// @brief Forwards an SDL mouse motion event.
    /// @param event SDL mouse motion event.
    /// @return Success. The event may be dropped if the internal queue is full.
    [[nodiscard]] auto forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void>;
    /// @brief Forwards an SDL mouse wheel event.
    /// @param event SDL mouse wheel event.
    /// @return Success. The event may be dropped if the internal queue is full.
    [[nodiscard]] auto forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void>;

    /// @brief Queues an input event for the focused surface.
    /// @param event The input event to queue.
    /// @return True if the event was queued and the compositor was notified.
    [[nodiscard]] auto inject_event(const InputEvent& event) -> bool;
    /// @brief Returns true if pointer is currently locked (not confined) by target app.
    [[nodiscard]] auto is_pointer_locked() const -> bool;

    /// @brief Returns the latest compositor-presented frame (DMA-BUF), if available.
    /// @param after_frame_number Return a frame only if newer than this number.
    [[nodiscard]] auto get_presented_frame(uint64_t after_frame_number) const
        -> std::optional<util::ExternalImageFrame>;

    /// @brief Returns a snapshot of all connected surfaces.
    [[nodiscard]] auto get_surfaces() const -> std::vector<SurfaceInfo>;
    /// @brief Returns true if a manual input target is set.
    [[nodiscard]] auto is_manual_override_active() const -> bool;
    /// @brief Sets a manual input target by surface ID.
    void set_input_target(uint32_t surface_id);
    /// @brief Clears manual override, reverting to auto-selection (first surface).
    void clear_input_override();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
