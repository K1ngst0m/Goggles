#include "input_forwarder.hpp"

#include "xwayland_server.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <array>
#include <cstdio>

namespace goggles::input {

namespace {

// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
auto sdl_to_linux_keycode(SDL_Scancode scancode) -> uint32_t {
    switch (scancode) {
    case SDL_SCANCODE_A:
        return 30;
    case SDL_SCANCODE_B:
        return 48;
    case SDL_SCANCODE_C:
        return 46;
    case SDL_SCANCODE_D:
        return 32;
    case SDL_SCANCODE_E:
        return 18;
    case SDL_SCANCODE_F:
        return 33;
    case SDL_SCANCODE_G:
        return 34;
    case SDL_SCANCODE_H:
        return 35;
    case SDL_SCANCODE_I:
        return 23;
    case SDL_SCANCODE_J:
        return 36;
    case SDL_SCANCODE_K:
        return 37;
    case SDL_SCANCODE_L:
        return 38;
    case SDL_SCANCODE_M:
        return 50;
    case SDL_SCANCODE_N:
        return 49;
    case SDL_SCANCODE_O:
        return 24;
    case SDL_SCANCODE_P:
        return 25;
    case SDL_SCANCODE_Q:
        return 16;
    case SDL_SCANCODE_R:
        return 19;
    case SDL_SCANCODE_S:
        return 31;
    case SDL_SCANCODE_T:
        return 20;
    case SDL_SCANCODE_U:
        return 22;
    case SDL_SCANCODE_V:
        return 47;
    case SDL_SCANCODE_W:
        return 17;
    case SDL_SCANCODE_X:
        return 45;
    case SDL_SCANCODE_Y:
        return 21;
    case SDL_SCANCODE_Z:
        return 44;
    case SDL_SCANCODE_1:
        return 2;
    case SDL_SCANCODE_2:
        return 3;
    case SDL_SCANCODE_3:
        return 4;
    case SDL_SCANCODE_4:
        return 5;
    case SDL_SCANCODE_5:
        return 6;
    case SDL_SCANCODE_6:
        return 7;
    case SDL_SCANCODE_7:
        return 8;
    case SDL_SCANCODE_8:
        return 9;
    case SDL_SCANCODE_9:
        return 10;
    case SDL_SCANCODE_0:
        return 11;
    case SDL_SCANCODE_ESCAPE:
        return 1;
    case SDL_SCANCODE_RETURN:
        return 28;
    case SDL_SCANCODE_BACKSPACE:
        return 14;
    case SDL_SCANCODE_TAB:
        return 15;
    case SDL_SCANCODE_SPACE:
        return 57;
    case SDL_SCANCODE_UP:
        return 103;
    case SDL_SCANCODE_DOWN:
        return 108;
    case SDL_SCANCODE_LEFT:
        return 105;
    case SDL_SCANCODE_RIGHT:
        return 106;
    case SDL_SCANCODE_LCTRL:
        return 29;
    case SDL_SCANCODE_LSHIFT:
        return 42;
    case SDL_SCANCODE_LALT:
        return 56;
    case SDL_SCANCODE_RCTRL:
        return 97;
    case SDL_SCANCODE_RSHIFT:
        return 54;
    case SDL_SCANCODE_RALT:
        return 100;
    default:
        return 0;
    }
}

// X11 keycodes are Linux keycodes + 8 (X11 protocol offset)
auto linux_to_x11_keycode(uint32_t linux_keycode) -> uint32_t {
    return linux_keycode + 8;
}

} // anonymous namespace

struct InputForwarder::Impl {
    XWaylandServer server;
    Display* x11_display = nullptr;

    ~Impl() {
        if (x11_display) {
            XCloseDisplay(x11_display);
        }
    }
};

InputForwarder::InputForwarder() : m_impl(std::make_unique<Impl>()) {}

InputForwarder::~InputForwarder() = default;

auto InputForwarder::init() -> Result<void> {
    auto start_result = m_impl->server.start();
    if (!start_result) {
        return make_error<void>(start_result.error().code, start_result.error().message);
    }
    int display_num = *start_result;

    std::array<char, 32> display_str{};
    std::snprintf(display_str.data(), display_str.size(), ":%d", display_num);

    m_impl->x11_display = XOpenDisplay(display_str.data());
    if (!m_impl->x11_display) {
        m_impl->server.stop();
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to connect to XWayland server on DISPLAY " +
                                    std::string(display_str.data()));
    }

    // TODO: Send DISPLAY number to layer via IPC (Task 16)

    return {};
}

auto InputForwarder::forward_key(const SDL_KeyboardEvent& event) -> Result<void> {
    if (!m_impl->x11_display) {
        return {};
    }

    uint32_t linux_keycode = sdl_to_linux_keycode(event.scancode);
    if (linux_keycode == 0) {
        return {};
    }

    uint32_t x11_keycode = linux_to_x11_keycode(linux_keycode);
    Bool is_press = event.down ? True : False;

    XTestFakeKeyEvent(m_impl->x11_display, x11_keycode, is_press, CurrentTime);
    XFlush(m_impl->x11_display);

    return {};
}

auto InputForwarder::forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void> {
    if (!m_impl->x11_display) {
        return {};
    }

    // SDL and X11 use same button mapping: 1=left, 2=middle, 3=right
    unsigned int x11_button = event.button;
    Bool is_press = event.down ? True : False;

    XTestFakeButtonEvent(m_impl->x11_display, x11_button, is_press, CurrentTime);
    XFlush(m_impl->x11_display);

    return {};
}

auto InputForwarder::forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void> {
    if (!m_impl->x11_display) {
        return {};
    }

    int x = static_cast<int>(event.x);
    int y = static_cast<int>(event.y);

    // TODO: Coordinate mapping not implemented
    // This causes mouse position to be incorrect when viewer and target windows have different
    // sizes Needs: scale factor calculation, window dimension tracking, coordinate space
    // transformation See OpenSpec for coordinate mapping task
    XTestFakeMotionEvent(m_impl->x11_display, 0, x, y, CurrentTime);
    XFlush(m_impl->x11_display);

    return {};
}

auto InputForwarder::forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void> {
    if (!m_impl->x11_display) {
        return {};
    }

    // X11 protocol: wheel = button events: 4=up, 5=down, 6=left, 7=right
    unsigned int button = 0;

    if (event.y > 0) {
        button = 4;
    } else if (event.y < 0) {
        button = 5;
    } else if (event.x > 0) {
        button = 7;
    } else if (event.x < 0) {
        button = 6;
    } else {
        return {};
    }

    XTestFakeButtonEvent(m_impl->x11_display, button, True, CurrentTime);
    XTestFakeButtonEvent(m_impl->x11_display, button, False, CurrentTime);
    XFlush(m_impl->x11_display);

    return {};
}

auto InputForwarder::display_number() const -> int {
    return m_impl->server.display_number();
}

} // namespace goggles::input
