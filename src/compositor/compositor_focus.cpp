#include "compositor_state.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/region.h>

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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
// wlr_layer_shell_v1.h contains 'char *namespace' which conflicts with C++ keyword
#define namespace namespace_
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::input {

namespace {

auto get_time_msec() -> uint32_t {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

struct CursorBounds {
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
};

struct FocusSnapshot {
    wlr_surface* focused_surface = nullptr;
    wlr_xwayland_surface* focused_xsurface = nullptr;
    wlr_surface* root_surface = nullptr;
    wlr_xwayland_surface* root_xsurface = nullptr;
};

auto snapshot_focus(const CompositorState& state) -> FocusSnapshot {
    std::scoped_lock lock(state.hooks_mutex);

    FocusSnapshot snapshot{
        .focused_surface = state.focused_surface,
        .focused_xsurface = state.focused_xsurface,
    };
    if (snapshot.focused_xsurface && snapshot.focused_xsurface->surface) {
        snapshot.root_surface = snapshot.focused_xsurface->surface;
        snapshot.root_xsurface = snapshot.focused_xsurface;
    } else {
        snapshot.root_surface = snapshot.focused_surface;
    }

    return snapshot;
}

void deactivate_previous_focus(CompositorState& state) {
    wlr_xwayland_surface* old_xsurface = nullptr;
    wlr_xdg_toplevel* old_toplevel = nullptr;

    {
        std::scoped_lock lock(state.hooks_mutex);
        old_xsurface = state.focused_xsurface;
        if (!old_xsurface && state.focused_surface) {
            for (const auto& hooks_entry : state.xdg_hooks) {
                const auto* hooks = hooks_entry.get();
                if (hooks->surface == state.focused_surface && hooks->toplevel) {
                    old_toplevel = hooks->toplevel;
                    break;
                }
            }
        }
    }

    if (old_xsurface) {
        wlr_xwayland_surface_activate(old_xsurface, false);
        return;
    }
    if (old_toplevel) {
        wlr_xdg_toplevel_set_activated(old_toplevel, false);
    }
}

} // namespace

auto CompositorServer::is_pointer_locked() const -> bool {
    return m_impl->state.pointer_locked.load(std::memory_order_acquire);
}

auto CompositorServer::get_surfaces() const -> std::vector<SurfaceInfo> {
    GOGGLES_PROFILE_FUNCTION();
    return m_impl->state.get_surfaces_snapshot();
}

void CompositorServer::set_input_target(uint32_t surface_id) {
    m_impl->state.request_focus_target(surface_id);
}

void CompositorServer::request_surface_resize(uint32_t surface_id,
                                              const SurfaceResizeInfo& resize) {
    m_impl->state.request_surface_resize(surface_id, resize);
}

void CompositorState::handle_focus_request() {
    const auto focus_id = pending_focus_target.exchange(NO_FOCUS_TARGET, std::memory_order_acq_rel);
    if (focus_id == NO_FOCUS_TARGET) {
        return;
    }
    focus_surface_by_id(focus_id);
}

void CompositorState::handle_surface_resize_requests() {
    while (auto request_opt = resize_queue.try_pop()) {
        apply_surface_resize_request(*request_opt);
    }
}

void CompositorState::handle_new_pointer_constraint(wlr_pointer_constraint_v1* constraint) {
    const auto focus = snapshot_focus(*this);
    wlr_surface* target_surface = focus.root_surface;

    if (constraint->surface == target_surface) {
        activate_constraint(constraint);
    }

    auto hooks = std::make_unique<ConstraintHooks>();
    auto* hooks_ptr = hooks.get();
    hooks_ptr->state = this;
    hooks_ptr->constraint = constraint;
    {
        std::scoped_lock lock(hooks_mutex);
        constraint_hooks.push_back(std::move(hooks));
    }

    wl_list_init(&hooks_ptr->set_region.link);
    hooks_ptr->set_region.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<ConstraintHooks*>(reinterpret_cast<char*>(listener) -
                                                     offsetof(ConstraintHooks, set_region));
        h->state->handle_constraint_set_region(h);
    };
    wl_signal_add(&constraint->events.set_region, &hooks_ptr->set_region);

    wl_list_init(&hooks_ptr->destroy.link);
    hooks_ptr->destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<ConstraintHooks*>(reinterpret_cast<char*>(listener) -
                                                     offsetof(ConstraintHooks, destroy));
        h->state->handle_constraint_destroy(h);
    };
    wl_signal_add(&constraint->events.destroy, &hooks_ptr->destroy);
}

void CompositorState::handle_constraint_set_region(ConstraintHooks* hooks) {
    if (!hooks || active_constraint != hooks->constraint) {
        return;
    }

    apply_cursor_hint_if_needed();

    if (active_constraint->type != WLR_POINTER_CONSTRAINT_V1_CONFINED) {
        return;
    }

    if (!pixman_region32_not_empty(&active_constraint->region)) {
        return;
    }

    const double previous_x = cursor_x;
    const double previous_y = cursor_y;
    const int cursor_x_int = static_cast<int>(std::floor(cursor_x));
    const int cursor_y_int = static_cast<int>(std::floor(cursor_y));
    if (!pixman_region32_contains_point(&active_constraint->region, cursor_x_int, cursor_y_int,
                                        nullptr)) {
        int box_count = 0;
        pixman_box32_t* boxes = pixman_region32_rectangles(&active_constraint->region, &box_count);
        if (boxes && box_count > 0) {
            const auto& box = boxes[0];
            cursor_x =
                std::clamp(cursor_x, static_cast<double>(box.x1), static_cast<double>(box.x2 - 1));
            cursor_y =
                std::clamp(cursor_y, static_cast<double>(box.y1), static_cast<double>(box.y2 - 1));
            cursor_initialized = true;
        }
    }

    if (cursor_initialized && (previous_x != cursor_x || previous_y != cursor_y)) {
        const uint32_t time = get_time_msec();
        wlr_seat_pointer_notify_motion(seat, time, cursor_x, cursor_y);
        wlr_seat_pointer_notify_frame(seat);
    }

    request_present_reset();
}

void CompositorState::handle_constraint_destroy(ConstraintHooks* hooks) {
    if (!hooks) {
        return;
    }

    if (active_constraint == hooks->constraint) {
        active_constraint = nullptr;
        pointer_locked.store(false, std::memory_order_release);
        request_present_reset();
    }

    detach_listener(hooks->set_region);
    detach_listener(hooks->destroy);

    std::scoped_lock lock(hooks_mutex);
    auto hook_it = std::find_if(
        constraint_hooks.begin(), constraint_hooks.end(),
        [hooks](const std::unique_ptr<ConstraintHooks>& entry) { return entry.get() == hooks; });
    if (hook_it != constraint_hooks.end()) {
        constraint_hooks.erase(hook_it);
    }
}

void CompositorState::activate_constraint(wlr_pointer_constraint_v1* constraint) {
    if (active_constraint == constraint) {
        return;
    }
    // Pointer constraints belong to the previously focused surface and must be dropped first.
    deactivate_constraint();
    active_constraint = constraint;
    pointer_locked.store(constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED,
                         std::memory_order_release);
    wlr_pointer_constraint_v1_send_activated(constraint);
    apply_cursor_hint_if_needed();
    request_present_reset();
    GOGGLES_LOG_DEBUG("Pointer constraint activated: type={}",
                      constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED ? "locked" : "confined");
}

void CompositorState::deactivate_constraint() {
    if (!active_constraint) {
        return;
    }
    wlr_pointer_constraint_v1_send_deactivated(active_constraint);
    GOGGLES_LOG_DEBUG("Pointer constraint deactivated");
    active_constraint = nullptr;
    pointer_locked.store(false, std::memory_order_release);
    request_present_reset();
}

void CompositorState::focus_surface(wlr_surface* surface) {
    if (snapshot_focus(*this).focused_surface == surface) {
        return;
    }

    uint32_t focused_id = 0;
    std::string title;
    std::string app_id;
    int width = 0;
    int height = 0;
    {
        std::scoped_lock lock(hooks_mutex);
        for (const auto& hooks_entry : xdg_hooks) {
            const auto* hooks = hooks_entry.get();
            if (hooks->surface != surface || !hooks->toplevel) {
                continue;
            }
            focused_id = hooks->id;
            title = hooks->toplevel->title ? hooks->toplevel->title : "";
            app_id = hooks->toplevel->app_id ? hooks->toplevel->app_id : "";
            width = hooks->toplevel->current.width;
            height = hooks->toplevel->current.height;
            break;
        }
    }

    deactivate_constraint();

    deactivate_previous_focus(*this);

    {
        std::scoped_lock lock(hooks_mutex);
        focused_xsurface = nullptr;
        focused_surface = surface;
    }

    GOGGLES_LOG_DEBUG("Focused XDG: id={} surface={} title='{}' app_id='{}' size={}x{}", focused_id,
                      static_cast<void*>(surface), title, app_id, width, height);

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes,
                                   &keyboard->modifiers);
    reset_cursor_for_surface(surface);
    wlr_seat_pointer_notify_enter(seat, surface, cursor_x, cursor_y);
    keyboard_entered_surface = surface;
    pointer_entered_surface = surface;

    if (pointer_constraints) {
        auto* constraint =
            wlr_pointer_constraints_v1_constraint_for_surface(pointer_constraints, surface, seat);
        if (constraint) {
            activate_constraint(constraint);
        }
    }

    refresh_presented_frame();
}

void CompositorState::focus_xwayland_surface(wlr_xwayland_surface* xsurface) {
    if (snapshot_focus(*this).focused_xsurface == xsurface) {
        return;
    }

    deactivate_constraint();

    deactivate_previous_focus(*this);

    // Clear stale wl_surface focus before re-entering the XWayland path for this window.
    wlr_seat_keyboard_clear_focus(seat);
    wlr_seat_pointer_clear_focus(seat);
    keyboard_entered_surface = nullptr;
    pointer_entered_surface = nullptr;

    {
        std::scoped_lock lock(hooks_mutex);
        focused_xsurface = xsurface;
        focused_surface = xsurface->surface;
    }

    GOGGLES_LOG_DEBUG("Focused XWayland: window_id={} ptr={} surface={} title='{}'",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface),
                      static_cast<void*>(xsurface->surface),
                      xsurface->title ? xsurface->title : "");

    // XWayland needs explicit re-activation here so input keeps following the focused window.
    wlr_xwayland_surface_activate(xsurface, true);

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, xsurface->surface, keyboard->keycodes,
                                   keyboard->num_keycodes, &keyboard->modifiers);
    reset_cursor_for_surface(xsurface->surface);
    wlr_seat_pointer_notify_enter(seat, xsurface->surface, cursor_x, cursor_y);
    keyboard_entered_surface = xsurface->surface;
    pointer_entered_surface = xsurface->surface;

    if (pointer_constraints && xsurface->surface) {
        auto* constraint = wlr_pointer_constraints_v1_constraint_for_surface(
            pointer_constraints, xsurface->surface, seat);
        if (constraint) {
            activate_constraint(constraint);
        }
    }

    refresh_presented_frame();
}

bool CompositorState::focus_surface_by_id(uint32_t surface_id) {
    wlr_xwayland_surface* xwayland_target = nullptr;
    wlr_surface* xdg_surface_target = nullptr;
    wlr_xdg_toplevel* xdg_toplevel_target = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        for (const auto& hooks_entry : xwayland_hooks) {
            auto* hooks = hooks_entry.get();
            if (hooks->override_redirect) {
                continue;
            }
            if (hooks->id == surface_id && hooks->xsurface && hooks->xsurface->surface) {
                xwayland_target = hooks->xsurface;
                break;
            }
        }
        if (!xwayland_target) {
            for (const auto& hooks_entry : xdg_hooks) {
                auto* hooks = hooks_entry.get();
                if (hooks->id == surface_id && hooks->surface && hooks->toplevel) {
                    xdg_surface_target = hooks->surface;
                    xdg_toplevel_target = hooks->toplevel;
                    break;
                }
            }
        }
    }

    if (xwayland_target) {
        focus_xwayland_surface(xwayland_target);
        return true;
    }

    if (xdg_surface_target && xdg_toplevel_target) {
        wlr_xdg_toplevel_set_activated(xdg_toplevel_target, true);
        focus_surface(xdg_surface_target);
        return true;
    }

    return false;
}

void CompositorState::apply_surface_resize_request(const SurfaceResizeRequest& request) {
    if (request.surface_id == NO_FOCUS_TARGET) {
        return;
    }

    XdgToplevelHooks* xdg_hooks_entry = nullptr;
    XWaylandSurfaceHooks* xwayland_hooks_entry = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        for (const auto& hooks_entry : xdg_hooks) {
            auto* hooks = hooks_entry.get();
            if (hooks && hooks->id == request.surface_id) {
                xdg_hooks_entry = hooks;
                break;
            }
        }
        if (!xdg_hooks_entry) {
            for (const auto& hooks_entry : xwayland_hooks) {
                auto* hooks = hooks_entry.get();
                if (hooks && hooks->id == request.surface_id) {
                    xwayland_hooks_entry = hooks;
                    break;
                }
            }
        }
    }

    if (xdg_hooks_entry && xdg_hooks_entry->toplevel) {
        wlr_xdg_toplevel_set_maximized(xdg_hooks_entry->toplevel, request.resize.maximized);
        if (request.resize.width > 0 && request.resize.height > 0) {
            wlr_xdg_toplevel_set_size(xdg_hooks_entry->toplevel,
                                      static_cast<int>(request.resize.width),
                                      static_cast<int>(request.resize.height));
        } else {
            wlr_xdg_toplevel_set_size(xdg_hooks_entry->toplevel, 0, 0);
        }
        request_present_reset();
        return;
    }

    if (xwayland_hooks_entry && xwayland_hooks_entry->xsurface) {
        auto* xsurface = xwayland_hooks_entry->xsurface;
        wlr_xwayland_surface_set_maximized(xsurface, request.resize.maximized,
                                           request.resize.maximized);
        if (request.resize.width > 0 && request.resize.height > 0) {
            const auto width = static_cast<uint16_t>(
                std::min<uint32_t>(request.resize.width, std::numeric_limits<uint16_t>::max()));
            const auto height = static_cast<uint16_t>(
                std::min<uint32_t>(request.resize.height, std::numeric_limits<uint16_t>::max()));
            wlr_xwayland_surface_configure(xsurface, static_cast<int16_t>(xsurface->x),
                                           static_cast<int16_t>(xsurface->y), width, height);
        }
        request_present_reset();
    }
}

auto get_surface_extent(wlr_surface* surface) -> std::optional<std::pair<uint32_t, uint32_t>> {
    if (surface && surface->current.width > 0 && surface->current.height > 0) {
        return std::pair<uint32_t, uint32_t>(static_cast<uint32_t>(surface->current.width),
                                             static_cast<uint32_t>(surface->current.height));
    }

    return std::nullopt;
}

auto get_root_xdg_surface(wlr_surface* surface) -> wlr_xdg_surface* {
    auto* xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
    while (xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        if (!xdg_surface->popup || !xdg_surface->popup->parent) {
            break;
        }
        auto* parent = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
        if (!parent || parent == xdg_surface) {
            break;
        }
        xdg_surface = parent;
    }
    return xdg_surface;
}

auto get_popup_owner_root_surface(const CompositorState& state, const XdgPopupHooks& hooks)
    -> wlr_surface* {
    wlr_surface* owner_surface = hooks.parent_surface;
    if (!owner_surface) {
        auto* root_xdg = get_root_xdg_surface(hooks.surface);
        return root_xdg ? root_xdg->surface : nullptr;
    }

    auto* root_xdg = get_root_xdg_surface(owner_surface);
    if (root_xdg && root_xdg->surface) {
        return root_xdg->surface;
    }

    for (;;) {
        const auto it =
            std::find_if(state.xdg_popup_hooks.begin(), state.xdg_popup_hooks.end(),
                         [owner_surface](const auto& popup_hooks) {
                             return popup_hooks && popup_hooks->surface == owner_surface;
                         });
        if (it == state.xdg_popup_hooks.end() || !(*it) || !(*it)->parent_surface) {
            break;
        }

        owner_surface = (*it)->parent_surface;
        root_xdg = get_root_xdg_surface(owner_surface);
        if (root_xdg && root_xdg->surface) {
            return root_xdg->surface;
        }
    }

    return owner_surface;
}

auto get_xdg_popup_position(const XdgPopupHooks* hooks) -> std::pair<double, double> {
    if (!hooks || !hooks->popup) {
        return {0.0, 0.0};
    }

    double popup_x = 0.0;
    double popup_y = 0.0;
    wlr_xdg_popup_get_position(hooks->popup, &popup_x, &popup_y);
    return {popup_x, popup_y};
}

auto compute_layer_position(const wlr_layer_surface_v1_state& state, int out_w, int out_h)
    -> std::pair<int, int> {
    const auto& margin = state.margin;
    const int surf_w = static_cast<int>(state.actual_width);
    const int surf_h = static_cast<int>(state.actual_height);

    const bool anchored_left = (state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) != 0;
    const bool anchored_right = (state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) != 0;
    const bool anchored_top = (state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) != 0;
    const bool anchored_bottom = (state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) != 0;

    int pos_x = 0;
    int pos_y = 0;

    if (anchored_left) {
        pos_x = static_cast<int>(margin.left);
    } else if (anchored_right) {
        pos_x = out_w - surf_w - static_cast<int>(margin.right);
    } else {
        pos_x = (out_w - surf_w) / 2;
    }

    if (anchored_top) {
        pos_y = static_cast<int>(margin.top);
    } else if (anchored_bottom) {
        pos_y = out_h - surf_h - static_cast<int>(margin.bottom);
    } else {
        pos_y = (out_h - surf_h) / 2;
    }

    return {pos_x, pos_y};
}

auto get_cursor_bounds_rect(const CompositorState& state, const InputTarget& root_target)
    -> std::optional<CursorBounds> {
    if (!root_target.root_surface) {
        return std::nullopt;
    }

    auto extent_opt = get_surface_extent(root_target.root_surface);
    if (!extent_opt) {
        return std::nullopt;
    }

    CursorBounds bounds{
        .min_x = 0.0,
        .min_y = 0.0,
        .max_x = static_cast<double>(extent_opt->first),
        .max_y = static_cast<double>(extent_opt->second),
    };

    if (!root_target.root_xsurface) {
        std::scoped_lock lock(state.hooks_mutex);
        for (const auto& hooks : state.xdg_popup_hooks) {
            auto* popup_hooks = hooks.get();
            if (!popup_hooks || !popup_hooks->mapped || !popup_hooks->popup ||
                !popup_hooks->surface) {
                continue;
            }
            if (!popup_hooks->acked_configure) {
                continue;
            }
            auto* owner_root = get_popup_owner_root_surface(state, *popup_hooks);
            if (owner_root != root_target.root_surface) {
                continue;
            }

            auto popup_extent = get_surface_extent(popup_hooks->surface);
            if (!popup_extent) {
                continue;
            }

            auto [popup_x, popup_y] = get_xdg_popup_position(popup_hooks);
            bounds.min_x = std::min(bounds.min_x, popup_x);
            bounds.min_y = std::min(bounds.min_y, popup_y);
            bounds.max_x =
                std::max(bounds.max_x, popup_x + static_cast<double>(popup_extent->first));
            bounds.max_y =
                std::max(bounds.max_y, popup_y + static_cast<double>(popup_extent->second));
        }

        return bounds;
    }

    std::scoped_lock lock(state.hooks_mutex);
    for (const auto& hooks_entry : state.xwayland_hooks) {
        const auto* hooks = hooks_entry.get();
        if (!hooks->mapped || !hooks->override_redirect || !hooks->xsurface ||
            !hooks->xsurface->surface) {
            continue;
        }

        const auto* popup = hooks->xsurface;
        const auto* parent = popup->parent;
        bool belongs_to_root = false;
        while (parent) {
            if (parent == root_target.root_xsurface) {
                belongs_to_root = true;
                break;
            }
            parent = parent->parent;
        }
        if (!belongs_to_root && !popup->parent) {
            belongs_to_root = true;
        }
        if (!belongs_to_root) {
            continue;
        }

        auto popup_extent = get_surface_extent(popup->surface);
        if (!popup_extent) {
            continue;
        }

        const double popup_x =
            static_cast<double>(popup->x) - static_cast<double>(root_target.root_xsurface->x);
        const double popup_y =
            static_cast<double>(popup->y) - static_cast<double>(root_target.root_xsurface->y);
        bounds.min_x = std::min(bounds.min_x, popup_x);
        bounds.min_y = std::min(bounds.min_y, popup_y);
        bounds.max_x = std::max(bounds.max_x, popup_x + static_cast<double>(popup_extent->first));
        bounds.max_y = std::max(bounds.max_y, popup_y + static_cast<double>(popup_extent->second));
    }

    return bounds;
}

auto get_surface_local_coords(const CompositorState& state, const InputTarget& target)
    -> std::pair<double, double> {
    if (!target.surface) {
        return {0.0, 0.0};
    }

    double local_x = state.cursor_x - target.offset_x;
    double local_y = state.cursor_y - target.offset_y;

    const bool clamp_to_surface = !target.root_surface || target.surface == target.root_surface;
    if (clamp_to_surface) {
        auto extent_opt = get_surface_extent(target.surface);
        if (extent_opt) {
            const auto [width, height] = *extent_opt;
            if (width > 0 && height > 0) {
                local_x = std::clamp(local_x, 0.0, static_cast<double>(width - 1));
                local_y = std::clamp(local_y, 0.0, static_cast<double>(height - 1));
            }
        }
    }

    return {local_x, local_y};
}

void CompositorState::reset_cursor_for_surface(wlr_surface* surface) {
    cursor_surface = surface;
    auto extent_opt = get_surface_extent(surface);
    if (!extent_opt) {
        cursor_initialized = false;
        return;
    }

    const auto [width, height] = *extent_opt;
    if (width == 0 || height == 0) {
        cursor_initialized = false;
        return;
    }

    cursor_x = static_cast<double>(width) * 0.5;
    cursor_y = static_cast<double>(height) * 0.5;
    cursor_initialized = true;
}

void CompositorState::apply_cursor_hint_if_needed() {
    if (!active_constraint || active_constraint->type != WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        return;
    }

    cursor_surface = active_constraint->surface;

    const auto& hint = active_constraint->current.cursor_hint;
    if (!hint.enabled) {
        return;
    }

    const double previous_x = cursor_x;
    const double previous_y = cursor_y;
    cursor_x = hint.x;
    cursor_y = hint.y;
    cursor_initialized = true;

    auto extent_opt = get_surface_extent(active_constraint->surface);
    if (!extent_opt) {
        return;
    }

    const auto [width, height] = *extent_opt;
    if (width == 0 || height == 0) {
        return;
    }
    cursor_x = std::clamp(cursor_x, 0.0, static_cast<double>(width - 1));
    cursor_y = std::clamp(cursor_y, 0.0, static_cast<double>(height - 1));

    if ((previous_x != cursor_x || previous_y != cursor_y) &&
        cursor_visible.load(std::memory_order_acquire)) {
        request_present_reset();
    }
}

void CompositorState::auto_focus_next_surface() {
    wlr_xwayland_surface* xwayland_target = nullptr;
    wlr_surface* xdg_surface_target = nullptr;
    wlr_xdg_toplevel* xdg_toplevel_target = nullptr;

    {
        std::scoped_lock lock(hooks_mutex);
        for (const auto& hooks_entry : xwayland_hooks) {
            auto* hooks = hooks_entry.get();
            if (hooks->mapped && !hooks->override_redirect && hooks->xsurface &&
                hooks->xsurface->surface) {
                xwayland_target = hooks->xsurface;
            }
        }

        if (!xwayland_target) {
            for (const auto& hooks_entry : xdg_hooks) {
                auto* hooks = hooks_entry.get();
                if (hooks->mapped && hooks->surface && hooks->toplevel) {
                    xdg_surface_target = hooks->surface;
                    xdg_toplevel_target = hooks->toplevel;
                }
            }
        }
    }

    if (xwayland_target) {
        focus_xwayland_surface(xwayland_target);
        return;
    }

    if (xdg_surface_target && xdg_toplevel_target) {
        wlr_xdg_toplevel_set_activated(xdg_toplevel_target, true);
        focus_surface(xdg_surface_target);
        return;
    }

    clear_presented_frame();
}

void CompositorState::update_cursor_position(const InputEvent& event,
                                             const InputTarget& root_target) {
    wlr_surface* surface = root_target.root_surface;
    if (!surface) {
        return;
    }
    if (cursor_surface != surface || !cursor_initialized) {
        reset_cursor_for_surface(surface);
    }
    if (!cursor_initialized) {
        return;
    }

    const double previous_x = cursor_x;
    const double previous_y = cursor_y;
    double next_x = cursor_x + event.dx;
    double next_y = cursor_y + event.dy;

    auto bounds_opt = get_cursor_bounds_rect(*this, root_target);
    if (bounds_opt) {
        const auto& bounds = *bounds_opt;
        if (bounds.max_x > bounds.min_x && bounds.max_y > bounds.min_y) {
            next_x = std::clamp(next_x, bounds.min_x, bounds.max_x - 1.0);
            next_y = std::clamp(next_y, bounds.min_y, bounds.max_y - 1.0);
        }
    }

    if (active_constraint && active_constraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED) {
        if (pixman_region32_not_empty(&active_constraint->region)) {
            double confined_x = next_x;
            double confined_y = next_y;
            if (wlr_region_confine(&active_constraint->region, cursor_x, cursor_y, next_x, next_y,
                                   &confined_x, &confined_y)) {
                next_x = confined_x;
                next_y = confined_y;
            }
        }
    }

    cursor_x = next_x;
    cursor_y = next_y;
    cursor_initialized = true;

    const bool show_cursor =
        cursor_visible.load(std::memory_order_acquire) &&
        (!active_constraint || active_constraint->type != WLR_POINTER_CONSTRAINT_V1_LOCKED);
    if (show_cursor && (previous_x != cursor_x || previous_y != cursor_y)) {
        request_present_reset();
    }
}

auto get_root_input_target(CompositorState& state) -> InputTarget {
    InputTarget target{};

    {
        std::scoped_lock lock(state.hooks_mutex);
        for (const auto& owned_hooks : state.layer_hooks) {
            const auto* hooks = owned_hooks.get();
            if (!hooks->mapped || !hooks->layer_surface || !hooks->surface) {
                continue;
            }
            if (hooks->layer != ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY &&
                hooks->layer != ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
                continue;
            }

            const auto& layer_state = hooks->layer_surface->current;
            const int out_w = state.output ? state.output->width : 0;
            const int out_h = state.output ? state.output->height : 0;
            const int surf_w = static_cast<int>(layer_state.actual_width);
            const int surf_h = static_cast<int>(layer_state.actual_height);

            const auto [pos_x, pos_y] = compute_layer_position(layer_state, out_w, out_h);
            const double local_x = state.cursor_x - static_cast<double>(pos_x);
            const double local_y = state.cursor_y - static_cast<double>(pos_y);

            if (local_x < 0.0 || local_y < 0.0 || local_x >= static_cast<double>(surf_w) ||
                local_y >= static_cast<double>(surf_h)) {
                continue;
            }

            target.surface = hooks->surface;
            target.root_surface = hooks->surface;
            target.offset_x = static_cast<double>(pos_x);
            target.offset_y = static_cast<double>(pos_y);
            return target;
        }
    }

    const auto focus = snapshot_focus(state);
    wlr_surface* root_surface = focus.root_surface;
    wlr_xwayland_surface* root_xsurface = focus.root_xsurface;

    if (!root_surface) {
        return target;
    }

    target.surface = root_surface;
    target.xsurface = root_xsurface;
    target.root_surface = root_surface;
    target.root_xsurface = root_xsurface;
    return target;
}

auto resolve_input_target(CompositorState& state, const InputTarget& root_target,
                          bool use_pointer_hit_test) -> InputTarget {
    InputTarget target = root_target;
    if (!root_target.root_surface) {
        return target;
    }

    if (!root_target.root_xsurface) {
        wlr_surface* hit_surface = nullptr;
        double sub_x = state.cursor_x;
        double sub_y = state.cursor_y;

        if (use_pointer_hit_test) {
            auto* root_xdg = get_root_xdg_surface(root_target.root_surface);
            if (root_xdg && root_xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
                hit_surface = wlr_xdg_surface_surface_at(root_xdg, state.cursor_x, state.cursor_y,
                                                         &sub_x, &sub_y);
            } else {
                hit_surface = wlr_surface_surface_at(root_target.root_surface, state.cursor_x,
                                                     state.cursor_y, &sub_x, &sub_y);
            }
        }

        XdgPopupHooks* topmost_popup = nullptr;
        {
            std::scoped_lock lock(state.hooks_mutex);
            for (const auto& hooks : state.xdg_popup_hooks) {
                auto* popup_hooks = hooks.get();
                if (!popup_hooks || !popup_hooks->mapped || !popup_hooks->popup ||
                    !popup_hooks->surface) {
                    continue;
                }
                if (!popup_hooks->acked_configure || popup_hooks->popup->seat != state.seat) {
                    continue;
                }
                if (get_popup_owner_root_surface(state, *popup_hooks) == root_target.root_surface) {
                    topmost_popup = popup_hooks;
                }
            }
        }

        if (topmost_popup) {
            if (use_pointer_hit_test && hit_surface) {
                wlr_surface* hit_root = wlr_surface_get_root_surface(hit_surface);
                if (hit_root == topmost_popup->surface) {
                    target.surface = hit_surface;
                    target.offset_x = state.cursor_x - sub_x;
                    target.offset_y = state.cursor_y - sub_y;
                    return target;
                }
            }

            auto [popup_sx, popup_sy] = get_xdg_popup_position(topmost_popup);
            target.surface = topmost_popup->surface;
            target.offset_x = popup_sx;
            target.offset_y = popup_sy;
            return target;
        }

        if (use_pointer_hit_test && hit_surface) {
            target.surface = hit_surface;
            target.offset_x = state.cursor_x - sub_x;
            target.offset_y = state.cursor_y - sub_y;
            return target;
        }

        target.surface = root_target.root_surface;
        target.offset_x = 0.0;
        target.offset_y = 0.0;
        return target;
    }

    XWaylandSurfaceHooks* topmost_popup = nullptr;
    {
        std::scoped_lock lock(state.hooks_mutex);
        for (const auto& hooks_entry : state.xwayland_hooks) {
            auto* hooks = hooks_entry.get();
            if (!hooks->mapped || !hooks->override_redirect || !hooks->xsurface ||
                !hooks->xsurface->surface) {
                continue;
            }

            const auto* popup = hooks->xsurface;
            const auto* parent = popup->parent;
            bool belongs_to_root = false;
            while (parent) {
                if (parent == root_target.root_xsurface) {
                    belongs_to_root = true;
                    break;
                }
                parent = parent->parent;
            }
            if (!belongs_to_root && !popup->parent) {
                belongs_to_root = true;
            }
            if (!belongs_to_root) {
                continue;
            }

            topmost_popup = hooks;
        }
    }

    if (topmost_popup && topmost_popup->xsurface && topmost_popup->xsurface->surface) {
        const auto* popup = topmost_popup->xsurface;
        target.surface = popup->surface;
        target.xsurface = topmost_popup->xsurface;
        target.offset_x =
            static_cast<double>(popup->x) - static_cast<double>(root_target.root_xsurface->x);
        target.offset_y =
            static_cast<double>(popup->y) - static_cast<double>(root_target.root_xsurface->y);
        return target;
    }

    target.surface = root_target.root_surface;
    target.xsurface = root_target.root_xsurface;
    target.offset_x = 0.0;
    target.offset_y = 0.0;
    return target;
}

auto get_input_target(CompositorState& state) -> InputTarget {
    auto root_target = get_root_input_target(state);
    return resolve_input_target(state, root_target, false);
}

auto CompositorState::get_surfaces_snapshot() const -> std::vector<SurfaceInfo> {
    std::scoped_lock lock(hooks_mutex);
    std::vector<SurfaceInfo> result;

    uint32_t target_id = 0;
    if (focused_xsurface && focused_xsurface->surface) {
        for (const auto& hooks_entry : xwayland_hooks) {
            auto* hooks = hooks_entry.get();
            if (!hooks->override_redirect && hooks->xsurface == focused_xsurface) {
                target_id = hooks->id;
                break;
            }
        }
    }
    if (target_id == 0 && focused_surface) {
        for (const auto& hooks_entry : xdg_hooks) {
            auto* hooks = hooks_entry.get();
            if (hooks->surface == focused_surface) {
                target_id = hooks->id;
                break;
            }
        }
    }

    for (const auto& hooks_entry : xwayland_hooks) {
        const auto* hooks = hooks_entry.get();
        if (hooks->override_redirect) {
            continue;
        }
        if (!hooks->xsurface || !hooks->xsurface->surface) {
            continue;
        }
        SurfaceInfo info{};
        info.id = hooks->id;
        info.title = hooks->title;
        info.class_name = hooks->class_name;
        info.width = hooks->xsurface->width;
        info.height = hooks->xsurface->height;
        info.is_xwayland = true;
        info.is_input_target = (info.id == target_id);
        result.push_back(std::move(info));
    }

    for (const auto& hooks_entry : xdg_hooks) {
        const auto* hooks = hooks_entry.get();
        if (!hooks->surface || !hooks->toplevel) {
            continue;
        }
        SurfaceInfo info{};
        info.id = hooks->id;
        info.title = hooks->toplevel->title ? hooks->toplevel->title : "";
        info.class_name = hooks->toplevel->app_id ? hooks->toplevel->app_id : "";
        info.width = hooks->toplevel->current.width;
        info.height = hooks->toplevel->current.height;
        info.is_xwayland = false;
        info.is_input_target = (info.id == target_id);
        result.push_back(std::move(info));
    }

    return result;
}

} // namespace goggles::input
