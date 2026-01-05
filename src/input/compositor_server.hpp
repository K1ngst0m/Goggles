#pragma once

#include <thread>
#include <util/error.hpp>
#include <util/queues.hpp>
#include <util/unique_fd.hpp>
#include <vector>

extern "C" {
struct wl_display;
struct wl_event_loop;
struct wl_event_source;
struct wl_listener;          // NOLINT(readability-identifier-naming)
struct wlr_backend;          // NOLINT(readability-identifier-naming)
struct wlr_compositor;       // NOLINT(readability-identifier-naming)
struct wlr_seat;             // NOLINT(readability-identifier-naming)
struct wlr_xwayland;         // NOLINT(readability-identifier-naming)
struct wlr_renderer;         // NOLINT(readability-identifier-naming)
struct wlr_allocator;        // NOLINT(readability-identifier-naming)
struct wlr_xdg_shell;        // NOLINT(readability-identifier-naming)
struct wlr_surface;          // NOLINT(readability-identifier-naming)
struct wlr_xdg_toplevel;     // NOLINT(readability-identifier-naming)
struct wlr_xwayland_surface; // NOLINT(readability-identifier-naming)
struct wlr_keyboard;         // NOLINT(readability-identifier-naming)
struct wlr_output;           // NOLINT(readability-identifier-naming)
struct wlr_output_layout;    // NOLINT(readability-identifier-naming)
struct xkb_context;          // NOLINT(readability-identifier-naming)
}

namespace goggles::input {

enum class InputEventType : std::uint8_t { key, pointer_motion, pointer_button, pointer_axis };

struct InputEvent {
    InputEventType type;
    uint32_t code;
    bool pressed;
    double x, y;
    double value;
    bool horizontal;
};

class CompositorServer {
public:
    CompositorServer();
    ~CompositorServer();

    CompositorServer(const CompositorServer&) = delete;
    CompositorServer& operator=(const CompositorServer&) = delete;
    CompositorServer(CompositorServer&&) = delete;
    CompositorServer& operator=(CompositorServer&&) = delete;

    [[nodiscard]] auto start() -> Result<int>;
    void stop();
    [[nodiscard]] auto x11_display() const -> std::string {
        return ":" + std::to_string(m_display_number);
    }
    [[nodiscard]] auto wayland_display() const -> std::string {
        return "wayland-" + std::to_string(m_display_number);
    }

    [[nodiscard]] auto inject_key(uint32_t linux_keycode, bool pressed) -> bool;
    [[nodiscard]] auto inject_pointer_motion(double sx, double sy) -> bool;
    [[nodiscard]] auto inject_pointer_button(uint32_t button, bool pressed) -> bool;
    [[nodiscard]] auto inject_pointer_axis(double value, bool horizontal) -> bool;

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
    struct wlr_keyboard* m_keyboard = nullptr;
    struct xkb_context* m_xkb_context = nullptr;
    struct wlr_output_layout* m_output_layout = nullptr;
    struct wlr_output* m_output = nullptr;

    struct wlr_surface* m_focused_surface = nullptr;
    struct wlr_xwayland_surface* m_focused_xsurface = nullptr;
    std::vector<wlr_surface*> m_surfaces;
    double m_last_pointer_x = 0.0;
    double m_last_pointer_y = 0.0;

    struct Listeners;
    std::unique_ptr<Listeners> m_listeners;

    util::UniqueFd m_event_fd;
    // Must be power-of-2 for lock-free modulo
    util::SPSCQueue<InputEvent> m_event_queue{64};
    struct wl_event_source* m_event_source = nullptr;

    std::jthread m_compositor_thread;
    int m_display_number = -1;

    void handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel);
    void handle_xdg_surface_commit();
    void handle_xdg_surface_map();
    void handle_xdg_surface_destroy();
    void handle_new_xwayland_surface(wlr_xwayland_surface* surface);
    void handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_map(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_destroy();
    void handle_surface_destroy(wlr_surface* surface);
    void process_input_events();
    void focus_surface(wlr_surface* surface);
    void focus_xwayland_surface(wlr_xwayland_surface* xsurface);
};

} // namespace goggles::input
