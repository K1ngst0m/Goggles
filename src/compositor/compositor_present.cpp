#include "compositor_state.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <numeric>

extern "C" {
#include <wlr/render/allocator.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>

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

#include <util/drm_format.hpp>
#include <util/drm_fourcc.hpp>
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

void clear_runtime_metric_samples(CompositorState::RuntimeMetricsState& metrics) {
    metrics.game_frame_intervals_ms.fill(0.0F);
    metrics.compositor_latency_samples_ms.fill(0.0F);
    metrics.game_frame_interval_index = 0;
    metrics.compositor_latency_index = 0;
    metrics.game_frame_interval_count = 0;
    metrics.compositor_latency_count = 0;
    metrics.has_last_game_commit_time = false;
    metrics.has_pending_capture_commit_time = false;
    metrics.snapshot = {};
}

void reset_runtime_metrics_state(CompositorState::RuntimeMetricsState& metrics,
                                 wlr_surface* root_surface) {
    metrics.active_root_surface = root_surface;
    clear_runtime_metric_samples(metrics);
}

void push_runtime_metric_sample(
    std::array<float, CompositorState::RuntimeMetricsState::K_SAMPLE_WINDOW>& samples,
    size_t& index, float sample_ms, size_t& count) {
    samples[index] = sample_ms;
    index = (index + 1) % samples.size();
    count = std::min(count + 1, samples.size());
}

auto average_runtime_metric_sample(
    const std::array<float, CompositorState::RuntimeMetricsState::K_SAMPLE_WINDOW>& samples,
    size_t count) -> float {
    if (count == 0) {
        return 0.0F;
    }

    return std::accumulate(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(count),
                           0.0F) /
           static_cast<float>(count);
}

} // namespace

auto CompositorState::initialize_present_output() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    if (!output) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Cannot initialize presentation without output");
    }

    const wlr_drm_format_set* primary_formats =
        wlr_output_get_primary_formats(output, allocator->buffer_caps);

    const wlr_drm_format* selected = nullptr;
    if (primary_formats) {
        constexpr std::array<uint32_t, 2> PREFERRED_FORMATS = {
            util::DRM_FORMAT_XRGB8888,
            util::DRM_FORMAT_ARGB8888,
        };
        for (uint32_t format : PREFERRED_FORMATS) {
            selected = wlr_drm_format_set_get(primary_formats, format);
            if (selected) {
                break;
            }
        }
    }

    if (selected && selected->len > 0) {
        present_modifiers.assign(selected->modifiers, selected->modifiers + selected->len);
        present_format.format = selected->format;
    } else {
        present_modifiers = {util::DRM_FORMAT_MOD_LINEAR};
        present_format.format = util::DRM_FORMAT_XRGB8888;
    }
    present_format.len = present_modifiers.size();
    present_format.capacity = present_modifiers.size();
    present_format.modifiers = present_modifiers.data();

    present_swapchain =
        wlr_swapchain_create(allocator, output->width, output->height, &present_format);
    if (!present_swapchain) {
        GOGGLES_LOG_WARN(
            "Compositor present swapchain unavailable; non-Vulkan presentation disabled");
        return {};
    }

    present_width = static_cast<uint32_t>(output->width);
    present_height = static_cast<uint32_t>(output->height);
    return {};
}

auto CompositorServer::get_presented_frame(uint64_t after_frame_number) const
    -> std::optional<util::ExternalImageFrame> {
    GOGGLES_PROFILE_FUNCTION();
    std::scoped_lock lock(m_impl->state.present_mutex);
    if (!m_impl->state.presented_frame) {
        return std::nullopt;
    }
    const auto& stored = *m_impl->state.presented_frame;
    if (stored.frame_number <= after_frame_number) {
        return std::nullopt;
    }

    util::ExternalImageFrame frame{};
    frame.image.width = stored.image.width;
    frame.image.height = stored.image.height;
    frame.image.stride = stored.image.stride;
    frame.image.offset = stored.image.offset;
    frame.image.format = stored.image.format;
    frame.image.modifier = stored.image.modifier;
    frame.frame_number = stored.frame_number;
    frame.image.handle = stored.image.handle.dup();
    if (!frame.image.handle) {
        return std::nullopt;
    }
    if (stored.sync_fd.valid()) {
        frame.sync_fd = stored.sync_fd.dup();
        if (!frame.sync_fd.valid()) {
            return std::nullopt;
        }
    }
    return frame;
}

void CompositorState::clear_presented_frame() {
    std::scoped_lock lock(present_mutex);
    if (presented_buffer) {
        wlr_buffer_unlock(presented_buffer);
        presented_buffer = nullptr;
    }
    presented_frame.reset();
    presented_surface = nullptr;
    reset_runtime_metrics_state(runtime_metrics, nullptr);
}

void CompositorState::request_present_reset() {
    if (!present_reset_requested.exchange(true, std::memory_order_acq_rel)) {
        wake_event_loop();
    }
}

void CompositorState::update_presented_frame(wlr_surface* surface) {
    GOGGLES_PROFILE_FUNCTION();
    auto target = get_input_target(*this);
    if (!target.root_surface || !surface) {
        return;
    }

    if (target.surface != surface && target.root_surface != surface) {
        return;
    }

    render_surface_to_frame(target);
}

void CompositorState::refresh_presented_frame() {
    GOGGLES_PROFILE_FUNCTION();
    auto target = get_input_target(*this);
    if (!target.root_surface) {
        clear_presented_frame();
        return;
    }

    reset_runtime_metrics_for_target(target.root_surface);

    if (!render_surface_to_frame(target) && presented_surface != target.root_surface) {
        clear_presented_frame();
    }
}

void CompositorState::note_active_surface_commit(wlr_surface* surface) {
    GOGGLES_PROFILE_FUNCTION();
    auto target = get_input_target(*this);
    if (!surface || !target.root_surface) {
        return;
    }

    const bool is_root_commit = surface == target.root_surface;
    const bool is_active_descendant_commit = target.surface && surface == target.surface;
    if (!is_root_commit && !is_active_descendant_commit) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(present_mutex);
    if (runtime_metrics.active_root_surface != target.root_surface) {
        reset_runtime_metrics_state(runtime_metrics, target.root_surface);
    }

    if (is_root_commit) {
        if (runtime_metrics.has_last_game_commit_time) {
            const auto delta_ms = std::chrono::duration<float, std::milli>(
                                      now - runtime_metrics.last_game_commit_time)
                                      .count();
            push_runtime_metric_sample(runtime_metrics.game_frame_intervals_ms,
                                       runtime_metrics.game_frame_interval_index, delta_ms,
                                       runtime_metrics.game_frame_interval_count);
            const float avg_ms = average_runtime_metric_sample(
                runtime_metrics.game_frame_intervals_ms, runtime_metrics.game_frame_interval_count);
            runtime_metrics.snapshot.game_fps = avg_ms > 0.0F ? 1000.0F / avg_ms : 0.0F;
        }

        runtime_metrics.last_game_commit_time = now;
        runtime_metrics.has_last_game_commit_time = true;
    }

    runtime_metrics.pending_capture_commit_time = now;
    runtime_metrics.has_pending_capture_commit_time = true;
}

void CompositorState::reset_runtime_metrics_for_target(wlr_surface* root_surface) {
    std::scoped_lock lock(present_mutex);
    if (runtime_metrics.active_root_surface == root_surface) {
        return;
    }

    reset_runtime_metrics_state(runtime_metrics, root_surface);
}

auto CompositorState::get_runtime_metrics_snapshot() const
    -> util::CompositorRuntimeMetricsSnapshot {
    std::scoped_lock lock(present_mutex);

    auto snapshot = runtime_metrics.snapshot;
    if (!runtime_metrics.has_last_game_commit_time ||
        runtime_metrics.game_frame_interval_count == 0) {
        return snapshot;
    }

    const float avg_ms = average_runtime_metric_sample(runtime_metrics.game_frame_intervals_ms,
                                                       runtime_metrics.game_frame_interval_count);
    if (avg_ms <= 0.0F) {
        return snapshot;
    }

    const auto idle_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() -
                                                 runtime_metrics.last_game_commit_time)
            .count();
    if (idle_ms > std::max(avg_ms * 3.0F, 100.0F)) {
        snapshot.game_fps = 0.0F;
    }

    return snapshot;
}

void CompositorState::render_root_surface_tree(wlr_render_pass* pass, wlr_surface* root_surface) {
    RenderSurfaceContext context{};
    context.pass = pass;

    auto* root_xdg = get_root_xdg_surface(root_surface);
    if (root_xdg && root_xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wlr_xdg_surface_for_each_surface(root_xdg, render_surface_iterator, &context);
    } else {
        wlr_surface_for_each_surface(root_surface, render_surface_iterator, &context);
    }
}

void CompositorState::render_xwayland_popup_surfaces(wlr_render_pass* pass,
                                                     const InputTarget& target) {
    std::scoped_lock lock(hooks_mutex);
    for (const auto& hooks_entry : xwayland_hooks) {
        const auto* hooks = hooks_entry.get();
        if (!hooks->mapped || !hooks->override_redirect || !hooks->xsurface ||
            !hooks->xsurface->surface) {
            continue;
        }

        const auto* popup = hooks->xsurface;
        const auto* parent = popup->parent;
        bool belongs_to_root = false;
        while (parent) {
            if (parent == target.root_xsurface) {
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

        RenderSurfaceContext context{};
        context.pass = pass;
        context.offset_x =
            static_cast<int32_t>(popup->x) - static_cast<int32_t>(target.root_xsurface->x);
        context.offset_y =
            static_cast<int32_t>(popup->y) - static_cast<int32_t>(target.root_xsurface->y);
        wlr_surface_for_each_surface(popup->surface, render_surface_iterator, &context);
    }
}

bool CompositorState::render_surface_to_frame(const InputTarget& target) {
    GOGGLES_PROFILE_SCOPE("CompositorRenderSurfaceToFrame");
    wlr_surface* root_surface = target.root_surface ? target.root_surface : target.surface;
    if (!present_swapchain || !root_surface) {
        return false;
    }

    wlr_texture* root_texture = wlr_surface_get_texture(root_surface);
    if (!root_texture) {
        return false;
    }

    // Export sizing tracks the root surface texture so retained frames stay surface-native.
    const auto desired_width = static_cast<uint32_t>(root_texture->width);
    const auto desired_height = static_cast<uint32_t>(root_texture->height);
    if (desired_width == 0 || desired_height == 0) {
        return false;
    }

    if (present_width != desired_width || present_height != desired_height) {
        wlr_swapchain_destroy(present_swapchain);
        present_swapchain = wlr_swapchain_create(allocator, static_cast<int>(desired_width),
                                                 static_cast<int>(desired_height), &present_format);
        if (!present_swapchain) {
            GOGGLES_LOG_WARN("Compositor present swapchain unavailable; non-Vulkan presentation "
                             "disabled");
            present_width = 0;
            present_height = 0;
            return false;
        }
        present_width = desired_width;
        present_height = desired_height;
    }

    wlr_buffer* buffer = wlr_swapchain_acquire(present_swapchain);
    if (!buffer) {
        return false;
    }

    wlr_render_pass* pass = wlr_renderer_begin_buffer_pass(renderer, buffer, nullptr);
    if (!pass) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    render_layer_surfaces(pass, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND);
    render_layer_surfaces(pass, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM);
    render_root_surface_tree(pass, root_surface);
    if (target.root_xsurface) {
        render_xwayland_popup_surfaces(pass, target);
    }
    render_layer_surfaces(pass, ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    render_layer_surfaces(pass, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
    render_cursor_overlay(pass);

    if (!wlr_render_pass_submit(pass)) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    wlr_dmabuf_attributes attribs{};
    if (!wlr_buffer_get_dmabuf(buffer, &attribs)) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    if (attribs.n_planes != 1) {
        GOGGLES_LOG_DEBUG("Skipping multi-plane DMA-BUF output (planes={})", attribs.n_planes);
        wlr_buffer_unlock(buffer);
        return false;
    }

    auto dup_fd = util::UniqueFd::dup_from(attribs.fd[0]);
    if (!dup_fd) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    const auto capture_time = std::chrono::steady_clock::now();

    std::scoped_lock lock(present_mutex);
    if (runtime_metrics.active_root_surface != root_surface) {
        reset_runtime_metrics_state(runtime_metrics, root_surface);
    }
    if (presented_buffer) {
        wlr_buffer_unlock(presented_buffer);
        presented_buffer = nullptr;
    }

    presented_buffer = buffer;

    util::ExternalImageFrame frame{};
    frame.image.width = static_cast<uint32_t>(attribs.width);
    frame.image.height = static_cast<uint32_t>(attribs.height);
    frame.image.stride = attribs.stride[0];
    frame.image.offset = attribs.offset[0];
    frame.image.format = util::drm_to_vk_format(attribs.format);
    frame.image.modifier = attribs.modifier;
    frame.image.handle = std::move(dup_fd);
    frame.frame_number = ++presented_frame_number;

    // Export the acquire fence from the root surface so Vulkan waits on compositor writes.
    wlr_linux_drm_syncobj_surface_v1_state* syncobj_state =
        wlr_linux_drm_syncobj_v1_get_surface_state(root_surface);
    if (syncobj_state && syncobj_state->acquire_timeline) {
        int sync_file = wlr_drm_syncobj_timeline_export_sync_file(syncobj_state->acquire_timeline,
                                                                  syncobj_state->acquire_point);
        if (sync_file >= 0) {
            frame.sync_fd = util::UniqueFd{sync_file};
        }
    }

    // Release stays tied to the exported buffer so wlroots can retire it after import completes.
    if (syncobj_state && syncobj_state->release_timeline) {
        wlr_linux_drm_syncobj_v1_state_signal_release_with_buffer(syncobj_state, buffer);
    }

    if (runtime_metrics.has_pending_capture_commit_time) {
        const auto latency_ms = std::chrono::duration<float, std::milli>(
                                    capture_time - runtime_metrics.pending_capture_commit_time)
                                    .count();
        push_runtime_metric_sample(runtime_metrics.compositor_latency_samples_ms,
                                   runtime_metrics.compositor_latency_index, latency_ms,
                                   runtime_metrics.compositor_latency_count);
        runtime_metrics.snapshot.compositor_latency_ms =
            average_runtime_metric_sample(runtime_metrics.compositor_latency_samples_ms,
                                          runtime_metrics.compositor_latency_count);
        runtime_metrics.has_pending_capture_commit_time = false;
    }

    presented_frame = std::move(frame);
    presented_surface = root_surface;
    return true;
}

} // namespace goggles::input
