#include "compositor_server.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <linux/input-event-codes.h>
#include <memory>
#include <sys/eventfd.h>
#include <unistd.h>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>

// xwayland.h contains 'char *class' which conflicts with C++ keyword
#define class class_
#include <wlr/xwayland/xwayland.h>
#undef class

#include <wlr/types/wlr_xdg_shell.h>
}

namespace goggles::input {

namespace {

auto get_time_msec() -> uint32_t {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

auto bind_wayland_socket(wl_display* display) -> Result<int> {
    for (int display_num = 1; display_num < 10; ++display_num) {
        std::array<char, 32> socket_name{};
        std::snprintf(socket_name.data(), socket_name.size(), "wayland-%d", display_num);
        if (wl_display_add_socket(display, socket_name.data()) == 0) {
            return display_num;
        }
    }
    return make_error<int>(ErrorCode::input_init_failed,
                           "No available DISPLAY numbers (1-9 all bound)");
}

} // anonymous namespace

struct CompositorServer::Listeners {
    CompositorServer* server = nullptr;
    wl_listener new_xdg_toplevel{};
    wl_listener xdg_surface_commit{};
    wl_listener xdg_surface_map{};
    wl_listener xdg_surface_destroy{};
    wl_listener new_xwayland_surface{};
    wl_listener xwayland_surface_associate{};
    wl_listener xwayland_surface_map{};
    wl_listener xwayland_surface_destroy{};
    wlr_xdg_toplevel* pending_toplevel = nullptr;
    wlr_xwayland_surface* pending_xsurface = nullptr;
};

CompositorServer::CompositorServer() : m_listeners(std::make_unique<Listeners>()) {
    m_listeners->server = this;
    wl_list_init(&m_listeners->xdg_surface_commit.link);
    wl_list_init(&m_listeners->xdg_surface_map.link);
    wl_list_init(&m_listeners->xdg_surface_destroy.link);
    wl_list_init(&m_listeners->xwayland_surface_associate.link);
    wl_list_init(&m_listeners->xwayland_surface_map.link);
    wl_list_init(&m_listeners->xwayland_surface_destroy.link);
}

CompositorServer::~CompositorServer() {
    stop();
}

auto CompositorServer::start() -> Result<int> {
    auto cleanup_on_error = [this](void*) { stop(); };
    std::unique_ptr<void, decltype(cleanup_on_error)> guard(this, cleanup_on_error);

    m_display = wl_display_create();
    if (!m_display) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create Wayland display");
    }

    m_event_loop = wl_display_get_event_loop(m_display);
    if (!m_event_loop) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to get event loop");
    }

    m_backend = wlr_headless_backend_create(m_event_loop);
    if (!m_backend) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create headless backend");
    }

    m_renderer = wlr_renderer_autocreate(m_backend);
    if (!m_renderer) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create renderer");
    }

    if (!wlr_renderer_init_wl_display(m_renderer, m_display)) {
        return make_error<int>(ErrorCode::input_init_failed,
                               "Failed to initialize renderer protocols");
    }

    m_allocator = wlr_allocator_autocreate(m_backend, m_renderer);
    if (!m_allocator) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create allocator");
    }

    m_compositor = wlr_compositor_create(m_display, 6, m_renderer);
    if (!m_compositor) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create compositor");
    }

    m_output_layout = wlr_output_layout_create(m_display);
    if (!m_output_layout) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create output layout");
    }

    m_xdg_shell = wlr_xdg_shell_create(m_display, 3);
    if (!m_xdg_shell) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xdg-shell");
    }

    wl_list_init(&m_listeners->new_xdg_toplevel.link);
    m_listeners->new_xdg_toplevel.notify = [](wl_listener* listener, void* data) {
        auto* listeners = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(Listeners, new_xdg_toplevel));
        auto* toplevel = static_cast<wlr_xdg_toplevel*>(data);
        listeners->server->handle_new_xdg_toplevel(toplevel);
    };
    wl_signal_add(&m_xdg_shell->events.new_toplevel, &m_listeners->new_xdg_toplevel);

    m_seat = wlr_seat_create(m_display, "seat0");
    if (!m_seat) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create seat");
    }

    wlr_seat_set_capabilities(m_seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_xkb_context) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xkb context");
    }

    xkb_keymap* keymap =
        xkb_keymap_new_from_names(m_xkb_context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xkb keymap");
    }

    m_keyboard = new wlr_keyboard{};
    wlr_keyboard_init(m_keyboard, nullptr, "virtual-keyboard");
    wlr_keyboard_set_keymap(m_keyboard, keymap);
    xkb_keymap_unref(keymap);

    wlr_seat_set_keyboard(m_seat, m_keyboard);

    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create eventfd");
    }
    m_event_fd = util::UniqueFd(efd);

    m_event_source = wl_event_loop_add_fd(
        m_event_loop, m_event_fd.get(), WL_EVENT_READABLE,
        [](int /*fd*/, uint32_t /*mask*/, void* data) -> int {
            auto* server = static_cast<CompositorServer*>(data);
            uint64_t val = 0;
            // eventfd guarantees 8-byte atomic read when readable
            (void)read(server->m_event_fd.get(), &val, sizeof(val));
            server->process_input_events();
            return 0;
        },
        this);

    if (!m_event_source) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to add eventfd to event loop");
    }

    auto socket_result = bind_wayland_socket(m_display);
    if (!socket_result) {
        return make_error<int>(socket_result.error().code, socket_result.error().message);
    }
    m_display_number = *socket_result;

    m_xwayland = wlr_xwayland_create(m_display, m_compositor, false);
    if (!m_xwayland) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create XWayland server");
    }

    m_listeners->new_xwayland_surface.notify = [](wl_listener* listener, void* data) {
        auto* listeners = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(Listeners, new_xwayland_surface));
        auto* xsurface = static_cast<wlr_xwayland_surface*>(data);
        listeners->server->handle_new_xwayland_surface(xsurface);
    };
    wl_signal_add(&m_xwayland->events.new_surface, &m_listeners->new_xwayland_surface);

    // wlr_xwm translates seat events to X11 KeyPress/MotionNotify
    wlr_xwayland_set_seat(m_xwayland, m_seat);

    if (!wlr_backend_start(m_backend)) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to start wlroots backend");
    }

    // Create headless output for native Wayland clients
    m_output = wlr_headless_add_output(m_backend, 1920, 1080);
    if (!m_output) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create headless output");
    }
    wlr_output_init_render(m_output, m_allocator, m_renderer);
    wlr_output_layout_add_auto(m_output_layout, m_output);

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_commit_state(m_output, &state);
    wlr_output_state_finish(&state);

    m_compositor_thread = std::jthread([this] { wl_display_run(m_display); });

    guard.release(); // NOLINT(bugprone-unused-return-value)
    return m_display_number;
}

void CompositorServer::stop() {
    if (!m_display) {
        return;
    }

    wl_display_terminate(m_display);

    // Must join before destroying objects thread accesses
    if (m_compositor_thread.joinable()) {
        m_compositor_thread.join();
    }

    // Must remove before closing eventfd
    if (m_event_source) {
        wl_event_source_remove(m_event_source);
        m_event_source = nullptr;
    }

    m_surfaces.clear();
    m_focused_surface = nullptr;
    m_focused_xsurface = nullptr;

    // Destruction order: xwayland before compositor, seat before display
    if (m_xwayland) {
        wlr_xwayland_destroy(m_xwayland);
        m_xwayland = nullptr;
    }

    if (m_keyboard) {
        wlr_keyboard_finish(m_keyboard);
        delete m_keyboard;
        m_keyboard = nullptr;
    }

    if (m_xkb_context) {
        xkb_context_unref(m_xkb_context);
        m_xkb_context = nullptr;
    }

    if (m_seat) {
        wlr_seat_destroy(m_seat);
        m_seat = nullptr;
    }

    m_xdg_shell = nullptr;
    m_compositor = nullptr;
    m_output = nullptr;

    if (m_output_layout) {
        wlr_output_layout_destroy(m_output_layout);
        m_output_layout = nullptr;
    }

    if (m_allocator) {
        wlr_allocator_destroy(m_allocator);
        m_allocator = nullptr;
    }

    if (m_renderer) {
        wlr_renderer_destroy(m_renderer);
        m_renderer = nullptr;
    }

    if (m_backend) {
        wlr_backend_destroy(m_backend);
        m_backend = nullptr;
    }

    if (m_display) {
        wl_display_destroy(m_display);
        m_display = nullptr;
    }

    m_event_loop = nullptr;
    m_display_number = -1;
}

auto CompositorServer::inject_key(uint32_t linux_keycode, bool pressed) -> bool {
    InputEvent event{};
    event.type = InputEventType::key;
    event.code = linux_keycode;
    event.pressed = pressed;

    if (m_event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

auto CompositorServer::inject_pointer_motion(double sx, double sy) -> bool {
    InputEvent event{};
    event.type = InputEventType::pointer_motion;
    event.x = sx;
    event.y = sy;

    if (m_event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

auto CompositorServer::inject_pointer_button(uint32_t button, bool pressed) -> bool {
    InputEvent event{};
    event.type = InputEventType::pointer_button;
    event.code = button;
    event.pressed = pressed;

    if (m_event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

auto CompositorServer::inject_pointer_axis(double value, bool horizontal) -> bool {
    InputEvent event{};
    event.type = InputEventType::pointer_axis;
    event.value = value;
    event.horizontal = horizontal;

    if (m_event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

void CompositorServer::process_input_events() {
    while (auto event_opt = m_event_queue.try_pop()) {
        auto& event = *event_opt;
        uint32_t time = get_time_msec();

        switch (event.type) {
        case InputEventType::key: {
            if (!m_focused_surface) {
                break;
            }

            // For XWayland: must re-activate and re-enter keyboard focus before each key
            if (m_focused_xsurface) {
                wlr_xwayland_surface_activate(m_focused_xsurface, true);
            }
            wlr_seat_set_keyboard(m_seat, m_keyboard);
            wlr_seat_keyboard_notify_enter(m_seat, m_focused_surface, m_keyboard->keycodes,
                                           m_keyboard->num_keycodes, &m_keyboard->modifiers);

            auto state =
                event.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
            wlr_seat_keyboard_notify_key(m_seat, time, event.code, state);
            break;
        }
        case InputEventType::pointer_motion: {
            if (!m_focused_surface) {
                break;
            }
            // For XWayland: must re-activate and re-enter pointer focus
            if (m_focused_xsurface) {
                wlr_xwayland_surface_activate(m_focused_xsurface, true);
                wlr_seat_pointer_notify_enter(m_seat, m_focused_surface, event.x, event.y);
            }
            wlr_seat_pointer_notify_motion(m_seat, time, event.x, event.y);
            wlr_seat_pointer_notify_frame(m_seat);
            m_last_pointer_x = event.x;
            m_last_pointer_y = event.y;
            break;
        }
        case InputEventType::pointer_button: {
            if (!m_focused_surface) {
                break;
            }
            if (m_focused_xsurface) {
                wlr_xwayland_surface_activate(m_focused_xsurface, true);
                wlr_seat_pointer_notify_enter(m_seat, m_focused_surface, m_last_pointer_x,
                                              m_last_pointer_y);
            }
            auto state =
                event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
            wlr_seat_pointer_notify_button(m_seat, time, event.code, state);
            wlr_seat_pointer_notify_frame(m_seat);
            break;
        }
        case InputEventType::pointer_axis: {
            if (!m_focused_surface) {
                break;
            }
            if (m_focused_xsurface) {
                wlr_xwayland_surface_activate(m_focused_xsurface, true);
                wlr_seat_pointer_notify_enter(m_seat, m_focused_surface, m_last_pointer_x,
                                              m_last_pointer_y);
            }
            auto orientation = event.horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL
                                                : WL_POINTER_AXIS_VERTICAL_SCROLL;
            wlr_seat_pointer_notify_axis(m_seat, time, orientation, event.value,
                                         0, // value_discrete (legacy)
                                         WL_POINTER_AXIS_SOURCE_WHEEL,
                                         WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
            wlr_seat_pointer_notify_frame(m_seat);
            break;
        }
        }
    }
}

void CompositorServer::handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel) {
    // Skip if already waiting for a surface
    if (!wl_list_empty(&m_listeners->xdg_surface_commit.link)) {
        return;
    }

    m_listeners->pending_toplevel = toplevel;

    // Stage 1: Listen for commit to detect when surface becomes initialized
    wl_list_init(&m_listeners->xdg_surface_commit.link);
    m_listeners->xdg_surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* listeners = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(Listeners, xdg_surface_commit));
        listeners->server->handle_xdg_surface_commit();
    };
    wl_signal_add(&toplevel->base->surface->events.commit, &m_listeners->xdg_surface_commit);
}

void CompositorServer::handle_xdg_surface_commit() {
    auto* toplevel = m_listeners->pending_toplevel;
    if (!toplevel || !toplevel->base->initialized) {
        return;
    }

    // Remove commit listener - we only need it once
    wl_list_remove(&m_listeners->xdg_surface_commit.link);
    wl_list_init(&m_listeners->xdg_surface_commit.link);

    // Send initial configure
    wlr_xdg_toplevel_set_activated(toplevel, true);
    wlr_xdg_surface_schedule_configure(toplevel->base);

    // Stage 2: Listen for map signal (buffer committed)
    wl_list_init(&m_listeners->xdg_surface_map.link);
    m_listeners->xdg_surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* listeners = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(Listeners, xdg_surface_map));
        listeners->server->handle_xdg_surface_map();
    };
    wl_signal_add(&toplevel->base->surface->events.map, &m_listeners->xdg_surface_map);
}

void CompositorServer::handle_xdg_surface_map() {
    auto* toplevel = m_listeners->pending_toplevel;
    if (!toplevel) {
        return;
    }

    // Remove map listener
    wl_list_remove(&m_listeners->xdg_surface_map.link);
    wl_list_init(&m_listeners->xdg_surface_map.link);

    wlr_surface* surface = toplevel->base->surface;
    m_surfaces.push_back(surface);

    // Register destroy listener
    wl_list_remove(&m_listeners->xdg_surface_destroy.link);
    wl_list_init(&m_listeners->xdg_surface_destroy.link);
    m_listeners->xdg_surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* listeners = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(Listeners, xdg_surface_destroy));
        listeners->server->handle_xdg_surface_destroy();
    };
    wl_signal_add(&surface->events.destroy, &m_listeners->xdg_surface_destroy);

    // Focus new Wayland surface if:
    // - No current focus, OR
    // - Current focus is XWayland (might be stale - can't detect X11 disconnect)
    if (!m_focused_surface || m_focused_xsurface) {
        focus_surface(surface);
    }

    m_listeners->pending_toplevel = nullptr;
}

void CompositorServer::handle_xdg_surface_destroy() {
    // Remove all XDG-related listeners
    wl_list_remove(&m_listeners->xdg_surface_destroy.link);
    wl_list_init(&m_listeners->xdg_surface_destroy.link);
    wl_list_remove(&m_listeners->xdg_surface_commit.link);
    wl_list_init(&m_listeners->xdg_surface_commit.link);
    wl_list_remove(&m_listeners->xdg_surface_map.link);
    wl_list_init(&m_listeners->xdg_surface_map.link);

    // Clear focus if this was a Wayland surface (not XWayland)
    if (m_focused_surface && !m_focused_xsurface) {
        auto it = std::find(m_surfaces.begin(), m_surfaces.end(), m_focused_surface);
        if (it != m_surfaces.end()) {
            m_surfaces.erase(it);
        }

        m_focused_surface = nullptr;
        wlr_seat_keyboard_clear_focus(m_seat);
        wlr_seat_pointer_clear_focus(m_seat);
    }

    m_listeners->pending_toplevel = nullptr;
}

void CompositorServer::handle_new_xwayland_surface(wlr_xwayland_surface* xsurface) {
    // Listener can only belong to one signal's list - skip if already registered
    if (!wl_list_empty(&m_listeners->xwayland_surface_associate.link)) {
        return;
    }

    // Store xsurface for retrieval in callback
    m_listeners->pending_xsurface = xsurface;

    m_listeners->xwayland_surface_associate.notify = [](wl_listener* listener, void* /*data*/) {
        auto* listeners = reinterpret_cast<Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Listeners, xwayland_surface_associate));
        auto* xsurface_inner = listeners->pending_xsurface;
        listeners->server->handle_xwayland_surface_associate(xsurface_inner);
    };
    wl_signal_add(&xsurface->events.associate, &m_listeners->xwayland_surface_associate);
}

void CompositorServer::handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface) {
    // Remove associate listener - it's one-shot
    wl_list_remove(&m_listeners->xwayland_surface_associate.link);
    wl_list_init(&m_listeners->xwayland_surface_associate.link);

    if (!xsurface->surface) {
        return;
    }

    // NOTE: Do NOT add xsurface->surface to m_surfaces - we can't clean it up
    // when X11 client disconnects (destroy listener breaks X11 input), so it
    // would become a dangling pointer.

    // NOTE: Do NOT register destroy listener on xsurface->surface->events.destroy
    // It fires unexpectedly during normal operation, breaking X11 input entirely.

    // Only focus if no surface has focus at all - don't steal from Wayland surfaces
    if (!m_focused_surface) {
        focus_xwayland_surface(xsurface);
    }
}

void CompositorServer::handle_xwayland_surface_map(wlr_xwayland_surface* xsurface) {
    // Remove map listener - it's one-shot
    wl_list_remove(&m_listeners->xwayland_surface_map.link);
    wl_list_init(&m_listeners->xwayland_surface_map.link);

    // Only focus if no surface has focus - don't steal from Wayland surfaces
    if (!m_focused_surface) {
        focus_xwayland_surface(xsurface);
    }

    m_listeners->pending_xsurface = nullptr;
}

void CompositorServer::handle_xwayland_surface_destroy() {
    // Remove all XWayland-related listeners
    wl_list_remove(&m_listeners->xwayland_surface_destroy.link);
    wl_list_init(&m_listeners->xwayland_surface_destroy.link);
    wl_list_remove(&m_listeners->xwayland_surface_associate.link);
    wl_list_init(&m_listeners->xwayland_surface_associate.link);
    wl_list_remove(&m_listeners->xwayland_surface_map.link);
    wl_list_init(&m_listeners->xwayland_surface_map.link);

    // Clear focus if this was an XWayland surface
    if (m_focused_xsurface) {
        auto it = std::find(m_surfaces.begin(), m_surfaces.end(), m_focused_surface);
        if (it != m_surfaces.end()) {
            m_surfaces.erase(it);
        }

        m_focused_xsurface = nullptr;
        m_focused_surface = nullptr;
        wlr_seat_keyboard_clear_focus(m_seat);
        wlr_seat_pointer_clear_focus(m_seat);
    }

    m_listeners->pending_xsurface = nullptr;
}

void CompositorServer::handle_surface_destroy(wlr_surface* surface) {
    auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
    if (it != m_surfaces.end()) {
        m_surfaces.erase(it);
    }

    if (m_focused_surface == surface) {
        m_focused_surface = nullptr;
        wlr_seat_keyboard_clear_focus(m_seat);
        wlr_seat_pointer_clear_focus(m_seat);
    }
}

void CompositorServer::focus_surface(wlr_surface* surface) {
    if (m_focused_surface == surface) {
        return;
    }

    // Clear stale pointers BEFORE any wlroots calls that might access them
    // This prevents crashes when switching from XWayland to native Wayland
    m_focused_xsurface = nullptr;
    m_focused_surface = surface;

    // Clean up any pending XWayland listeners that might be dangling
    wl_list_remove(&m_listeners->xwayland_surface_associate.link);
    wl_list_init(&m_listeners->xwayland_surface_associate.link);
    wl_list_remove(&m_listeners->xwayland_surface_map.link);
    wl_list_init(&m_listeners->xwayland_surface_map.link);
    m_listeners->pending_xsurface = nullptr;

    wlr_seat_set_keyboard(m_seat, m_keyboard);
    wlr_seat_keyboard_notify_enter(m_seat, surface, m_keyboard->keycodes, m_keyboard->num_keycodes,
                                   &m_keyboard->modifiers);
    wlr_seat_pointer_notify_enter(m_seat, surface, 0.0, 0.0);
}

void CompositorServer::focus_xwayland_surface(wlr_xwayland_surface* xsurface) {
    if (m_focused_xsurface == xsurface) {
        return;
    }

    // Clear seat focus first to prevent wlroots from sending leave events to stale surface
    wlr_seat_keyboard_clear_focus(m_seat);
    wlr_seat_pointer_clear_focus(m_seat);

    // Clean up any pending XDG listeners that might be dangling
    wl_list_remove(&m_listeners->xdg_surface_commit.link);
    wl_list_init(&m_listeners->xdg_surface_commit.link);
    wl_list_remove(&m_listeners->xdg_surface_map.link);
    wl_list_init(&m_listeners->xdg_surface_map.link);
    wl_list_remove(&m_listeners->xdg_surface_destroy.link);
    wl_list_init(&m_listeners->xdg_surface_destroy.link);
    m_listeners->pending_toplevel = nullptr;

    m_focused_xsurface = xsurface;
    m_focused_surface = xsurface->surface;

    // Activate the X11 window - required for wlr_xwm to send focus events
    wlr_xwayland_surface_activate(xsurface, true);

    wlr_seat_set_keyboard(m_seat, m_keyboard);
    wlr_seat_keyboard_notify_enter(m_seat, xsurface->surface, m_keyboard->keycodes,
                                   m_keyboard->num_keycodes, &m_keyboard->modifiers);
    wlr_seat_pointer_notify_enter(m_seat, xsurface->surface, 0.0, 0.0);
}

} // namespace goggles::input
