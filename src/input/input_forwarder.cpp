#include "input_forwarder.hpp"

#include "compositor_server.hpp"

#include <linux/input-event-codes.h>
#include <util/logging.hpp>

namespace goggles::input {

namespace {

auto sdl_to_linux_keycode(SDL_Scancode scancode) -> uint32_t {
    switch (scancode) {
    case SDL_SCANCODE_A:
        return KEY_A;
    case SDL_SCANCODE_B:
        return KEY_B;
    case SDL_SCANCODE_C:
        return KEY_C;
    case SDL_SCANCODE_D:
        return KEY_D;
    case SDL_SCANCODE_E:
        return KEY_E;
    case SDL_SCANCODE_F:
        return KEY_F;
    case SDL_SCANCODE_G:
        return KEY_G;
    case SDL_SCANCODE_H:
        return KEY_H;
    case SDL_SCANCODE_I:
        return KEY_I;
    case SDL_SCANCODE_J:
        return KEY_J;
    case SDL_SCANCODE_K:
        return KEY_K;
    case SDL_SCANCODE_L:
        return KEY_L;
    case SDL_SCANCODE_M:
        return KEY_M;
    case SDL_SCANCODE_N:
        return KEY_N;
    case SDL_SCANCODE_O:
        return KEY_O;
    case SDL_SCANCODE_P:
        return KEY_P;
    case SDL_SCANCODE_Q:
        return KEY_Q;
    case SDL_SCANCODE_R:
        return KEY_R;
    case SDL_SCANCODE_S:
        return KEY_S;
    case SDL_SCANCODE_T:
        return KEY_T;
    case SDL_SCANCODE_U:
        return KEY_U;
    case SDL_SCANCODE_V:
        return KEY_V;
    case SDL_SCANCODE_W:
        return KEY_W;
    case SDL_SCANCODE_X:
        return KEY_X;
    case SDL_SCANCODE_Y:
        return KEY_Y;
    case SDL_SCANCODE_Z:
        return KEY_Z;
    case SDL_SCANCODE_1:
        return KEY_1;
    case SDL_SCANCODE_2:
        return KEY_2;
    case SDL_SCANCODE_3:
        return KEY_3;
    case SDL_SCANCODE_4:
        return KEY_4;
    case SDL_SCANCODE_5:
        return KEY_5;
    case SDL_SCANCODE_6:
        return KEY_6;
    case SDL_SCANCODE_7:
        return KEY_7;
    case SDL_SCANCODE_8:
        return KEY_8;
    case SDL_SCANCODE_9:
        return KEY_9;
    case SDL_SCANCODE_0:
        return KEY_0;
    case SDL_SCANCODE_ESCAPE:
        return KEY_ESC;
    case SDL_SCANCODE_RETURN:
        return KEY_ENTER;
    case SDL_SCANCODE_BACKSPACE:
        return KEY_BACKSPACE;
    case SDL_SCANCODE_TAB:
        return KEY_TAB;
    case SDL_SCANCODE_SPACE:
        return KEY_SPACE;
    case SDL_SCANCODE_UP:
        return KEY_UP;
    case SDL_SCANCODE_DOWN:
        return KEY_DOWN;
    case SDL_SCANCODE_LEFT:
        return KEY_LEFT;
    case SDL_SCANCODE_RIGHT:
        return KEY_RIGHT;
    case SDL_SCANCODE_LCTRL:
        return KEY_LEFTCTRL;
    case SDL_SCANCODE_LSHIFT:
        return KEY_LEFTSHIFT;
    case SDL_SCANCODE_LALT:
        return KEY_LEFTALT;
    case SDL_SCANCODE_RCTRL:
        return KEY_RIGHTCTRL;
    case SDL_SCANCODE_RSHIFT:
        return KEY_RIGHTSHIFT;
    case SDL_SCANCODE_RALT:
        return KEY_RIGHTALT;
    default:
        return 0;
    }
}

auto sdl_to_linux_button(uint8_t sdl_button) -> uint32_t {
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        return BTN_LEFT;
    case SDL_BUTTON_MIDDLE:
        return BTN_MIDDLE;
    case SDL_BUTTON_RIGHT:
        return BTN_RIGHT;
    case SDL_BUTTON_X1:
        return BTN_SIDE;
    case SDL_BUTTON_X2:
        return BTN_EXTRA;
    default:
        // SDL buttons beyond X2: map to BTN_FORWARD, BTN_BACK, BTN_TASK
        if (sdl_button == 6) {
            return BTN_FORWARD;
        }
        if (sdl_button == 7) {
            return BTN_BACK;
        }
        if (sdl_button == 8) {
            return BTN_TASK;
        }
        // Fallback: BTN_MISC + offset for unknown buttons
        if (sdl_button > 8) {
            GOGGLES_LOG_TRACE("Unmapped SDL button {} -> BTN_MISC+{}", sdl_button, sdl_button - 8);
            return BTN_MISC + (sdl_button - 8);
        }
        return 0;
    }
}

} // anonymous namespace

struct InputForwarder::Impl {
    CompositorServer server;
};

InputForwarder::InputForwarder() : m_impl(std::make_unique<Impl>()) {}

InputForwarder::~InputForwarder() = default;

auto InputForwarder::create() -> ResultPtr<InputForwarder> {
    auto forwarder = std::unique_ptr<InputForwarder>(new InputForwarder());

    auto start_result = forwarder->m_impl->server.start();
    if (!start_result) {
        return make_result_ptr_error<InputForwarder>(start_result.error().code,
                                                     start_result.error().message);
    }

    return make_result_ptr(std::move(forwarder));
}

auto InputForwarder::forward_key(const SDL_KeyboardEvent& event) -> Result<void> {
    uint32_t linux_keycode = sdl_to_linux_keycode(event.scancode);
    if (linux_keycode == 0) {
        GOGGLES_LOG_TRACE("Unmapped key scancode={}, down={}", static_cast<int>(event.scancode),
                          event.down);
        return {};
    }

    GOGGLES_LOG_TRACE("Forward key scancode={}, down={} -> linux_keycode={}",
                      static_cast<int>(event.scancode), event.down, linux_keycode);
    if (!m_impl->server.inject_key(linux_keycode, event.down)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped key event");
    }
    return {};
}

auto InputForwarder::forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void> {
    uint32_t button = sdl_to_linux_button(event.button);
    if (button == 0) {
        GOGGLES_LOG_TRACE("Unmapped mouse button {}", event.button);
        return {};
    }

    if (!m_impl->server.inject_pointer_button(button, event.down)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped button event");
    }
    return {};
}

auto InputForwarder::forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void> {
    if (!m_impl->server.inject_pointer_motion(
            static_cast<double>(event.x), static_cast<double>(event.y),
            static_cast<double>(event.xrel), static_cast<double>(event.yrel))) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped motion event");
    }
    return {};
}

auto InputForwarder::forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void> {
    if (event.y != 0) {
        // SDL: positive = up, Wayland: positive = down; negate to match
        double value = static_cast<double>(-event.y) * 15.0;
        if (!m_impl->server.inject_pointer_axis(value, false)) {
            GOGGLES_LOG_DEBUG("Input queue full, dropped axis event");
        }
    }

    if (event.x != 0) {
        double value = static_cast<double>(event.x) * 15.0;
        if (!m_impl->server.inject_pointer_axis(value, true)) {
            GOGGLES_LOG_DEBUG("Input queue full, dropped axis event");
        }
    }

    return {};
}

auto InputForwarder::x11_display() const -> std::string {
    return m_impl->server.x11_display();
}

auto InputForwarder::wayland_display() const -> std::string {
    return m_impl->server.wayland_display();
}

auto InputForwarder::is_pointer_locked() const -> bool {
    return m_impl->server.is_pointer_locked();
}

} // namespace goggles::input
