#pragma once

#include <cstdint>
#include <optional>
#include <utility>

extern "C" {
// NOLINTBEGIN(readability-identifier-naming)
struct wlr_layer_surface_v1_state;
struct wlr_surface;
struct wlr_xdg_surface;
struct wlr_xwayland_surface;
// NOLINTEND(readability-identifier-naming)
}

namespace goggles::compositor {

struct CompositorState;
struct LayerSurfaceHooks;
struct XdgPopupHooks;

using ::wlr_layer_surface_v1_state;
using ::wlr_surface;
using ::wlr_xdg_surface;
using ::wlr_xwayland_surface;

struct InputTarget {
    wlr_surface* surface = nullptr;
    wlr_xwayland_surface* xsurface = nullptr;
    wlr_surface* root_surface = nullptr;
    wlr_xwayland_surface* root_xsurface = nullptr;
    double offset_x = 0.0;
    double offset_y = 0.0;
};

auto compute_layer_position(const wlr_layer_surface_v1_state& state, int out_w, int out_h)
    -> std::pair<int, int>;
auto get_surface_extent(wlr_surface* surface) -> std::optional<std::pair<uint32_t, uint32_t>>;
auto get_root_xdg_surface(wlr_surface* surface) -> wlr_xdg_surface*;
auto get_popup_owner_root_surface(const CompositorState& state, const XdgPopupHooks& hooks)
    -> wlr_surface*;
auto get_xdg_popup_position(const XdgPopupHooks* hooks) -> std::pair<double, double>;
auto get_root_input_target(CompositorState& state) -> InputTarget;
auto resolve_input_target(CompositorState& state, const InputTarget& root_target,
                          bool use_pointer_hit_test) -> InputTarget;
auto get_input_target(CompositorState& state) -> InputTarget;
auto get_cursor_bounds(const CompositorState& state, const InputTarget& root_target)
    -> std::optional<std::pair<double, double>>;
auto get_surface_local_coords(const CompositorState& state, const InputTarget& target)
    -> std::pair<double, double>;

} // namespace goggles::compositor
