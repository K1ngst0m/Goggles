#pragma once

#include <thread>
#include <util/error.hpp>

// Forward declarations for wlroots types (external library types use snake_case)
extern "C" {
struct wl_display;
struct wl_event_loop;
struct wlr_backend;    // NOLINT(readability-identifier-naming)
struct wlr_compositor; // NOLINT(readability-identifier-naming)
struct wlr_seat;       // NOLINT(readability-identifier-naming)
struct wlr_xwayland;   // NOLINT(readability-identifier-naming)
struct wlr_renderer;   // NOLINT(readability-identifier-naming)
struct wlr_allocator;  // NOLINT(readability-identifier-naming)
struct wlr_xdg_shell;  // NOLINT(readability-identifier-naming)
}

namespace goggles::input {

class XWaylandServer {
public:
    XWaylandServer();
    ~XWaylandServer();

    XWaylandServer(const XWaylandServer&) = delete;
    XWaylandServer& operator=(const XWaylandServer&) = delete;
    XWaylandServer(XWaylandServer&&) = delete;
    XWaylandServer& operator=(XWaylandServer&&) = delete;

    [[nodiscard]] auto start() -> Result<int>;
    void stop();
    [[nodiscard]] auto display_number() const -> int { return m_display_number; }

private:
    struct wl_display* m_display = nullptr;
    struct wl_event_loop* m_event_loop = nullptr;
    struct wlr_backend* m_backend = nullptr;
    struct wlr_renderer* m_renderer = nullptr;
    struct wlr_allocator* m_allocator = nullptr;
    struct wlr_compositor* m_compositor = nullptr;
    struct wlr_xdg_shell* m_xdg_shell = nullptr;
    struct wlr_seat* m_seat = nullptr;
    struct wlr_xwayland* m_xwayland = nullptr;
    std::jthread m_compositor_thread;
    int m_display_number = -1;
};

} // namespace goggles::input
