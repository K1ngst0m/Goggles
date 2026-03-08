#include "compositor_state.hpp"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <memory>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
}

#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::input {

auto CompositorState::setup_xdg_shell() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    xdg_shell = wlr_xdg_shell_create(display, 3);
    if (!xdg_shell) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xdg-shell");
    }

    wl_list_init(&listeners.new_xdg_toplevel.link);
    listeners.new_xdg_toplevel.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                  offsetof(Listeners, new_xdg_toplevel));
        list->state->handle_new_xdg_toplevel(static_cast<wlr_xdg_toplevel*>(data));
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &listeners.new_xdg_toplevel);

    wl_list_init(&listeners.new_xdg_popup.link);
    listeners.new_xdg_popup.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                  offsetof(Listeners, new_xdg_popup));
        list->state->handle_new_xdg_popup(static_cast<wlr_xdg_popup*>(data));
    };
    wl_signal_add(&xdg_shell->events.new_popup, &listeners.new_xdg_popup);

    return {};
}

void CompositorState::handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel) {
    if (!toplevel || !toplevel->base) {
        return;
    }

    GOGGLES_LOG_DEBUG("New XDG toplevel: toplevel={} surface={} title='{}' app_id='{}'",
                      static_cast<void*>(toplevel), static_cast<void*>(toplevel->base->surface),
                      toplevel->title ? toplevel->title : "",
                      toplevel->app_id ? toplevel->app_id : "");

    // Stable hook allocation keeps wl_listener container_of recovery valid after container growth.
    auto hooks = std::make_unique<XdgToplevelHooks>();
    auto* hooks_ptr = hooks.get();
    hooks_ptr->state = this;
    hooks_ptr->toplevel = toplevel;
    hooks_ptr->surface = toplevel->base->surface;
    hooks_ptr->id = next_surface_id++;
    {
        std::scoped_lock lock(hooks_mutex);
        xdg_hooks.push_back(std::move(hooks));
    }

    wl_list_init(&hooks_ptr->surface_commit.link);
    hooks_ptr->surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_commit));
        h->state->handle_xdg_surface_commit(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.commit, &hooks_ptr->surface_commit);

    wl_list_init(&hooks_ptr->xdg_ack_configure.link);
    hooks_ptr->xdg_ack_configure.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XdgToplevelHooks, xdg_ack_configure));
        h->state->handle_xdg_surface_ack_configure(h);
    };
    wl_signal_add(&toplevel->base->events.ack_configure, &hooks_ptr->xdg_ack_configure);

    wl_list_init(&hooks_ptr->surface_map.link);
    hooks_ptr->surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_map));
        h->state->handle_xdg_surface_map(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.map, &hooks_ptr->surface_map);

    wl_list_init(&hooks_ptr->surface_destroy.link);
    hooks_ptr->surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_destroy));
        h->state->handle_xdg_surface_destroy(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.destroy, &hooks_ptr->surface_destroy);

    wl_list_init(&hooks_ptr->toplevel_destroy.link);
    hooks_ptr->toplevel_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, toplevel_destroy));
        detach_listener(h->toplevel_destroy);
        detach_listener(h->xdg_ack_configure);
        h->toplevel = nullptr;
    };
    wl_signal_add(&toplevel->events.destroy, &hooks_ptr->toplevel_destroy);
}

void CompositorState::handle_new_xdg_popup(wlr_xdg_popup* popup) {
    if (!popup || !popup->base || !popup->base->surface) {
        return;
    }

    GOGGLES_LOG_DEBUG("New XDG popup: popup={} surface={} parent={}", static_cast<void*>(popup),
                      static_cast<void*>(popup->base->surface), static_cast<void*>(popup->parent));

    // Stable hook allocation keeps wl_listener container_of recovery valid after container growth.
    auto hooks = std::make_unique<XdgPopupHooks>();
    auto* hooks_ptr = hooks.get();
    hooks_ptr->state = this;
    hooks_ptr->popup = popup;
    hooks_ptr->surface = popup->base->surface;
    hooks_ptr->parent_surface = popup->parent;
    hooks_ptr->id = next_surface_id++;
    {
        std::scoped_lock lock(hooks_mutex);
        xdg_popup_hooks.push_back(std::move(hooks));
    }

    wl_list_init(&hooks_ptr->surface_commit.link);
    hooks_ptr->surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, surface_commit));
        h->state->handle_xdg_popup_commit(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.commit, &hooks_ptr->surface_commit);

    wl_list_init(&hooks_ptr->xdg_ack_configure.link);
    hooks_ptr->xdg_ack_configure.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, xdg_ack_configure));
        h->state->handle_xdg_popup_ack_configure(h);
    };
    wl_signal_add(&popup->base->events.ack_configure, &hooks_ptr->xdg_ack_configure);

    wl_list_init(&hooks_ptr->surface_map.link);
    hooks_ptr->surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, surface_map));
        h->state->handle_xdg_popup_map(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.map, &hooks_ptr->surface_map);

    wl_list_init(&hooks_ptr->surface_destroy.link);
    hooks_ptr->surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, surface_destroy));
        h->state->handle_xdg_popup_destroy(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.destroy, &hooks_ptr->surface_destroy);

    wl_list_init(&hooks_ptr->popup_destroy.link);
    hooks_ptr->popup_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, popup_destroy));
        h->state->handle_xdg_popup_destroy(h);
    };
    wl_signal_add(&popup->events.destroy, &hooks_ptr->popup_destroy);
}

void CompositorState::handle_xdg_popup_commit(XdgPopupHooks* hooks) {
    if (!hooks || !hooks->popup || !hooks->popup->base || !hooks->popup->base->initialized) {
        return;
    }

    if (!hooks->sent_configure) {
        wlr_xdg_surface_schedule_configure(hooks->popup->base);
        hooks->sent_configure = true;

        std::scoped_lock lock(hooks_mutex);
        auto* owner_root = get_popup_owner_root_surface(*this, *hooks);
        if (owner_root) {
            auto extent_opt = get_surface_extent(owner_root);
            if (extent_opt) {
                const auto [width, height] = *extent_opt;
                if (width > 0 && height > 0) {
                    wlr_box constraint_box{
                        .x = 0,
                        .y = 0,
                        .width = static_cast<int>(width),
                        .height = static_cast<int>(height),
                    };
                    wlr_xdg_popup_unconstrain_from_box(hooks->popup, &constraint_box);
                }
            }
        }
    }

    note_active_surface_commit(hooks->surface);

    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->surface, &now);

    update_presented_frame(hooks->surface);
}

void CompositorState::handle_xdg_popup_ack_configure(XdgPopupHooks* hooks) {
    if (!hooks || hooks->acked_configure) {
        return;
    }

    hooks->acked_configure = true;
    detach_listener(hooks->xdg_ack_configure);
}

void CompositorState::handle_xdg_popup_map(XdgPopupHooks* hooks) {
    if (!hooks || hooks->mapped) {
        return;
    }

    hooks->mapped = true;

    GOGGLES_LOG_DEBUG("XDG popup mapped: id={} surface={} parent={}", hooks->id,
                      static_cast<void*>(hooks->surface),
                      static_cast<void*>(hooks->parent_surface));

    detach_listener(hooks->surface_map);
    request_present_reset();
}

void CompositorState::handle_xdg_popup_destroy(XdgPopupHooks* hooks) {
    if (!hooks || hooks->destroyed) {
        return;
    }

    auto* surface = hooks->surface;
    hooks->destroyed = true;

    GOGGLES_LOG_DEBUG("XDG popup destroyed: id={} surface={} parent={}", hooks->id,
                      static_cast<void*>(hooks->surface),
                      static_cast<void*>(hooks->parent_surface));

    detach_listener(hooks->surface_destroy);
    detach_listener(hooks->surface_commit);
    detach_listener(hooks->surface_map);
    detach_listener(hooks->xdg_ack_configure);
    detach_listener(hooks->popup_destroy);

    {
        std::scoped_lock lock(hooks_mutex);
        auto hook_it = std::find_if(
            xdg_popup_hooks.begin(), xdg_popup_hooks.end(),
            [hooks](const std::unique_ptr<XdgPopupHooks>& entry) { return entry.get() == hooks; });
        if (hook_it != xdg_popup_hooks.end()) {
            xdg_popup_hooks.erase(hook_it);
        }
    }

    if (keyboard_entered_surface == surface) {
        keyboard_entered_surface = nullptr;
    }
    if (pointer_entered_surface == surface) {
        pointer_entered_surface = nullptr;
    }

    request_present_reset();
}

void CompositorState::handle_xdg_surface_commit(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || !hooks->toplevel->base || !hooks->toplevel->base->initialized) {
        return;
    }

    if (!hooks->sent_configure) {
        wlr_xdg_surface_schedule_configure(hooks->toplevel->base);
        hooks->sent_configure = true;
    }

    note_active_surface_commit(hooks->surface);

    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->surface, &now);

    update_presented_frame(hooks->surface);
}

void CompositorState::handle_xdg_surface_ack_configure(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || hooks->acked_configure) {
        return;
    }

    hooks->acked_configure = true;
    detach_listener(hooks->xdg_ack_configure);

    if (!hooks->sent_configure) {
        return;
    }

    wlr_xdg_toplevel_set_activated(hooks->toplevel, true);
    focus_surface(hooks->surface);
}

void CompositorState::handle_xdg_surface_map(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || hooks->mapped) {
        return;
    }

    hooks->mapped = true;

    GOGGLES_LOG_DEBUG("XDG surface mapped: id={} surface={} title='{}' app_id='{}' size={}x{}",
                      hooks->id, static_cast<void*>(hooks->surface),
                      hooks->toplevel->title ? hooks->toplevel->title : "",
                      hooks->toplevel->app_id ? hooks->toplevel->app_id : "",
                      hooks->toplevel->current.width, hooks->toplevel->current.height);

    detach_listener(hooks->surface_map);
}

void CompositorState::handle_xdg_surface_destroy(XdgToplevelHooks* hooks) {
    auto* surface = hooks->surface;

    detach_listener(hooks->surface_destroy);
    detach_listener(hooks->surface_commit);
    detach_listener(hooks->surface_map);
    detach_listener(hooks->xdg_ack_configure);
    detach_listener(hooks->toplevel_destroy);

    bool clear_focus = false;
    {
        std::scoped_lock lock(hooks_mutex);
        if (!focused_xsurface && focused_surface == surface) {
            focused_surface = nullptr;
            clear_focus = true;
        }

        auto hook_it = std::find_if(xdg_hooks.begin(), xdg_hooks.end(),
                                    [hooks](const std::unique_ptr<XdgToplevelHooks>& entry) {
                                        return entry.get() == hooks;
                                    });
        if (hook_it != xdg_hooks.end()) {
            xdg_hooks.erase(hook_it);
        }
    }

    if (clear_focus) {
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
    if (presented_surface == surface) {
        clear_presented_frame();
    }
}

} // namespace goggles::input
