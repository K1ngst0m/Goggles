#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <util/error.hpp>

namespace goggles::input {

/// @brief Identifies input events queued for dispatch on the compositor thread.
enum class InputEventType : std::uint8_t { key, pointer_motion, pointer_button, pointer_axis };

/// @brief Normalized input event for compositor injection.
struct InputEvent {
    InputEventType type;
    uint32_t code;
    bool pressed;
    double x, y;
    double value;
    bool horizontal;
};

/// @brief Runs a headless Wayland/XWayland compositor for input injection.
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

    /// @brief Starts the compositor thread and binds a Wayland socket.
    /// @return Success or an error.
    [[nodiscard]] auto start() -> Result<void>;
    /// @brief Stops the compositor thread and releases Wayland/XWayland resources.
    void stop();
    /// @brief Returns the X11 display name, or an empty string if unavailable.
    [[nodiscard]] auto x11_display() const -> std::string;
    /// @brief Returns the Wayland socket name, or an empty string if not started.
    [[nodiscard]] auto wayland_display() const -> std::string;

    /// @brief Queues a Linux key event for the focused surface.
    /// @param linux_keycode Linux input keycode (see `linux/input-event-codes.h`).
    /// @param pressed True for press, false for release.
    /// @return True if the event was queued and the compositor was notified.
    [[nodiscard]] auto inject_key(uint32_t linux_keycode, bool pressed) -> bool;
    /// @brief Queues a pointer motion event for the focused surface.
    /// @param sx Surface-local x coordinate.
    /// @param sy Surface-local y coordinate.
    /// @return True if the event was queued and the compositor was notified.
    [[nodiscard]] auto inject_pointer_motion(double sx, double sy) -> bool;
    /// @brief Queues a pointer button event for the focused surface.
    /// @param button Linux button code (see `linux/input-event-codes.h`).
    /// @param pressed True for press, false for release.
    /// @return True if the event was queued and the compositor was notified.
    [[nodiscard]] auto inject_pointer_button(uint32_t button, bool pressed) -> bool;
    /// @brief Queues a pointer scroll event for the focused surface.
    /// @param value Scroll delta in compositor units.
    /// @param horizontal True for horizontal scroll, false for vertical.
    /// @return True if the event was queued and the compositor was notified.
    [[nodiscard]] auto inject_pointer_axis(double value, bool horizontal) -> bool;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
