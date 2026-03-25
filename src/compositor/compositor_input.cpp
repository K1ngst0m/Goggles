#include "compositor_state.hpp"

#include <ctime>
#include <linux/input-event-codes.h>
#include <memory>
#include <unistd.h>

extern "C" {
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>
}

#include <goggles/profiling.hpp>
#include <util/logging.hpp>

namespace goggles::compositor {

namespace {

auto get_time_msec() -> uint32_t {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

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
        if (sdl_button == 6) {
            return BTN_FORWARD;
        }
        if (sdl_button == 7) {
            return BTN_BACK;
        }
        if (sdl_button == 8) {
            return BTN_TASK;
        }
        if (sdl_button > 8) {
            GOGGLES_LOG_TRACE("Unmapped SDL button {} -> BTN_MISC+{}", sdl_button, sdl_button - 8);
            return BTN_MISC + (sdl_button - 8);
        }
        return 0;
    }
}

} // namespace

auto CompositorState::setup_input_devices() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    seat = wlr_seat_create(display, "seat0");
    if (!seat) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create seat");
    }

    wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkb_ctx) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xkb context");
    }

    xkb_keymap* keymap = xkb_keymap_new_from_names(xkb_ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xkb keymap");
    }

    keyboard = UniqueKeyboard(std::make_unique<wlr_keyboard>().release());
    wlr_keyboard_init(keyboard.get(), nullptr, "virtual-keyboard");
    wlr_keyboard_set_keymap(keyboard.get(), keymap);
    xkb_keymap_unref(keymap);

    wlr_seat_set_keyboard(seat, keyboard.get());

    relative_pointer_manager = wlr_relative_pointer_manager_v1_create(display);
    if (!relative_pointer_manager) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to create relative pointer manager");
    }

    pointer_constraints = wlr_pointer_constraints_v1_create(display);
    if (!pointer_constraints) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to create pointer constraints");
    }

    wl_list_init(&listeners.new_pointer_constraint.link);
    listeners.new_pointer_constraint.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                  offsetof(Listeners, new_pointer_constraint));
        list->state->handle_new_pointer_constraint(static_cast<wlr_pointer_constraint_v1*>(data));
    };
    wl_signal_add(&pointer_constraints->events.new_constraint, &listeners.new_pointer_constraint);

    return {};
}

auto CompositorServer::forward_key(const SDL_KeyboardEvent& event) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    uint32_t linux_keycode = sdl_to_linux_keycode(event.scancode);
    if (linux_keycode == 0) {
        GOGGLES_LOG_TRACE("Unmapped key scancode={}, down={}", static_cast<int>(event.scancode),
                          event.down);
        return {};
    }

    GOGGLES_LOG_TRACE("Forward key scancode={}, down={} -> linux_keycode={}",
                      static_cast<int>(event.scancode), event.down, linux_keycode);
    InputEvent input_event{};
    input_event.type = InputEventType::key;
    input_event.code = linux_keycode;
    input_event.pressed = event.down;
    if (!inject_event(input_event)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped key event");
    }
    return {};
}

auto CompositorServer::forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    uint32_t button = sdl_to_linux_button(event.button);
    if (button == 0) {
        GOGGLES_LOG_TRACE("Unmapped mouse button {}", event.button);
        return {};
    }

    InputEvent input_event{};
    input_event.type = InputEventType::pointer_button;
    input_event.code = button;
    input_event.pressed = event.down;
    if (!inject_event(input_event)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped button event");
    }
    return {};
}

auto CompositorServer::forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    InputEvent input_event{};
    input_event.type = InputEventType::pointer_motion;
    input_event.dx = static_cast<double>(event.xrel);
    input_event.dy = static_cast<double>(event.yrel);
    if (!inject_event(input_event)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped motion event");
    }
    return {};
}

auto CompositorServer::forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    if (event.y != 0) {
        InputEvent input_event{};
        input_event.type = InputEventType::pointer_axis;
        input_event.value = static_cast<double>(-event.y) * 15.0;
        input_event.horizontal = false;
        if (!inject_event(input_event)) {
            GOGGLES_LOG_DEBUG("Input queue full, dropped axis event");
        }
    }

    if (event.x != 0) {
        InputEvent input_event{};
        input_event.type = InputEventType::pointer_axis;
        input_event.value = static_cast<double>(event.x) * 15.0;
        input_event.horizontal = true;
        if (!inject_event(input_event)) {
            GOGGLES_LOG_DEBUG("Input queue full, dropped axis event");
        }
    }

    return {};
}

auto CompositorServer::inject_event(const InputEvent& event) -> bool {
    GOGGLES_PROFILE_FUNCTION();
    if (!m_state->event_queue.try_push(event)) {
        return false;
    }
    return m_state->wake_event_loop();
}

bool CompositorState::wake_event_loop() {
    GOGGLES_PROFILE_FUNCTION();
    if (!event_fd.valid()) {
        return false;
    }
    uint64_t value = 1;
    auto result = write(event_fd.get(), &value, sizeof(value));
    return result == sizeof(value);
}

void CompositorState::request_focus_target(uint32_t surface_id) {
    if (surface_id == NO_FOCUS_TARGET) {
        return;
    }
    pending_focus_target.store(surface_id, std::memory_order_release);
    wake_event_loop();
}

void CompositorState::request_surface_resize(uint32_t surface_id, const SurfaceResizeInfo& resize) {
    if (surface_id == NO_FOCUS_TARGET) {
        return;
    }
    SurfaceResizeRequest request{};
    request.surface_id = surface_id;
    request.resize = resize;
    if (!resize_queue.try_push(request)) {
        return;
    }
    wake_event_loop();
}

void CompositorState::process_input_events() {
    GOGGLES_PROFILE_FUNCTION();
    handle_focus_request();
    handle_surface_resize_requests();
    if (present_reset_requested.exchange(false, std::memory_order_acq_rel)) {
        refresh_presented_frame();
    }
    process_capture_pacing();

    while (auto event_opt = event_queue.try_pop()) {
        auto& event = *event_opt;
        uint32_t time = get_time_msec();

        switch (event.type) {
        case InputEventType::key:
            handle_key_event(event, time);
            break;
        case InputEventType::pointer_motion:
            handle_pointer_motion_event(event, time);
            break;
        case InputEventType::pointer_button:
            handle_pointer_button_event(event, time);
            break;
        case InputEventType::pointer_axis:
            handle_pointer_axis_event(event, time);
            break;
        }
    }
}

void CompositorState::handle_key_event(const InputEvent& event, uint32_t time) {
    GOGGLES_PROFILE_FUNCTION();
    auto target = get_input_target(*this);
    wlr_surface* target_surface = target.surface;

    if (!target_surface) {
        return;
    }

    this->prepare_keyboard_dispatch(target);
    auto state = event.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
    wlr_seat_keyboard_notify_key(seat, time, event.code, state);
}

void CompositorState::handle_pointer_motion_event(const InputEvent& event, uint32_t time) {
    GOGGLES_PROFILE_FUNCTION();
    auto root_target = get_root_input_target(*this);
    if (!root_target.root_surface) {
        return;
    }

    if (relative_pointer_manager && (event.dx != 0.0 || event.dy != 0.0)) {
        wlr_relative_pointer_manager_v1_send_relative_motion(
            relative_pointer_manager, seat, static_cast<uint64_t>(time) * 1000, event.dx, event.dy,
            event.dx, event.dy);
    }

    if (active_constraint && active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        apply_cursor_hint_if_needed();
        wlr_seat_pointer_notify_frame(seat);
        return;
    }

    update_cursor_position(event, root_target);

    auto target = resolve_input_target(*this, root_target, true);
    wlr_surface* target_surface = target.surface;
    if (!target_surface) {
        return;
    }

    const auto [local_x, local_y] = get_surface_local_coords(*this, target);
    this->prepare_pointer_motion_dispatch(target, local_x, local_y);
    wlr_seat_pointer_notify_motion(seat, time, local_x, local_y);
    wlr_seat_pointer_notify_frame(seat);
}

void CompositorState::handle_pointer_button_event(const InputEvent& event, uint32_t time) {
    GOGGLES_PROFILE_FUNCTION();
    auto root_target = get_root_input_target(*this);
    auto target = resolve_input_target(*this, root_target, true);
    wlr_surface* target_surface = target.surface;
    wlr_surface* cursor_reference = target.root_surface ? target.root_surface : target_surface;

    if (!target_surface) {
        return;
    }

    if (cursor_surface != cursor_reference || !cursor_initialized) {
        reset_cursor_for_surface(cursor_reference);
    }

    const auto [local_x, local_y] = get_surface_local_coords(*this, target);
    this->prepare_pointer_button_dispatch(target, local_x, local_y);
    auto state = event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
    wlr_seat_pointer_notify_button(seat, time, event.code, state);
    wlr_seat_pointer_notify_frame(seat);
}

void CompositorState::handle_pointer_axis_event(const InputEvent& event, uint32_t time) {
    GOGGLES_PROFILE_FUNCTION();
    auto root_target = get_root_input_target(*this);
    auto target = resolve_input_target(*this, root_target, true);
    wlr_surface* target_surface = target.surface;
    wlr_surface* cursor_reference = target.root_surface ? target.root_surface : target_surface;

    if (!target_surface) {
        return;
    }

    if (cursor_surface != cursor_reference || !cursor_initialized) {
        reset_cursor_for_surface(cursor_reference);
    }

    const auto [local_x, local_y] = get_surface_local_coords(*this, target);
    this->prepare_pointer_axis_dispatch(target, local_x, local_y);
    auto orientation =
        event.horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL : WL_POINTER_AXIS_VERTICAL_SCROLL;
    wlr_seat_pointer_notify_axis(seat, time, orientation, event.value, 0,
                                 WL_POINTER_AXIS_SOURCE_WHEEL,
                                 WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
    wlr_seat_pointer_notify_frame(seat);
}

} // namespace goggles::compositor
