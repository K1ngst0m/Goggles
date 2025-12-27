#pragma once

#include <memory>
#include <SDL3/SDL_events.h>
#include <util/error.hpp>

namespace goggles::input {

class InputForwarder {
public:
    InputForwarder();
    ~InputForwarder();

    InputForwarder(const InputForwarder&) = delete;
    InputForwarder& operator=(const InputForwarder&) = delete;
    InputForwarder(InputForwarder&&) = delete;
    InputForwarder& operator=(InputForwarder&&) = delete;

    [[nodiscard]] auto init() -> Result<void>;
    [[nodiscard]] auto forward_key(const SDL_KeyboardEvent& event) -> Result<void>;
    [[nodiscard]] auto forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void>;
    [[nodiscard]] auto forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void>;
    [[nodiscard]] auto forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void>;
    [[nodiscard]] auto display_number() const -> int;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goggles::input
