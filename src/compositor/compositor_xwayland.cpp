#include "compositor_state.hpp"

#include <algorithm>
#include <cstddef>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
// xwayland.h contains 'char *class' which conflicts with C++ keyword
#define class class_
#include <wlr/xwayland/xwayland.h>
#undef class
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::input {

namespace {

// XWayland/helper tools emit stderr warnings (xkbcomp, event loop errors).
// Suppress at info+ levels; wlroots logs use the project logger.
class ScopedXwaylandStderrSuppression {
public:
    ScopedXwaylandStderrSuppression() {
        if (goggles::get_logger()->level() <= spdlog::level::debug) {
            return;
        }

        m_saved_stderr = util::UniqueFd::dup_from(STDERR_FILENO);
        if (!m_saved_stderr.valid()) {
            return;
        }

        const int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd < 0) {
            m_saved_stderr = {};
            return;
        }

        if (dup2(null_fd, STDERR_FILENO) < 0) {
            close(null_fd);
            m_saved_stderr = {};
            return;
        }

        close(null_fd);
    }

    ~ScopedXwaylandStderrSuppression() {
        if (!m_saved_stderr.valid()) {
            return;
        }
        (void)dup2(m_saved_stderr.get(), STDERR_FILENO);
    }

    ScopedXwaylandStderrSuppression(const ScopedXwaylandStderrSuppression&) = delete;
    auto operator=(const ScopedXwaylandStderrSuppression&)
        -> ScopedXwaylandStderrSuppression& = delete;

private:
    util::UniqueFd m_saved_stderr;
};

void enter_wayland_keyboard_target(CompositorState& state, wlr_surface* target_surface) {
    if (state.keyboard_entered_surface == target_surface) {
        return;
    }
    wlr_seat_set_keyboard(state.seat, state.keyboard.get());
    wlr_seat_keyboard_notify_enter(state.seat, target_surface, state.keyboard->keycodes,
                                   state.keyboard->num_keycodes, &state.keyboard->modifiers);
    state.keyboard_entered_surface = target_surface;
}

void enter_wayland_pointer_target(CompositorState& state, wlr_surface* target_surface,
                                  double local_x, double local_y) {
    if (state.pointer_entered_surface == target_surface) {
        return;
    }
    wlr_seat_pointer_notify_enter(state.seat, target_surface, local_x, local_y);
    state.pointer_entered_surface = target_surface;
}

void reactivate_xwayland_keyboard_target(CompositorState& state, const InputTarget& target) {
    // XWayland quirk: wlr_xwm requires re-activation and keyboard re-entry before each
    // key event. Without this, X11 clients silently drop input after the first event.
    wlr_xwayland_surface_activate(target.xsurface, true);
    wlr_seat_set_keyboard(state.seat, state.keyboard.get());
    wlr_seat_keyboard_notify_enter(state.seat, target.surface, state.keyboard->keycodes,
                                   state.keyboard->num_keycodes, &state.keyboard->modifiers);
}

void reactivate_xwayland_pointer_target(CompositorState& state, const InputTarget& target,
                                        double local_x, double local_y,
                                        bool skip_if_entered_surface_matches,
                                        bool track_entered_surface) {
    wlr_xwayland_surface_activate(target.xsurface, true);
    if (skip_if_entered_surface_matches && state.pointer_entered_surface == target.surface) {
        return;
    }
    wlr_seat_pointer_notify_enter(state.seat, target.surface, local_x, local_y);
    if (track_entered_surface) {
        state.pointer_entered_surface = target.surface;
    }
}

} // namespace

auto CompositorState::setup_xwayland() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    {
        ScopedXwaylandStderrSuppression suppress_stderr;
        xwayland = wlr_xwayland_create(display, compositor, false);
    }
    if (!xwayland) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create XWayland server");
    }

    wl_list_init(&listeners.new_xwayland_surface.link);
    listeners.new_xwayland_surface.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                  offsetof(Listeners, new_xwayland_surface));
        list->state->handle_new_xwayland_surface(static_cast<wlr_xwayland_surface*>(data));
    };
    wl_signal_add(&xwayland->events.new_surface, &listeners.new_xwayland_surface);

    // wlr_xwm translates seat events to X11 KeyPress/MotionNotify
    wlr_xwayland_set_seat(xwayland, seat);

    return {};
}

auto CompositorState::x11_display_name() const -> std::string {
    if (xwayland && xwayland->display_name) {
        return xwayland->display_name;
    }
    return "";
}

void CompositorState::run_compositor_display_loop() {
    ScopedXwaylandStderrSuppression suppress_stderr;
    wl_display_run(display);
}

void CompositorState::prepare_keyboard_dispatch(const InputTarget& target) {
    if (!target.surface) {
        return;
    }
    if (target.xsurface) {
        reactivate_xwayland_keyboard_target(*this, target);
        return;
    }
    enter_wayland_keyboard_target(*this, target.surface);
}

void CompositorState::prepare_pointer_motion_dispatch(const InputTarget& target, double local_x,
                                                      double local_y) {
    if (!target.surface) {
        return;
    }
    if (target.xsurface) {
        reactivate_xwayland_pointer_target(*this, target, local_x, local_y, false, false);
        return;
    }
    enter_wayland_pointer_target(*this, target.surface, local_x, local_y);
}

void CompositorState::prepare_pointer_button_dispatch(const InputTarget& target, double local_x,
                                                      double local_y) {
    if (!target.surface) {
        return;
    }
    if (target.xsurface) {
        reactivate_xwayland_pointer_target(*this, target, local_x, local_y, true, true);
        return;
    }
    enter_wayland_pointer_target(*this, target.surface, local_x, local_y);
}

void CompositorState::prepare_pointer_axis_dispatch(const InputTarget& target, double local_x,
                                                    double local_y) {
    if (!target.surface) {
        return;
    }
    if (target.xsurface) {
        reactivate_xwayland_pointer_target(*this, target, local_x, local_y, false, false);
        return;
    }
    enter_wayland_pointer_target(*this, target.surface, local_x, local_y);
}

void CompositorState::handle_new_xwayland_surface(wlr_xwayland_surface* xsurface) {
    GOGGLES_LOG_DEBUG("New XWayland surface: window_id={} ptr={}",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface));

    // Stable hook allocation keeps wl_listener container_of recovery valid after container growth.
    auto hooks = std::make_unique<XWaylandSurfaceHooks>();
    auto* hooks_ptr = hooks.get();
    hooks_ptr->state = this;
    hooks_ptr->xsurface = xsurface;
    hooks_ptr->id = next_surface_id++;
    hooks_ptr->override_redirect = xsurface->override_redirect;
    {
        std::scoped_lock lock(hooks_mutex);
        xwayland_hooks.push_back(std::move(hooks));
    }

    wl_list_init(&hooks_ptr->associate.link);
    wl_list_init(&hooks_ptr->map_request.link);
    wl_list_init(&hooks_ptr->commit.link);
    wl_list_init(&hooks_ptr->destroy.link);

    hooks_ptr->associate.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XWaylandSurfaceHooks, associate));
        h->state->handle_xwayland_surface_associate(h->xsurface);

        if (h->xsurface->surface && h->commit.link.next == &h->commit.link) {
            h->commit.notify = [](wl_listener* hook_listener, void* /*data*/) {
                auto* hook =
                    reinterpret_cast<XWaylandSurfaceHooks*>(reinterpret_cast<char*>(hook_listener) -
                                                            offsetof(XWaylandSurfaceHooks, commit));
                hook->state->handle_xwayland_surface_commit(hook);
            };
            wl_signal_add(&h->xsurface->surface->events.commit, &h->commit);
        }
    };
    wl_signal_add(&xsurface->events.associate, &hooks_ptr->associate);

    wl_list_init(&hooks_ptr->dissociate.link);
    hooks_ptr->dissociate.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XWaylandSurfaceHooks, dissociate));
        if (h->override_redirect && h->mapped) {
            h->mapped = false;
            h->state->request_present_reset();
        }
        // Stale commit events after dissociation would dereference the now-invalid wlr_surface.
        if (h->commit.link.next != nullptr && h->commit.link.next != &h->commit.link) {
            wl_list_remove(&h->commit.link);
            wl_list_init(&h->commit.link);
        }
    };
    wl_signal_add(&xsurface->events.dissociate, &hooks_ptr->dissociate);

    hooks_ptr->map_request.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XWaylandSurfaceHooks, map_request));
        h->state->handle_xwayland_surface_map_request(h);
    };
    wl_signal_add(&xsurface->events.map_request, &hooks_ptr->map_request);

    hooks_ptr->destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                          offsetof(XWaylandSurfaceHooks, destroy));
        wl_list_remove(&h->associate.link);
        wl_list_remove(&h->dissociate.link);
        wl_list_remove(&h->map_request.link);
        if (h->commit.link.next != nullptr && h->commit.link.next != &h->commit.link) {
            wl_list_remove(&h->commit.link);
        }
        wl_list_remove(&h->destroy.link);
        h->state->handle_xwayland_surface_destroy(h->xsurface);
    };
    wl_signal_add(&xsurface->events.destroy, &hooks_ptr->destroy);
}

void CompositorState::handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface) {
    if (!xsurface->surface) {
        return;
    }

    XWaylandSurfaceHooks* hooks = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        auto hook_it =
            std::find_if(xwayland_hooks.begin(), xwayland_hooks.end(),
                         [xsurface](const auto& entry) { return entry->xsurface == xsurface; });
        if (hook_it != xwayland_hooks.end()) {
            hooks = hook_it->get();
            hooks->title = xsurface->title ? xsurface->title : "";
            hooks->class_name = xsurface->class_ ? xsurface->class_ : "";
        }
    }

    GOGGLES_LOG_DEBUG("XWayland surface associated: window_id={} ptr={} surface={} title='{}'",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface),
                      static_cast<void*>(xsurface->surface),
                      xsurface->title ? xsurface->title : "");

    // NOTE: Do NOT register destroy listener on xsurface->surface->events.destroy
    // It fires unexpectedly during normal operation, breaking X11 input entirely.

    // XWayland events can arrive out-of-order (map_request before associate).
    if (hooks && !hooks->mapped) {
        if (hooks->override_redirect) {
            // Override-redirect windows bypass the WM: events.map_request never fires
            // for them (X11 sends MapNotify, not MapRequest). Treat association as mapped.
            hooks->mapped = true;
            request_present_reset();
        } else if (hooks->map_requested) {
            hooks->mapped = true;
            focus_xwayland_surface(xsurface);
        }
    }
}

void CompositorState::handle_xwayland_surface_map_request(XWaylandSurfaceHooks* hooks) {
    if (!hooks || !hooks->xsurface) {
        return;
    }
    auto* xsurface = hooks->xsurface;

    hooks->map_requested = true;
    if (hooks->override_redirect) {
        GOGGLES_LOG_DEBUG("XWayland override-redirect map request: window_id={} ptr={}",
                          static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface));
    }
    if (!xsurface->surface) {
        // Wait for associate: wlroots will set xsurface->surface later.
        return;
    }

    hooks->mapped = true;

    if (!hooks->override_redirect) {
        focus_xwayland_surface(xsurface);
    } else {
        request_present_reset();
    }
}

void CompositorState::handle_xwayland_surface_commit(XWaylandSurfaceHooks* hooks) {
    if (!hooks->xsurface || !hooks->xsurface->surface) {
        return;
    }

    // Release buffer to allow swapchain image reuse
    // Without this, X11 clients block on vkQueuePresentKHR
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->xsurface->surface, &now);

    if (hooks->mapped) {
        if (hooks->override_redirect) {
            request_present_reset();
        } else {
            update_presented_frame(hooks->xsurface->surface);
        }
    }
}

void CompositorState::handle_xwayland_surface_destroy(wlr_xwayland_surface* xsurface) {
    auto* surface = xsurface ? xsurface->surface : nullptr;

    if (xsurface && xsurface->override_redirect) {
        GOGGLES_LOG_DEBUG("XWayland override-redirect destroyed: window_id={} ptr={}",
                          static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface));
    }

    bool clear_focus = false;
    {
        std::scoped_lock lock(hooks_mutex);
        if (focused_xsurface == xsurface) {
            focused_xsurface = nullptr;
            focused_surface = nullptr;
            clear_focus = true;
        }

        auto hook_it =
            std::find_if(xwayland_hooks.begin(), xwayland_hooks.end(),
                         [xsurface](const auto& entry) { return entry->xsurface == xsurface; });
        if (hook_it != xwayland_hooks.end()) {
            xwayland_hooks.erase(hook_it);
        }
    }

    if (keyboard_entered_surface == surface) {
        keyboard_entered_surface = nullptr;
    }
    if (pointer_entered_surface == surface) {
        pointer_entered_surface = nullptr;
    }

    if (presented_surface == surface) {
        clear_presented_frame();
    }

    if (xsurface && xsurface->override_redirect) {
        request_present_reset();
    }

    if (!clear_focus) {
        return;
    }

    GOGGLES_LOG_DEBUG("Focused XWayland surface destroyed: ptr={}", static_cast<void*>(xsurface));
    deactivate_constraint();
    keyboard_entered_surface = nullptr;
    pointer_entered_surface = nullptr;
    cursor_surface = nullptr;
    cursor_initialized = false;
    wlr_seat_keyboard_clear_focus(seat);
    wlr_seat_pointer_clear_focus(seat);
    while (event_queue.try_pop()) {
    }
    auto_focus_next_surface();
}

} // namespace goggles::input
