#pragma once

#include "compositor_server.hpp"

#include <SDL3/SDL_events.h>
#include <memory>
#include <util/error.hpp>
#include <vector>

namespace goggles::input {

/// @brief Forwards SDL input events into a headless compositor.
///
/// The forwarder starts an internal `CompositorServer` and injects events into the focused surface.
class InputForwarder {
public:
    ~InputForwarder();

    InputForwarder(const InputForwarder&) = delete;
    InputForwarder& operator=(const InputForwarder&) = delete;
    InputForwarder(InputForwarder&&) = delete;
    InputForwarder& operator=(InputForwarder&&) = delete;

    /// @brief Creates and starts an input forwarder.
    /// @return A ready forwarder or an error.
    [[nodiscard]] static auto create() -> ResultPtr<InputForwarder>;
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
    /// @brief Returns the X11 display name for the internal compositor.
    [[nodiscard]] auto x11_display() const -> std::string;
    /// @brief Returns the Wayland socket name for the internal compositor.
    [[nodiscard]] auto wayland_display() const -> std::string;
    /// @brief Returns true if pointer is currently locked by target app.
    [[nodiscard]] auto is_pointer_locked() const -> bool;
    /// @brief Returns a snapshot of all connected surfaces.
    [[nodiscard]] auto get_surfaces() const -> std::vector<SurfaceInfo>;
    /// @brief Returns true if a manual input target is set.
    [[nodiscard]] auto is_manual_override_active() const -> bool;
    /// @brief Sets a manual input target by surface ID.
    void set_input_target(uint32_t surface_id);
    /// @brief Clears manual override, reverting to auto-selection (first surface).
    void clear_input_override();

private:
    InputForwarder();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
