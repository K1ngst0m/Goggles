#include "compositor_server.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <util/logging.hpp>
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

// XWayland/wlroots emit stderr warnings (xkbcomp, event loop errors).
// Suppress at info+ levels; visible at debug/trace for troubleshooting.
class StderrSuppressor {
public:
    StderrSuppressor() {
        if (goggles::get_logger()->level() <= spdlog::level::debug) {
            return;
        }
        m_saved_stderr = dup(STDERR_FILENO);
        if (m_saved_stderr < 0) {
            return;
        }
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        } else {
            close(m_saved_stderr);
            m_saved_stderr = -1;
        }
    }
    ~StderrSuppressor() {
        if (m_saved_stderr >= 0) {
            dup2(m_saved_stderr, STDERR_FILENO);
            close(m_saved_stderr);
        }
    }
    StderrSuppressor(const StderrSuppressor&) = delete;
    StderrSuppressor& operator=(const StderrSuppressor&) = delete;

private:
    int m_saved_stderr = -1;
};

auto bind_wayland_socket(wl_display* display) -> Result<std::string> {
    for (int display_num = 0; display_num < 10; ++display_num) {
        std::array<char, 32> socket_name{};
        std::snprintf(socket_name.data(), socket_name.size(), "goggles-%d", display_num);
        if (wl_display_add_socket(display, socket_name.data()) == 0) {
            return std::string(socket_name.data());
        }
    }
    return make_error<std::string>(ErrorCode::input_init_failed,
                                   "No available goggles sockets (goggles-0..9 all bound)");
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
    struct XWaylandSurfaceHooks {
        Impl* impl = nullptr;
        wlr_xwayland_surface* xsurface = nullptr;
        wl_listener associate{};
        wl_listener commit{};
        wl_listener destroy{};
    };

    struct XdgToplevelHooks {
        Impl* impl = nullptr;
        wlr_xdg_toplevel* toplevel = nullptr;
        wlr_surface* surface = nullptr;
        bool sent_configure = false;
        bool acked_configure = false;
        bool mapped = false;

        wl_listener surface_commit{};
        wl_listener surface_map{};
        wl_listener surface_destroy{};
        wl_listener xdg_ack_configure{};
    };

    struct Listeners {
        Impl* impl = nullptr;

        wl_listener new_xdg_toplevel{};
        wl_listener new_xwayland_surface{};
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
    wlr_surface* keyboard_entered_surface = nullptr;
    wlr_surface* pointer_entered_surface = nullptr;
    double last_pointer_x = 0.0;
    double last_pointer_y = 0.0;
    bool xwayland_surface_detached = false;
    std::jthread compositor_thread;
    std::vector<wlr_surface*> surfaces;
    Listeners listeners;
    util::UniqueFd event_fd;
    std::string wayland_socket_name;

    Impl() { listeners.impl = this; }

    [[nodiscard]] auto setup_base_components() -> Result<void>;
    [[nodiscard]] auto create_allocator() -> Result<void>;
    [[nodiscard]] auto create_compositor() -> Result<void>;
    [[nodiscard]] auto create_output_layout() -> Result<void>;
    [[nodiscard]] auto setup_xdg_shell() -> Result<void>;
    [[nodiscard]] auto setup_input_devices() -> Result<void>;
    [[nodiscard]] auto setup_event_loop_fd() -> Result<void>;
    [[nodiscard]] auto setup_xwayland() -> Result<void>;
    [[nodiscard]] auto start_backend() -> Result<void>;
    [[nodiscard]] auto setup_output() -> Result<void>;
    void start_compositor_thread();

    void process_input_events();
    void handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel);
    void handle_xdg_surface_commit(XdgToplevelHooks* hooks);
    void handle_xdg_surface_map(XdgToplevelHooks* hooks);
    void handle_xdg_surface_destroy(XdgToplevelHooks* hooks);
    void handle_xdg_surface_ack_configure(XdgToplevelHooks* hooks);
    void handle_new_xwayland_surface(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_commit(XWaylandSurfaceHooks* hooks);
    void handle_xwayland_surface_destroy(wlr_xwayland_surface* xsurface);
    void focus_surface(wlr_surface* surface);
    void focus_xwayland_surface(wlr_xwayland_surface* xsurface);
};

CompositorServer::CompositorServer() : m_impl(std::make_unique<Impl>()) {}

CompositorServer::~CompositorServer() {
    stop();
}

auto CompositorServer::x11_display() const -> std::string {
    if (m_impl->xwayland && m_impl->xwayland->display_name) {
        return m_impl->xwayland->display_name;
    }
    return "";
}

auto CompositorServer::wayland_display() const -> std::string {
    return m_impl->wayland_socket_name;
}

auto CompositorServer::Impl::setup_base_components() -> Result<void> {
    display = wl_display_create();
    if (!display) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create Wayland display");
    }

    event_loop = wl_display_get_event_loop(display);
    if (!event_loop) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to get event loop");
    }

    backend = wlr_headless_backend_create(event_loop);
    if (!backend) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create headless backend");
    }

    renderer = wlr_renderer_autocreate(backend);
    if (!renderer) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create renderer");
    }

    if (!wlr_renderer_init_wl_display(renderer, display)) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to initialize renderer protocols");
    }

    return {};
}

auto CompositorServer::Impl::create_allocator() -> Result<void> {
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create allocator");
    }
    return {};
}

auto CompositorServer::Impl::create_compositor() -> Result<void> {
    compositor = wlr_compositor_create(display, 6, renderer);
    if (!compositor) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create compositor");
    }
    return {};
}

auto CompositorServer::Impl::create_output_layout() -> Result<void> {
    output_layout = wlr_output_layout_create(display);
    if (!output_layout) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create output layout");
    }
    return {};
}

auto CompositorServer::Impl::setup_xdg_shell() -> Result<void> {
    xdg_shell = wlr_xdg_shell_create(display, 3);
    if (!xdg_shell) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xdg-shell");
    }

    wl_list_init(&listeners.new_xdg_toplevel.link);
    listeners.new_xdg_toplevel.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_xdg_toplevel));
        list->impl->handle_new_xdg_toplevel(static_cast<wlr_xdg_toplevel*>(data));
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &listeners.new_xdg_toplevel);

    return {};
}

auto CompositorServer::Impl::setup_input_devices() -> Result<void> {
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

    keyboard = UniqueKeyboard(new wlr_keyboard{});
    wlr_keyboard_init(keyboard.get(), nullptr, "virtual-keyboard");
    wlr_keyboard_set_keymap(keyboard.get(), keymap);
    xkb_keymap_unref(keymap);

    wlr_seat_set_keyboard(seat, keyboard.get());

    return {};
}

auto CompositorServer::Impl::setup_event_loop_fd() -> Result<void> {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create eventfd");
    }
    event_fd = util::UniqueFd(efd);

    event_source = wl_event_loop_add_fd(
        event_loop, event_fd.get(), WL_EVENT_READABLE,
        [](int /*fd*/, uint32_t /*mask*/, void* data) -> int {
            auto* impl = static_cast<Impl*>(data);
            uint64_t val = 0;
            // eventfd guarantees 8-byte atomic read when readable
            (void)read(impl->event_fd.get(), &val, sizeof(val));
            impl->process_input_events();
            return 0;
        },
        this);

    if (!event_source) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to add eventfd to event loop");
    }

    return {};
}

auto CompositorServer::Impl::setup_xwayland() -> Result<void> {
    {
        StderrSuppressor suppress;
        xwayland = wlr_xwayland_create(display, compositor, false);
    }
    if (!xwayland) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create XWayland server");
    }

    listeners.new_xwayland_surface.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_xwayland_surface));
        list->impl->handle_new_xwayland_surface(static_cast<wlr_xwayland_surface*>(data));
    };
    wl_signal_add(&xwayland->events.new_surface, &listeners.new_xwayland_surface);

    // wlr_xwm translates seat events to X11 KeyPress/MotionNotify
    wlr_xwayland_set_seat(xwayland, seat);

    return {};
}

auto CompositorServer::Impl::start_backend() -> Result<void> {
    if (!wlr_backend_start(backend)) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to start wlroots backend");
    }
    return {};
}

auto CompositorServer::Impl::setup_output() -> Result<void> {
    // Create headless output for native Wayland clients
    output = wlr_headless_add_output(backend, 1920, 1080);
    if (!output) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create headless output");
    }
    wlr_output_init_render(output, allocator, renderer);
    wlr_output_layout_add_auto(output_layout, output);

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);

    return {};
}

void CompositorServer::Impl::start_compositor_thread() {
    compositor_thread = std::jthread([this] {
        StderrSuppressor suppress;
        wl_display_run(display);
    });
}

auto CompositorServer::start() -> Result<void> {
    auto& impl = *m_impl;
    auto cleanup_on_error = [this](void*) { stop(); };
    std::unique_ptr<void, decltype(cleanup_on_error)> guard(this, cleanup_on_error);

    GOGGLES_TRY(impl.setup_base_components());
    GOGGLES_TRY(impl.create_allocator());
    GOGGLES_TRY(impl.create_compositor());
    GOGGLES_TRY(impl.create_output_layout());
    GOGGLES_TRY(impl.setup_xdg_shell());
    GOGGLES_TRY(impl.setup_input_devices());
    GOGGLES_TRY(impl.setup_event_loop_fd());

    auto socket_result = bind_wayland_socket(impl.display);
    if (!socket_result) {
        return make_error<void>(socket_result.error().code, socket_result.error().message);
    }
    impl.wayland_socket_name = *socket_result;

    GOGGLES_TRY(impl.setup_xwayland());
    GOGGLES_TRY(impl.start_backend());
    GOGGLES_TRY(impl.setup_output());

    impl.start_compositor_thread();

    guard.release(); // NOLINT(bugprone-unused-return-value)
    return {};
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
    impl.keyboard_entered_surface = nullptr;
    impl.pointer_entered_surface = nullptr;

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
    impl.wayland_socket_name.clear();
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
            wlr_surface* target_surface = focused_surface;
            if (focused_xsurface) {
                target_surface = focused_xsurface->surface;
                if (!target_surface) {
                    if (!xwayland_surface_detached) {
                        GOGGLES_LOG_DEBUG("Dropping input: focused XWayland surface is dissociated "
                                          "(surface=null)");
                        xwayland_surface_detached = true;
                    }
                    break;
                }
                xwayland_surface_detached = false;
            }
            if (!target_surface) {
                break;
            }
            // XWayland quirk: wlr_xwm requires re-activation and keyboard re-entry before each
            // key event. Without this, X11 clients silently drop input after the first event.
            // Native Wayland clients maintain focus state correctly and only need enter on change.
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_set_keyboard(seat, keyboard.get());
                wlr_seat_keyboard_notify_enter(seat, target_surface, keyboard->keycodes,
                                               keyboard->num_keycodes, &keyboard->modifiers);
            } else if (keyboard_entered_surface != target_surface) {
                wlr_seat_set_keyboard(seat, keyboard.get());
                wlr_seat_keyboard_notify_enter(seat, target_surface, keyboard->keycodes,
                                               keyboard->num_keycodes, &keyboard->modifiers);
                keyboard_entered_surface = target_surface;
            }
            auto state =
                event.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
            wlr_seat_keyboard_notify_key(seat, time, event.code, state);
            break;
        }
        case InputEventType::pointer_motion: {
            wlr_surface* target_surface = focused_surface;
            if (focused_xsurface) {
                target_surface = focused_xsurface->surface;
                if (!target_surface) {
                    if (!xwayland_surface_detached) {
                        GOGGLES_LOG_DEBUG("Dropping input: focused XWayland surface is dissociated "
                                          "(surface=null)");
                        xwayland_surface_detached = true;
                    }
                    break;
                }
                xwayland_surface_detached = false;
            }
            if (!target_surface) {
                break;
            }
            // XWayland quirk: requires re-activation and pointer re-entry before each event
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_pointer_notify_enter(seat, target_surface, event.x, event.y);
            } else if (pointer_entered_surface != target_surface) {
                wlr_seat_pointer_notify_enter(seat, target_surface, event.x, event.y);
                pointer_entered_surface = target_surface;
            }
            wlr_seat_pointer_notify_motion(seat, time, event.x, event.y);
            wlr_seat_pointer_notify_frame(seat);
            last_pointer_x = event.x;
            last_pointer_y = event.y;
            break;
        }
        case InputEventType::pointer_button: {
            wlr_surface* target_surface = focused_surface;
            if (focused_xsurface) {
                target_surface = focused_xsurface->surface;
                if (!target_surface) {
                    if (!xwayland_surface_detached) {
                        GOGGLES_LOG_DEBUG("Dropping input: focused XWayland surface is dissociated "
                                          "(surface=null)");
                        xwayland_surface_detached = true;
                    }
                    break;
                }
                xwayland_surface_detached = false;
            }
            if (!target_surface) {
                break;
            }
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_pointer_notify_enter(seat, target_surface, last_pointer_x, last_pointer_y);
            } else if (pointer_entered_surface != target_surface) {
                wlr_seat_pointer_notify_enter(seat, target_surface, last_pointer_x, last_pointer_y);
                pointer_entered_surface = target_surface;
            }
            auto state =
                event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
            wlr_seat_pointer_notify_button(seat, time, event.code, state);
            wlr_seat_pointer_notify_frame(seat);
            break;
        }
        case InputEventType::pointer_axis: {
            wlr_surface* target_surface = focused_surface;
            if (focused_xsurface) {
                target_surface = focused_xsurface->surface;
                if (!target_surface) {
                    if (!xwayland_surface_detached) {
                        GOGGLES_LOG_DEBUG("Dropping input: focused XWayland surface is dissociated "
                                          "(surface=null)");
                        xwayland_surface_detached = true;
                    }
                    break;
                }
                xwayland_surface_detached = false;
            }
            if (!target_surface) {
                break;
            }
            if (focused_xsurface) {
                wlr_xwayland_surface_activate(focused_xsurface, true);
                wlr_seat_pointer_notify_enter(seat, target_surface, last_pointer_x, last_pointer_y);
            } else if (pointer_entered_surface != target_surface) {
                wlr_seat_pointer_notify_enter(seat, target_surface, last_pointer_x, last_pointer_y);
                pointer_entered_surface = target_surface;
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
    auto* hooks = new XdgToplevelHooks{};
    hooks->impl = this;
    hooks->toplevel = toplevel;
    hooks->surface = toplevel->base->surface;

    wl_list_init(&hooks->surface_commit.link);
    hooks->surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_commit));
        h->impl->handle_xdg_surface_commit(h);
    };
    wl_signal_add(&hooks->surface->events.commit, &hooks->surface_commit);

    wl_list_init(&hooks->xdg_ack_configure.link);
    hooks->xdg_ack_configure.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XdgToplevelHooks, xdg_ack_configure));
        h->impl->handle_xdg_surface_ack_configure(h);
    };
    wl_signal_add(&toplevel->base->events.ack_configure, &hooks->xdg_ack_configure);

    wl_list_init(&hooks->surface_map.link);
    hooks->surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_map));
        h->impl->handle_xdg_surface_map(h);
    };
    wl_signal_add(&hooks->surface->events.map, &hooks->surface_map);

    wl_list_init(&hooks->surface_destroy.link);
    hooks->surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_destroy));
        h->impl->handle_xdg_surface_destroy(h);
    };
    wl_signal_add(&hooks->surface->events.destroy, &hooks->surface_destroy);
}

void CompositorServer::Impl::handle_xdg_surface_commit(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || !hooks->toplevel->base->initialized) {
        return;
    }

    // Only do initial setup on first commit, but keep listening for all commits
    if (!hooks->sent_configure) {
        wlr_xdg_surface_schedule_configure(hooks->toplevel->base);
        hooks->sent_configure = true;
    }

    // Release buffer to allow swapchain image reuse
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->surface, &now);
}

void CompositorServer::Impl::handle_xdg_surface_ack_configure(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || hooks->acked_configure) {
        return;
    }

    hooks->acked_configure = true;

    wl_list_remove(&hooks->xdg_ack_configure.link);
    wl_list_init(&hooks->xdg_ack_configure.link);

    if (!hooks->sent_configure) {
        return;
    }

    if (!focused_surface || focused_xsurface) {
        wlr_xdg_toplevel_set_activated(hooks->toplevel, true);
        focus_surface(hooks->surface);
    }
}

void CompositorServer::Impl::handle_xdg_surface_map(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || hooks->mapped) {
        return;
    }

    hooks->mapped = true;

    wl_list_remove(&hooks->surface_map.link);
    wl_list_init(&hooks->surface_map.link);

    surfaces.push_back(hooks->surface);
}

void CompositorServer::Impl::handle_xdg_surface_destroy(XdgToplevelHooks* hooks) {
    wl_list_remove(&hooks->surface_destroy.link);
    wl_list_init(&hooks->surface_destroy.link);
    wl_list_remove(&hooks->surface_commit.link);
    wl_list_init(&hooks->surface_commit.link);
    wl_list_remove(&hooks->surface_map.link);
    wl_list_init(&hooks->surface_map.link);
    wl_list_remove(&hooks->xdg_ack_configure.link);
    wl_list_init(&hooks->xdg_ack_configure.link);

    if (!focused_xsurface && focused_surface == hooks->surface) {
        focused_surface = nullptr;
        keyboard_entered_surface = nullptr;
        pointer_entered_surface = nullptr;
        wlr_seat_keyboard_clear_focus(seat);
        wlr_seat_pointer_clear_focus(seat);
    }

    auto it = std::find(surfaces.begin(), surfaces.end(), hooks->surface);
    if (it != surfaces.end()) {
        surfaces.erase(it);
    }

    delete hooks;
}

void CompositorServer::Impl::handle_new_xwayland_surface(wlr_xwayland_surface* xsurface) {
    GOGGLES_LOG_DEBUG("New XWayland surface: window_id={} ptr={}",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface));

    auto* hooks = new XWaylandSurfaceHooks{};
    hooks->impl = this;
    hooks->xsurface = xsurface;

    wl_list_init(&hooks->associate.link);
    wl_list_init(&hooks->commit.link);
    wl_list_init(&hooks->destroy.link);

    hooks->associate.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XWaylandSurfaceHooks, associate));
        h->impl->handle_xwayland_surface_associate(h->xsurface);

        // Register commit listener now that surface is available
        if (h->xsurface->surface && h->commit.link.next == &h->commit.link) {
            h->commit.notify = [](wl_listener* l, void* /*data*/) {
                auto* hk = reinterpret_cast<XWaylandSurfaceHooks*>(
                    reinterpret_cast<char*>(l) - offsetof(XWaylandSurfaceHooks, commit));
                hk->impl->handle_xwayland_surface_commit(hk);
            };
            wl_signal_add(&h->xsurface->surface->events.commit, &h->commit);
        }
    };
    wl_signal_add(&xsurface->events.associate, &hooks->associate);

    hooks->destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                          offsetof(XWaylandSurfaceHooks, destroy));
        h->impl->handle_xwayland_surface_destroy(h->xsurface);
        wl_list_remove(&h->associate.link);
        if (h->commit.link.next != nullptr && h->commit.link.next != &h->commit.link) {
            wl_list_remove(&h->commit.link);
        }
        wl_list_remove(&h->destroy.link);
        delete h;
    };
    wl_signal_add(&xsurface->events.destroy, &hooks->destroy);
}

void CompositorServer::Impl::handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface) {
    if (!xsurface->surface) {
        return;
    }

    GOGGLES_LOG_DEBUG("XWayland surface associated: window_id={} ptr={} surface={} title='{}'",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface),
                      static_cast<void*>(xsurface->surface),
                      xsurface->title ? xsurface->title : "");

    if (xsurface->override_redirect) {
        return;
    }

    // NOTE: Do NOT add xsurface->surface to surfaces - we can't clean it up
    // when X11 client disconnects (destroy listener breaks X11 input), so it
    // would become a dangling pointer.

    // NOTE: Do NOT register destroy listener on xsurface->surface->events.destroy
    // It fires unexpectedly during normal operation, breaking X11 input entirely.

    // Focus XWayland surface if:
    // - No current focus, OR
    // - Current focus is XWayland (switch between XWayland surfaces safely)
    if (!focused_surface || focused_xsurface) {
        focus_xwayland_surface(xsurface);
    }
}

void CompositorServer::Impl::handle_xwayland_surface_commit(XWaylandSurfaceHooks* hooks) {
    if (!hooks->xsurface || !hooks->xsurface->surface) {
        return;
    }

    // Release buffer to allow swapchain image reuse
    // Without this, X11 clients block on vkQueuePresentKHR
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->xsurface->surface, &now);
}

void CompositorServer::Impl::handle_xwayland_surface_destroy(wlr_xwayland_surface* xsurface) {
    if (focused_xsurface != xsurface) {
        return;
    }

    GOGGLES_LOG_DEBUG("Focused XWayland surface destroyed: ptr={}", static_cast<void*>(xsurface));
    focused_xsurface = nullptr;
    focused_surface = nullptr;
    keyboard_entered_surface = nullptr;
    pointer_entered_surface = nullptr;
    xwayland_surface_detached = false;
    wlr_seat_keyboard_clear_focus(seat);
    wlr_seat_pointer_clear_focus(seat);
}

void CompositorServer::Impl::focus_surface(wlr_surface* surface) {
    if (focused_surface == surface) {
        return;
    }

    // Clear stale pointers BEFORE any wlroots calls that might access them
    // This prevents crashes when switching from XWayland to native Wayland
    focused_xsurface = nullptr;
    focused_surface = surface;

    // Clean up any pending XWayland state
    xwayland_surface_detached = false;

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes,
                                   &keyboard->modifiers);
    wlr_seat_pointer_notify_enter(seat, surface, 0.0, 0.0);
    keyboard_entered_surface = surface;
    pointer_entered_surface = surface;
}

void CompositorServer::Impl::focus_xwayland_surface(wlr_xwayland_surface* xsurface) {
    if (focused_xsurface == xsurface) {
        return;
    }

    xwayland_surface_detached = false;

    // Clear seat focus first to prevent wlroots from sending leave events to stale surface
    wlr_seat_keyboard_clear_focus(seat);
    wlr_seat_pointer_clear_focus(seat);
    keyboard_entered_surface = nullptr;
    pointer_entered_surface = nullptr;

    focused_xsurface = xsurface;
    focused_surface = xsurface->surface;

    GOGGLES_LOG_DEBUG("Focused XWayland: window_id={} ptr={} surface={} title='{}'",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface),
                      static_cast<void*>(xsurface->surface),
                      xsurface->title ? xsurface->title : "");

    // Activate the X11 window - required for wlr_xwm to send focus events
    wlr_xwayland_surface_activate(xsurface, true);

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, xsurface->surface, keyboard->keycodes,
                                   keyboard->num_keycodes, &keyboard->modifiers);
    wlr_seat_pointer_notify_enter(seat, xsurface->surface, 0.0, 0.0);
    keyboard_entered_surface = xsurface->surface;
    pointer_entered_surface = xsurface->surface;
}

} // namespace goggles::input
