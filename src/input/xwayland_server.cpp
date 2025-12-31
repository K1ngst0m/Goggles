#include "xwayland_server.hpp"

#include <array>
#include <cstdio>
#include <memory>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>

// xwayland.h contains 'char *class' which conflicts with C++ keyword
#define class class_
#include <wlr/xwayland/xwayland.h>
#undef class

struct wlr_xdg_shell;
struct wlr_xdg_shell* wlr_xdg_shell_create(struct wl_display*, uint32_t);
}

namespace goggles::input {

XWaylandServer::XWaylandServer() = default;

XWaylandServer::~XWaylandServer() {
    stop();
}

auto XWaylandServer::start() -> Result<int> {
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

    m_xdg_shell = wlr_xdg_shell_create(m_display, 3);
    if (!m_xdg_shell) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create xdg-shell");
    }

    m_seat = wlr_seat_create(m_display, "seat0");
    if (!m_seat) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create seat");
    }

    wlr_seat_set_capabilities(m_seat, WL_SEAT_CAPABILITY_KEYBOARD);

    bool socket_bound = false;
    for (int display_num = 1; display_num < 10; ++display_num) {
        std::array<char, 32> socket_name{};
        std::snprintf(socket_name.data(), socket_name.size(), "wayland-%d", display_num);

        int result = wl_display_add_socket(m_display, socket_name.data());
        if (result == 0) {
            m_display_number = display_num;
            socket_bound = true;
            break;
        }
    }

    if (!socket_bound) {
        return make_error<int>(ErrorCode::input_init_failed,
                               "No available DISPLAY numbers (1-9 all bound)");
    }

    m_xwayland = wlr_xwayland_create(m_display, m_compositor, false);
    if (!m_xwayland) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to create XWayland server");
    }

    if (!wlr_backend_start(m_backend)) {
        return make_error<int>(ErrorCode::input_init_failed, "Failed to start wlroots backend");
    }

    m_compositor_thread = std::jthread([this] { wl_display_run(m_display); });

    guard.release(); // NOLINT(bugprone-unused-return-value)
    return m_display_number;
}

void XWaylandServer::stop() {
    if (!m_display) {
        return;
    }

    wl_display_terminate(m_display);

    // Explicitly join compositor thread before destroying objects to prevent use-after-free
    if (m_compositor_thread.joinable()) {
        m_compositor_thread.join();
    }

    // Must be before compositor
    if (m_xwayland) {
        wlr_xwayland_destroy(m_xwayland);
        m_xwayland = nullptr;
    }

    // Must be before display
    if (m_seat) {
        wlr_seat_destroy(m_seat);
        m_seat = nullptr;
    }

    m_xdg_shell = nullptr;
    m_compositor = nullptr;

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

    m_event_loop = nullptr; // Part of display, already destroyed
    m_display_number = -1;
}

} // namespace goggles::input
