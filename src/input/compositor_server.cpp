#include "compositor_server.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <linux/input-event-codes.h>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <util/queues.hpp>
#include <util/unique_fd.hpp>
#include <vector>

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

struct KeyboardDeleter {
    void operator()(wlr_keyboard* kb) const {
        if (kb) {
            wlr_keyboard_finish(kb);
            delete kb;
        }
    }
};
using UniqueKeyboard = std::unique_ptr<wlr_keyboard, KeyboardDeleter>;

} // anonymous namespace

struct CompositorServer::Impl {
    struct Listeners {
        Impl* impl = nullptr;

        wl_listener new_xdg_toplevel{};
        wl_listener xdg_surface_commit{};
        wl_listener xdg_surface_map{};
        wl_listener xdg_surface_destroy{};
        wl_listener new_xwayland_surface{};
        wl_listener xwayland_surface_associate{};
        wl_listener xwayland_surface_map{};
        wl_listener xwayland_surface_destroy{};
    };

    // Fields ordered for optimal padding
    util::SPSCQueue<InputEvent> event_queue{64};
    wl_display* display = nullptr;
    wl_event_loop* event_loop = nullptr;
    wl_event_source* event_source = nullptr;
    wlr_backend* backend = nullptr;
    wlr_renderer* renderer = nullptr;
    wlr_allocator* allocator = nullptr;
    wlr_compositor* compositor = nullptr;
    wlr_xdg_shell* xdg_shell = nullptr;
    wlr_seat* seat = nullptr;
    wlr_xwayland* xwayland = nullptr;
    UniqueKeyboard keyboard;
    xkb_context* xkb_ctx = nullptr;
    wlr_output_layout* output_layout = nullptr;
    wlr_output* output = nullptr;
    wlr_surface* focused_surface = nullptr;
    wlr_xwayland_surface* focused_xsurface = nullptr;
    double last_pointer_x = 0.0;
    double last_pointer_y = 0.0;
    wlr_xdg_toplevel* pending_toplevel = nullptr;
    wlr_xwayland_surface* pending_xsurface = nullptr;
    std::jthread compositor_thread;
    std::vector<wlr_surface*> surfaces;
    Listeners listeners;
    util::UniqueFd event_fd;
    int display_number = -1;

    Impl() {
        listeners.impl = this;
        wl_list_init(&listeners.xdg_surface_commit.link);
        wl_list_init(&listeners.xdg_surface_map.link);
        wl_list_init(&listeners.xdg_surface_destroy.link);
        wl_list_init(&listeners.xwayland_surface_associate.link);
        wl_list_init(&listeners.xwayland_surface_map.link);
        wl_list_init(&listeners.xwayland_surface_destroy.link);
    }

    void process_input_events();
    void handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel);
    void handle_xdg_surface_commit();
    void handle_xdg_surface_map();
    void handle_xdg_surface_destroy();
    void handle_new_xwayland_surface(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_map(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_destroy();
    void focus_surface(wlr_surface* surface);
    void focus_xwayland_surface(wlr_xwayland_surface* xsurface);
};

CompositorServer::CompositorServer() : m_impl(std::make_unique<Impl>()) {}

CompositorServer::~CompositorServer() {
    stop();
}

auto CompositorServer::x11_display() const -> std::string {
    return ":" + std::to_string(m_impl->display_number);
}

auto CompositorServer::wayland_display() const -> std::string {
    return "wayland-" + std::to_string(m_impl->display_number);
}

auto CompositorServer::start() -> Result<int> {
    auto& impl = *m_impl;
    auto cleanup_on_error = [this](void*) { stop(); };
    std::unique_ptr<void, decltype(cleanup_on_error)> guard(this, cleanup_on_error);

    impl.display = wl_display_create();
    if (!impl.display) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create Wayland display");
    }

    impl.event_loop = wl_display_get_event_loop(impl.display);
    if (!impl.event_loop) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to get event loop");
    }

    impl.backend = wlr_headless_backend_create(impl.event_loop);
    if (!impl.backend) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create headless backend");
    }

    impl.renderer = wlr_renderer_autocreate(impl.backend);
    if (!impl.renderer) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create renderer");
    }

    if (!wlr_renderer_init_wl_display(impl.renderer, impl.display)) {
        return make_error<int>(ErrorCode::input_init_failed,
                               "Failed to initialize renderer protocols");
    }

    impl.allocator = wlr_allocator_autocreate(impl.backend, impl.renderer);
    if (!impl.allocator) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create allocator");
    }

    impl.compositor = wlr_compositor_create(impl.display, 6, impl.renderer);
    if (!impl.compositor) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create compositor");
    }

    impl.output_layout = wlr_output_layout_create(impl.display);
    if (!impl.output_layout) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create output layout");
    }

    impl.xdg_shell = wlr_xdg_shell_create(impl.display, 3);
    if (!impl.xdg_shell) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xdg-shell");
    }

    wl_list_init(&impl.listeners.new_xdg_toplevel.link);
    impl.listeners.new_xdg_toplevel.notify = [](wl_listener* listener, void* data) {
        auto* listeners = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_xdg_toplevel));
        listeners->impl->handle_new_xdg_toplevel(static_cast<wlr_xdg_toplevel*>(data));
    };
    wl_signal_add(&impl.xdg_shell->events.new_toplevel, &impl.listeners.new_xdg_toplevel);

    impl.seat = wlr_seat_create(impl.display, "seat0");
    if (!impl.seat) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create seat");
    }

    wlr_seat_set_capabilities(impl.seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    impl.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!impl.xkb_ctx) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xkb context");
    }

    xkb_keymap* keymap =
        xkb_keymap_new_from_names(impl.xkb_ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xkb keymap");
    }

    impl.keyboard = UniqueKeyboard(new wlr_keyboard{});
    wlr_keyboard_init(impl.keyboard.get(), nullptr, "virtual-keyboard");
    wlr_keyboard_set_keymap(impl.keyboard.get(), keymap);
    xkb_keymap_unref(keymap);

    wlr_seat_set_keyboard(impl.seat, impl.keyboard.get());

    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create eventfd");
    }
    impl.event_fd = util::UniqueFd(efd);

    impl.event_source = wl_event_loop_add_fd(
        impl.event_loop, impl.event_fd.get(), WL_EVENT_READABLE,
        [](int /*fd*/, uint32_t /*mask*/, void* data) -> int {
            auto* self = static_cast<Impl*>(data);
            uint64_t val = 0;
            // eventfd guarantees 8-byte atomic read when readable
            (void)read(self->event_fd.get(), &val, sizeof(val));
            self->process_input_events();
            return 0;
        },
        &impl);

    if (!impl.event_source) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to add eventfd to event loop");
    }

    auto socket_result = bind_wayland_socket(impl.display);
    if (!socket_result) {
        return make_error<int>(socket_result.error().code, socket_result.error().message);
    }
    impl.display_number = *socket_result;

    impl.xwayland = wlr_xwayland_create(impl.display, impl.compositor, false);
    if (!impl.xwayland) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create XWayland server");
    }

    impl.listeners.new_xwayland_surface.notify = [](wl_listener* listener, void* data) {
        auto* listeners = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_xwayland_surface));
        listeners->impl->handle_new_xwayland_surface(static_cast<wlr_xwayland_surface*>(data));
    };
    wl_signal_add(&impl.xwayland->events.new_surface, &impl.listeners.new_xwayland_surface);

    // wlr_xwm translates seat events to X11 KeyPress/MotionNotify
    wlr_xwayland_set_seat(impl.xwayland, impl.seat);

    if (!wlr_backend_start(impl.backend)) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to start wlroots backend");
    }

    // Create headless output for native Wayland clients
    impl.output = wlr_headless_add_output(impl.backend, 1920, 1080);
    if (!impl.output) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create headless output");
    }
    wlr_output_init_render(impl.output, impl.allocator, impl.renderer);
    wlr_output_layout_add_auto(impl.output_layout, impl.output);

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_commit_state(impl.output, &state);
    wlr_output_state_finish(&state);

    impl.compositor_thread = std::jthread([&impl] { wl_display_run(impl.display); });

    guard.release(); // NOLINT(bugprone-unused-return-value)
    return impl.display_number;
}

void CompositorServer::stop() {
    auto& impl = *m_impl;

    if (!impl.display) {
        return;
    }

    wl_display_terminate(impl.display);

    // Must join before destroying objects thread accesses
    if (impl.compositor_thread.joinable()) {
        impl.compositor_thread.join();
    }

    // Must remove before closing eventfd
    if (impl.event_source) {
        wl_event_source_remove(impl.event_source);
        impl.event_source = nullptr;
    }

    impl.surfaces.clear();
    impl.focused_surface = nullptr;
    impl.focused_xsurface = nullptr;

    // Destruction order: xwayland before compositor, seat before display
    if (impl.xwayland) {
        wlr_xwayland_destroy(impl.xwayland);
        impl.xwayland = nullptr;
    }

    if (impl.keyboard) {
        impl.keyboard.reset();
    }

    if (impl.xkb_ctx) {
        xkb_context_unref(impl.xkb_ctx);
        impl.xkb_ctx = nullptr;
    }

    if (impl.seat) {
        wlr_seat_destroy(impl.seat);
        impl.seat = nullptr;
    }

    impl.xdg_shell = nullptr;
    impl.compositor = nullptr;
    impl.output = nullptr;

    if (impl.output_layout) {
        wlr_output_layout_destroy(impl.output_layout);
        impl.output_layout = nullptr;
    }

    if (impl.allocator) {
        wlr_allocator_destroy(impl.allocator);
        impl.allocator = nullptr;
    }

    if (impl.renderer) {
        wlr_renderer_destroy(impl.renderer);
        impl.renderer = nullptr;
    }

    if (impl.backend) {
        wlr_backend_destroy(impl.backend);
        impl.backend = nullptr;
    }

    if (impl.display) {
        wl_display_destroy(impl.display);
        impl.display = nullptr;
    }

    impl.event_loop = nullptr;
    impl.display_number = -1;
}

auto CompositorServer::inject_key(uint32_t linux_keycode, bool pressed) -> bool {
    InputEvent event{};
    event.type = InputEventType::key;
    event.code = linux_keycode;
    event.pressed = pressed;

    if (m_impl->event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_impl->event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

auto CompositorServer::inject_pointer_motion(double sx, double sy) -> bool {
    InputEvent event{};
    event.type = InputEventType::pointer_motion;
    event.x = sx;
    event.y = sy;

    if (m_impl->event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_impl->event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

auto CompositorServer::inject_pointer_button(uint32_t button, bool pressed) -> bool {
    InputEvent event{};
    event.type = InputEventType::pointer_button;
    event.code = button;
    event.pressed = pressed;

    if (m_impl->event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_impl->event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

auto CompositorServer::inject_pointer_axis(double value, bool horizontal) -> bool {
    InputEvent event{};
    event.type = InputEventType::pointer_axis;
    event.value = value;
    event.horizontal = horizontal;

    if (m_impl->event_queue.try_push(event)) {
        uint64_t val = 1;
        auto ret = write(m_impl->event_fd.get(), &val, sizeof(val));
        return ret == sizeof(val);
    }
    return false;
}

void CompositorServer::Impl::process_input_events() {
    while (auto event_opt = event_queue.try_pop()) {
        auto& event = *event_opt;
        uint32_t time = get_time_msec();

        switch (event.type) {
        case InputEventType::key: {
            if (!focused_surface) {
                break;
            }
            // For XWayland: must re-activate and re-enter keyboard focus before each key
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
            }
            wlr_seat_set_keyboard(seat, keyboard.get());
            wlr_seat_keyboard_notify_enter(seat, focused_surface, keyboard->keycodes,
                                           keyboard->num_keycodes, &keyboard->modifiers);
            auto state =
                event.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
            wlr_seat_keyboard_notify_key(seat, time, event.code, state);
            break;
        }
        case InputEventType::pointer_motion: {
            if (!focused_surface) {
                break;
            }
            // For XWayland: must re-activate and re-enter pointer focus
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_pointer_notify_enter(seat, focused_surface, event.x, event.y);
            }
            wlr_seat_pointer_notify_motion(seat, time, event.x, event.y);
            wlr_seat_pointer_notify_frame(seat);
            last_pointer_x = event.x;
            last_pointer_y = event.y;
            break;
        }
        case InputEventType::pointer_button: {
            if (!focused_surface) {
                break;
            }
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_pointer_notify_enter(seat, focused_surface, last_pointer_x,
                                              last_pointer_y);
            }
            auto state =
                event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
            wlr_seat_pointer_notify_button(seat, time, event.code, state);
            wlr_seat_pointer_notify_frame(seat);
            break;
        }
        case InputEventType::pointer_axis: {
            if (!focused_surface) {
                break;
            }
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_pointer_notify_enter(seat, focused_surface, last_pointer_x,
                                              last_pointer_y);
            }
            auto orientation = event.horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL
                                                : WL_POINTER_AXIS_VERTICAL_SCROLL;
            wlr_seat_pointer_notify_axis(seat, time, orientation, event.value,
                                         0, // value_discrete (legacy)
                                         WL_POINTER_AXIS_SOURCE_WHEEL,
                                         WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
            wlr_seat_pointer_notify_frame(seat);
            break;
        }
        }
    }
}

void CompositorServer::Impl::handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel) {
    // Skip if already waiting for a surface
    if (!wl_list_empty(&listeners.xdg_surface_commit.link)) {
        return;
    }

    pending_toplevel = toplevel;

    // Stage 1: Listen for commit to detect when surface becomes initialized
    wl_list_init(&listeners.xdg_surface_commit.link);
    listeners.xdg_surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* l = reinterpret_cast<Impl::Listeners*>(reinterpret_cast<char*>(listener) -
                                                     offsetof(Impl::Listeners, xdg_surface_commit));
        l->impl->handle_xdg_surface_commit();
    };
    wl_signal_add(&toplevel->base->surface->events.commit, &listeners.xdg_surface_commit);
}

void CompositorServer::Impl::handle_xdg_surface_commit() {
    if (!pending_toplevel || !pending_toplevel->base->initialized) {
        return;
    }

    // Remove commit listener - we only need it once
    wl_list_remove(&listeners.xdg_surface_commit.link);
    wl_list_init(&listeners.xdg_surface_commit.link);

    wlr_xdg_toplevel_set_activated(pending_toplevel, true);
    wlr_xdg_surface_schedule_configure(pending_toplevel->base);

    // Stage 2: Listen for map signal (buffer committed)
    wl_list_init(&listeners.xdg_surface_map.link);
    listeners.xdg_surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* l = reinterpret_cast<Impl::Listeners*>(reinterpret_cast<char*>(listener) -
                                                     offsetof(Impl::Listeners, xdg_surface_map));
        l->impl->handle_xdg_surface_map();
    };
    wl_signal_add(&pending_toplevel->base->surface->events.map, &listeners.xdg_surface_map);
}

void CompositorServer::Impl::handle_xdg_surface_map() {
    if (!pending_toplevel) {
        return;
    }

    wl_list_remove(&listeners.xdg_surface_map.link);
    wl_list_init(&listeners.xdg_surface_map.link);

    wlr_surface* surface = pending_toplevel->base->surface;
    surfaces.push_back(surface);

    wl_list_remove(&listeners.xdg_surface_destroy.link);
    wl_list_init(&listeners.xdg_surface_destroy.link);
    listeners.xdg_surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* l = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, xdg_surface_destroy));
        l->impl->handle_xdg_surface_destroy();
    };
    wl_signal_add(&surface->events.destroy, &listeners.xdg_surface_destroy);

    // Focus new Wayland surface if:
    // - No current focus, OR
    // - Current focus is XWayland (might be stale - can't detect X11 disconnect)
    if (!focused_surface || focused_xsurface) {
        focus_surface(surface);
    }

    pending_toplevel = nullptr;
}

void CompositorServer::Impl::handle_xdg_surface_destroy() {
    wl_list_remove(&listeners.xdg_surface_destroy.link);
    wl_list_init(&listeners.xdg_surface_destroy.link);
    wl_list_remove(&listeners.xdg_surface_commit.link);
    wl_list_init(&listeners.xdg_surface_commit.link);
    wl_list_remove(&listeners.xdg_surface_map.link);
    wl_list_init(&listeners.xdg_surface_map.link);

    // Clear focus if this was a Wayland surface (not XWayland)
    if (focused_surface && !focused_xsurface) {
        auto it = std::find(surfaces.begin(), surfaces.end(), focused_surface);
        if (it != surfaces.end()) {
            surfaces.erase(it);
        }

        focused_surface = nullptr;
        wlr_seat_keyboard_clear_focus(seat);
        wlr_seat_pointer_clear_focus(seat);
    }

    pending_toplevel = nullptr;
}

void CompositorServer::Impl::handle_new_xwayland_surface(wlr_xwayland_surface* xsurface) {
    // Listener can only belong to one signal's list - skip if already registered
    if (!wl_list_empty(&listeners.xwayland_surface_associate.link)) {
        return;
    }

    // Store xsurface for retrieval in callback
    pending_xsurface = xsurface;

    listeners.xwayland_surface_associate.notify = [](wl_listener* listener, void* /*data*/) {
        auto* l = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) -
            offsetof(Impl::Listeners, xwayland_surface_associate));
        l->impl->handle_xwayland_surface_associate(l->impl->pending_xsurface);
    };
    wl_signal_add(&xsurface->events.associate, &listeners.xwayland_surface_associate);
}

void CompositorServer::Impl::handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface) {
    // Remove associate listener - it's one-shot
    wl_list_remove(&listeners.xwayland_surface_associate.link);
    wl_list_init(&listeners.xwayland_surface_associate.link);

    if (!xsurface->surface) {
        return;
    }

    // NOTE: Do NOT add xsurface->surface to surfaces - we can't clean it up
    // when X11 client disconnects (destroy listener breaks X11 input), so it
    // would become a dangling pointer.

    // NOTE: Do NOT register destroy listener on xsurface->surface->events.destroy
    // It fires unexpectedly during normal operation, breaking X11 input entirely.

    // Only focus if no surface has focus at all - don't steal from Wayland surfaces
    if (!focused_surface) {
        focus_xwayland_surface(xsurface);
    }
}

void CompositorServer::Impl::handle_xwayland_surface_map(wlr_xwayland_surface* xsurface) {
    // Remove map listener - it's one-shot
    wl_list_remove(&listeners.xwayland_surface_map.link);
    wl_list_init(&listeners.xwayland_surface_map.link);

    // Only focus if no surface has focus - don't steal from Wayland surfaces
    if (!focused_surface) {
        focus_xwayland_surface(xsurface);
    }

    pending_xsurface = nullptr;
}

void CompositorServer::Impl::handle_xwayland_surface_destroy() {
    wl_list_remove(&listeners.xwayland_surface_destroy.link);
    wl_list_init(&listeners.xwayland_surface_destroy.link);
    wl_list_remove(&listeners.xwayland_surface_associate.link);
    wl_list_init(&listeners.xwayland_surface_associate.link);
    wl_list_remove(&listeners.xwayland_surface_map.link);
    wl_list_init(&listeners.xwayland_surface_map.link);

    // Clear focus if this was an XWayland surface
    if (focused_xsurface) {
        auto it = std::find(surfaces.begin(), surfaces.end(), focused_surface);
        if (it != surfaces.end()) {
            surfaces.erase(it);
        }

        focused_xsurface = nullptr;
        focused_surface = nullptr;
        wlr_seat_keyboard_clear_focus(seat);
        wlr_seat_pointer_clear_focus(seat);
    }

    pending_xsurface = nullptr;
}

void CompositorServer::Impl::focus_surface(wlr_surface* surface) {
    if (focused_surface == surface) {
        return;
    }

    // Clear stale pointers BEFORE any wlroots calls that might access them
    // This prevents crashes when switching from XWayland to native Wayland
    focused_xsurface = nullptr;
    focused_surface = surface;

    // Clean up any pending XWayland listeners that might be dangling
    wl_list_remove(&listeners.xwayland_surface_associate.link);
    wl_list_init(&listeners.xwayland_surface_associate.link);
    wl_list_remove(&listeners.xwayland_surface_map.link);
    wl_list_init(&listeners.xwayland_surface_map.link);
    pending_xsurface = nullptr;

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes,
                                   &keyboard->modifiers);
    wlr_seat_pointer_notify_enter(seat, surface, 0.0, 0.0);
}

void CompositorServer::Impl::focus_xwayland_surface(wlr_xwayland_surface* xsurface) {
    if (focused_xsurface == xsurface) {
        return;
    }

    // Clear seat focus first to prevent wlroots from sending leave events to stale surface
    wlr_seat_keyboard_clear_focus(seat);
    wlr_seat_pointer_clear_focus(seat);

    // Clean up any pending XDG listeners that might be dangling
    wl_list_remove(&listeners.xdg_surface_commit.link);
    wl_list_init(&listeners.xdg_surface_commit.link);
    wl_list_remove(&listeners.xdg_surface_map.link);
    wl_list_init(&listeners.xdg_surface_map.link);
    wl_list_remove(&listeners.xdg_surface_destroy.link);
    wl_list_init(&listeners.xdg_surface_destroy.link);
    pending_toplevel = nullptr;

    focused_xsurface = xsurface;
    focused_surface = xsurface->surface;

    // Activate the X11 window - required for wlr_xwm to send focus events
    wlr_xwayland_surface_activate(xsurface, true);

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, xsurface->surface, keyboard->keycodes,
                                   keyboard->num_keycodes, &keyboard->modifiers);
    wlr_seat_pointer_notify_enter(seat, xsurface->surface, 0.0, 0.0);
}

} // namespace goggles::input
