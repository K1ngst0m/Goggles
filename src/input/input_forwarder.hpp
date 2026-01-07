#pragma once

#include <SDL3/SDL_events.h>
#include <memory>
#include <util/error.hpp>

namespace goggles::input {

class InputForwarder {
public:
    ~InputForwarder();

    InputForwarder(const InputForwarder&) = delete;
    InputForwarder& operator=(const InputForwarder&) = delete;
    InputForwarder(InputForwarder&&) = delete;
    InputForwarder& operator=(InputForwarder&&) = delete;

    [[nodiscard]] static auto create() -> ResultPtr<InputForwarder>;
    [[nodiscard]] auto forward_key(const SDL_KeyboardEvent& event) -> Result<void>;
    [[nodiscard]] auto forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void>;
    [[nodiscard]] auto forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void>;
    [[nodiscard]] auto forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void>;
    [[nodiscard]] auto x11_display() const -> std::string;
    [[nodiscard]] auto wayland_display() const -> std::string;

private:
    InputForwarder();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
