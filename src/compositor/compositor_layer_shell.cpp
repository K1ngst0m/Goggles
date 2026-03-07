#include "compositor_state.hpp"

#include <algorithm>
#include <cstddef>
#include <ctime>

extern "C" {
#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>

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

struct RenderSurfaceContext {
    wlr_render_pass* pass = nullptr;
    int32_t offset_x = 0;
    int32_t offset_y = 0;
};

void render_surface_iterator(wlr_surface* surface, int sx, int sy, void* data) {
    if (!surface || !data) {
        return;
    }
    auto* context = static_cast<RenderSurfaceContext*>(data);
    if (!context->pass) {
        return;
    }

    wlr_texture* texture = wlr_surface_get_texture(surface);
    if (!texture) {
        return;
    }

    wlr_render_texture_options options{};
    options.texture = texture;
    options.src_box = wlr_fbox{
        .x = 0.0,
        .y = 0.0,
        .width = static_cast<double>(texture->width),
        .height = static_cast<double>(texture->height),
    };
    options.dst_box = wlr_box{
        .x = context->offset_x + sx,
        .y = context->offset_y + sy,
        .width = static_cast<int>(texture->width),
        .height = static_cast<int>(texture->height),
    };
    options.filter_mode = WLR_SCALE_FILTER_BILINEAR;
    options.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
    wlr_render_pass_add_texture(context->pass, &options);
}

} // namespace

auto CompositorState::setup_layer_shell() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    layer_shell = wlr_layer_shell_v1_create(display, 4);
    if (!layer_shell) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create layer-shell");
    }

    listeners.new_layer_surface.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Listeners*>(reinterpret_cast<char*>(listener) -
                                                  offsetof(Listeners, new_layer_surface));
        list->state->handle_new_layer_surface(static_cast<wlr_layer_surface_v1*>(data));
    };
    wl_signal_add(&layer_shell->events.new_surface, &listeners.new_layer_surface);

    return {};
}

void CompositorState::handle_new_layer_surface(wlr_layer_surface_v1* layer_surface) {
    if (!layer_surface || !layer_surface->surface) {
        return;
    }

    if (!layer_surface->output) {
        layer_surface->output = output;
    }

    // Stable hook allocation keeps wl_listener container_of recovery valid after container growth.
    auto hooks = std::make_unique<LayerSurfaceHooks>();
    auto* hooks_ptr = hooks.get();
    hooks_ptr->state = this;
    hooks_ptr->layer_surface = layer_surface;
    hooks_ptr->surface = layer_surface->surface;
    hooks_ptr->id = next_surface_id++;
    hooks_ptr->layer = static_cast<uint32_t>(layer_surface->pending.layer);

    {
        std::scoped_lock lock(hooks_mutex);
        layer_hooks.push_back(std::move(hooks));
    }

    wl_list_init(&hooks_ptr->surface_commit.link);
    hooks_ptr->surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<LayerSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(LayerSurfaceHooks, surface_commit));
        h->state->handle_layer_surface_commit(h);
    };
    wl_signal_add(&layer_surface->surface->events.commit, &hooks_ptr->surface_commit);

    wl_list_init(&hooks_ptr->surface_map.link);
    hooks_ptr->surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<LayerSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(LayerSurfaceHooks, surface_map));
        h->state->handle_layer_surface_map(h);
    };
    wl_signal_add(&layer_surface->surface->events.map, &hooks_ptr->surface_map);

    wl_list_init(&hooks_ptr->surface_unmap.link);
    hooks_ptr->surface_unmap.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<LayerSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(LayerSurfaceHooks, surface_unmap));
        h->state->handle_layer_surface_unmap(h);
    };
    wl_signal_add(&layer_surface->surface->events.unmap, &hooks_ptr->surface_unmap);

    wl_list_init(&hooks_ptr->surface_destroy.link);
    hooks_ptr->surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<LayerSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(LayerSurfaceHooks, surface_destroy));
        h->state->handle_layer_surface_destroy(h);
    };
    wl_signal_add(&layer_surface->surface->events.destroy, &hooks_ptr->surface_destroy);

    wl_list_init(&hooks_ptr->layer_destroy.link);
    hooks_ptr->layer_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<LayerSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(LayerSurfaceHooks, layer_destroy));
        h->state->handle_layer_surface_destroy(h);
    };
    wl_signal_add(&layer_surface->events.destroy, &hooks_ptr->layer_destroy);

    wl_list_init(&hooks_ptr->new_popup.link);
    hooks_ptr->new_popup.notify = [](wl_listener* listener, void* data) {
        auto* h = reinterpret_cast<LayerSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                       offsetof(LayerSurfaceHooks, new_popup));
        h->state->handle_new_xdg_popup(static_cast<wlr_xdg_popup*>(data));
    };
    wl_signal_add(&layer_surface->events.new_popup, &hooks_ptr->new_popup);

    GOGGLES_LOG_DEBUG("New layer surface: id={} surface={} layer={}", hooks_ptr->id,
                      static_cast<void*>(layer_surface->surface),
                      static_cast<int>(hooks_ptr->layer));
}

void CompositorState::handle_layer_surface_commit(LayerSurfaceHooks* hooks) {
    if (!hooks->layer_surface || !hooks->surface) {
        return;
    }

    if (!hooks->configured && hooks->layer_surface->initial_commit) {
        const auto anchor = hooks->layer_surface->pending.anchor;
        const auto& margin = hooks->layer_surface->pending.margin;
        const auto desired_w = hooks->layer_surface->pending.desired_width;
        const auto desired_h = hooks->layer_surface->pending.desired_height;
        const int out_w = output ? output->width : 0;
        const int out_h = output ? output->height : 0;

        constexpr uint32_t ALL_ANCHORS =
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        constexpr uint32_t HORIZ_ANCHORS =
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        constexpr uint32_t VERT_ANCHORS =
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

        uint32_t width = 0;
        uint32_t height = 0;

        if ((anchor & ALL_ANCHORS) == ALL_ANCHORS) {
            width = static_cast<uint32_t>(std::max(0, out_w - static_cast<int>(margin.left) -
                                                          static_cast<int>(margin.right)));
            height = static_cast<uint32_t>(std::max(0, out_h - static_cast<int>(margin.top) -
                                                           static_cast<int>(margin.bottom)));
        } else if ((anchor & HORIZ_ANCHORS) == HORIZ_ANCHORS) {
            width = static_cast<uint32_t>(std::max(0, out_w - static_cast<int>(margin.left) -
                                                          static_cast<int>(margin.right)));
            height = desired_h > 0 ? desired_h : static_cast<uint32_t>(out_h);
        } else if ((anchor & VERT_ANCHORS) == VERT_ANCHORS) {
            width = desired_w > 0 ? desired_w : static_cast<uint32_t>(out_w);
            height = static_cast<uint32_t>(std::max(0, out_h - static_cast<int>(margin.top) -
                                                           static_cast<int>(margin.bottom)));
        } else {
            width = desired_w > 0 ? desired_w : static_cast<uint32_t>(out_w);
            height = desired_h > 0 ? desired_h : static_cast<uint32_t>(out_h);
        }

        wlr_layer_surface_v1_configure(hooks->layer_surface, width, height);
        hooks->configured = true;
        return;
    }

    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->surface, &now);
    request_present_reset();
}

void CompositorState::handle_layer_surface_map(LayerSurfaceHooks* hooks) {
    hooks->mapped = true;

    if (hooks->layer_surface && hooks->layer_surface->current.keyboard_interactive ==
                                    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
        wlr_seat_set_keyboard(seat, keyboard.get());
        wlr_seat_keyboard_notify_enter(seat, hooks->surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
    }

    request_present_reset();
}

void CompositorState::handle_layer_surface_unmap(LayerSurfaceHooks* hooks) {
    hooks->mapped = false;

    wlr_surface* next_focus_surface = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        next_focus_surface = focused_surface;
    }

    if (seat && hooks->surface && seat->keyboard_state.focused_surface == hooks->surface &&
        next_focus_surface) {
        wlr_seat_set_keyboard(seat, keyboard.get());
        wlr_seat_keyboard_notify_enter(seat, next_focus_surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
    }

    request_present_reset();
}

void CompositorState::handle_layer_surface_destroy(LayerSurfaceHooks* hooks) {
    if (hooks->destroyed) {
        return;
    }
    hooks->destroyed = true;

    wlr_surface* next_focus_surface = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        next_focus_surface = focused_surface;
    }

    if (seat && hooks->surface && seat->keyboard_state.focused_surface == hooks->surface &&
        next_focus_surface) {
        wlr_seat_set_keyboard(seat, keyboard.get());
        wlr_seat_keyboard_notify_enter(seat, next_focus_surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
    }

    detach_listener(hooks->surface_commit);
    detach_listener(hooks->surface_map);
    detach_listener(hooks->surface_unmap);
    detach_listener(hooks->surface_destroy);
    detach_listener(hooks->layer_destroy);
    detach_listener(hooks->new_popup);

    {
        std::scoped_lock lock(hooks_mutex);
        layer_hooks.erase(
            std::remove_if(layer_hooks.begin(), layer_hooks.end(),
                           [hooks](const auto& owned_hooks) { return owned_hooks.get() == hooks; }),
            layer_hooks.end());
    }

    GOGGLES_LOG_DEBUG("Layer surface destroyed: id={}", hooks->id);
}

void CompositorState::render_layer_surfaces(wlr_render_pass* pass, uint32_t target_layer) {
    std::scoped_lock lock(hooks_mutex);
    for (const auto& owned_hooks : layer_hooks) {
        const auto* hooks = owned_hooks.get();
        if (!hooks->mapped || !hooks->layer_surface || !hooks->surface) {
            continue;
        }
        if (hooks->layer != target_layer) {
            continue;
        }

        const auto& state = hooks->layer_surface->current;
        const int out_w = output ? output->width : 0;
        const int out_h = output ? output->height : 0;
        const auto [pos_x, pos_y] = compute_layer_position(state, out_w, out_h);

        RenderSurfaceContext context{};
        context.pass = pass;
        context.offset_x = static_cast<int32_t>(pos_x);
        context.offset_y = static_cast<int32_t>(pos_y);
        wlr_layer_surface_v1_for_each_surface(hooks->layer_surface, render_surface_iterator,
                                              &context);
    }
}

} // namespace goggles::input
