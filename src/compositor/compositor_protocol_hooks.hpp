#pragma once

#include <string>

extern "C" {
#include <wayland-server-core.h>
// NOLINTBEGIN(readability-identifier-naming)
struct wlr_layer_surface_v1;
struct wlr_pointer_constraint_v1;
struct wlr_surface;
struct wlr_xdg_popup;
struct wlr_xdg_toplevel;
struct wlr_xwayland_surface;
// NOLINTEND(readability-identifier-naming)
}

namespace goggles::compositor {

struct CompositorState;

using ::wl_listener;
using ::wlr_layer_surface_v1;
using ::wlr_pointer_constraint_v1;
using ::wlr_surface;
using ::wlr_xdg_popup;
using ::wlr_xdg_toplevel;
using ::wlr_xwayland_surface;

struct XWaylandSurfaceHooks {
    CompositorState* state = nullptr;
    wlr_xwayland_surface* xsurface = nullptr;
    uint32_t id = 0;
    std::string title;
    std::string class_name;
    // XWayland map_request can arrive before associate (surface becomes available).
    bool map_requested = false;
    bool mapped = false;
    bool override_redirect = false;
    wl_listener associate{};
    wl_listener dissociate{};
    wl_listener map_request{};
    wl_listener commit{};
    wl_listener destroy{};
};

struct XdgPopupHooks {
    CompositorState* state = nullptr;
    wlr_xdg_popup* popup = nullptr;
    wlr_surface* surface = nullptr;
    wlr_surface* parent_surface = nullptr;
    uint32_t id = 0;
    bool sent_configure = false;
    bool acked_configure = false;
    bool mapped = false;
    bool destroyed = false;

    wl_listener surface_commit{};
    wl_listener surface_map{};
    wl_listener surface_destroy{};
    wl_listener xdg_ack_configure{};
    wl_listener popup_destroy{};
};

struct XdgToplevelHooks {
    CompositorState* state = nullptr;
    wlr_xdg_toplevel* toplevel = nullptr;
    wlr_surface* surface = nullptr;
    uint32_t id = 0;
    bool sent_configure = false;
    bool acked_configure = false;
    bool mapped = false;

    wl_listener surface_commit{};
    wl_listener surface_map{};
    wl_listener surface_destroy{};
    wl_listener xdg_ack_configure{};
    wl_listener toplevel_destroy{};
};

struct LayerSurfaceHooks {
    CompositorState* state = nullptr;
    wlr_layer_surface_v1* layer_surface = nullptr;
    wlr_surface* surface = nullptr;
    uint32_t id = 0;
    uint32_t layer = 0;
    bool configured = false;
    bool mapped = false;
    bool destroyed = false;

    wl_listener surface_commit{};
    wl_listener surface_map{};
    wl_listener surface_unmap{};
    wl_listener surface_destroy{};
    wl_listener layer_destroy{};
    wl_listener new_popup{};
};

struct ConstraintHooks {
    CompositorState* state = nullptr;
    wlr_pointer_constraint_v1* constraint = nullptr;
    wl_listener set_region{};
    wl_listener destroy{};
};

inline void detach_listener(wl_listener& listener) {
    if (listener.link.next != nullptr && listener.link.prev != nullptr) {
        wl_list_remove(&listener.link);
    }
    wl_list_init(&listener.link);
}

} // namespace goggles::compositor
