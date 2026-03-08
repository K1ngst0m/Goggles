#pragma once

#include "compositor_protocol_hooks.hpp"
#include "compositor_server.hpp"
#include "compositor_targets.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

// NOLINTBEGIN(readability-identifier-naming)
struct wlr_allocator;
struct wlr_backend;
struct wlr_buffer;
struct wlr_compositor;
struct wlr_layer_shell_v1;
struct wlr_linux_drm_syncobj_manager_v1;
struct wlr_output;
struct wlr_output_layout;
struct wlr_pointer_constraint_v1;
struct wlr_pointer_constraints_v1;
struct wlr_relative_pointer_manager_v1;
struct wlr_render_pass;
struct wlr_renderer;
struct wlr_seat;
struct wlr_surface;
struct wlr_swapchain;
struct wlr_texture;
struct wlr_xcursor;
struct wlr_xcursor_theme;
struct wlr_xdg_popup;
struct wlr_xdg_shell;
struct wlr_xdg_toplevel;
struct wlr_xwayland;
struct wlr_xwayland_surface;
// NOLINTEND(readability-identifier-naming)
}

#include <util/queues.hpp>
#include <util/unique_fd.hpp>

namespace goggles::input {

using ::wl_display;
using ::wl_event_loop;
using ::wl_event_source;
using ::wl_listener;
using ::wlr_allocator;
using ::wlr_backend;
using ::wlr_buffer;
using ::wlr_compositor;
using ::wlr_layer_shell_v1;
using ::wlr_linux_drm_syncobj_manager_v1;
using ::wlr_output;
using ::wlr_output_layout;
using ::wlr_pointer_constraint_v1;
using ::wlr_pointer_constraints_v1;
using ::wlr_relative_pointer_manager_v1;
using ::wlr_render_pass;
using ::wlr_renderer;
using ::wlr_seat;
using ::wlr_surface;
using ::wlr_swapchain;
using ::wlr_texture;
using ::wlr_xcursor;
using ::wlr_xcursor_theme;
using ::wlr_xdg_popup;
using ::wlr_xdg_shell;
using ::wlr_xdg_toplevel;
using ::wlr_xwayland;
using ::wlr_xwayland_surface;

struct SurfaceResizeRequest {
    uint32_t surface_id = 0;
    SurfaceResizeInfo resize;
};

struct CursorFrame {
    wlr_texture* texture = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t hotspot_x = 0;
    uint32_t hotspot_y = 0;
    uint32_t delay_ms = 0;
};

struct Listeners {
    CompositorState* state = nullptr;

    wl_listener new_xdg_toplevel{};
    wl_listener new_xdg_popup{};
    wl_listener new_xwayland_surface{};
    wl_listener new_pointer_constraint{};
    wl_listener new_layer_surface{};
};

struct KeyboardDeleter {
    void operator()(wlr_keyboard* keyboard) const {
        if (keyboard) {
            wlr_keyboard_finish(keyboard);
            std::default_delete<wlr_keyboard>{}(keyboard);
        }
    }
};

using UniqueKeyboard = std::unique_ptr<wlr_keyboard, KeyboardDeleter>;

struct CompositorState {
    struct RuntimeMetricsState {
        static constexpr size_t K_SAMPLE_WINDOW = 120;

        wlr_surface* active_root_surface = nullptr;
        std::array<float, K_SAMPLE_WINDOW> game_frame_intervals_ms{};
        std::array<float, K_SAMPLE_WINDOW> compositor_latency_samples_ms{};
        size_t game_frame_interval_index = 0;
        size_t compositor_latency_index = 0;
        size_t game_frame_interval_count = 0;
        size_t compositor_latency_count = 0;
        std::chrono::steady_clock::time_point last_game_commit_time;
        std::chrono::steady_clock::time_point pending_capture_commit_time;
        bool has_last_game_commit_time = false;
        bool has_pending_capture_commit_time = false;
        util::CompositorRuntimeMetricsSnapshot snapshot;
    };

    util::SPSCQueue<InputEvent> event_queue{64};
    util::SPSCQueue<SurfaceResizeRequest> resize_queue{64};
    wl_display* display = nullptr;
    wl_event_loop* event_loop = nullptr;
    wl_event_source* event_source = nullptr;
    wlr_backend* backend = nullptr;
    wlr_renderer* renderer = nullptr;
    wlr_allocator* allocator = nullptr;
    wlr_compositor* compositor = nullptr;
    wlr_xdg_shell* xdg_shell = nullptr;
    wlr_seat* seat = nullptr;
    wlr_xwayland* xwayland = nullptr;
    wlr_relative_pointer_manager_v1* relative_pointer_manager = nullptr;
    wlr_pointer_constraints_v1* pointer_constraints = nullptr;
    wlr_pointer_constraint_v1* active_constraint = nullptr;
    UniqueKeyboard keyboard;
    xkb_context* xkb_ctx = nullptr;
    wlr_output_layout* output_layout = nullptr;
    wlr_output* output = nullptr;
    wlr_surface* focused_surface = nullptr;
    wlr_xwayland_surface* focused_xsurface = nullptr;
    wlr_surface* keyboard_entered_surface = nullptr;
    wlr_surface* pointer_entered_surface = nullptr;
    wlr_swapchain* present_swapchain = nullptr;
    std::vector<uint64_t> present_modifiers;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    wlr_surface* cursor_surface = nullptr;
    wlr_xcursor_theme* cursor_theme = nullptr;
    wlr_xcursor* cursor_shape = nullptr;
    wlr_buffer* presented_buffer = nullptr;
    wlr_surface* presented_surface = nullptr;
    uint64_t presented_frame_number = 0;
    std::jthread compositor_thread;
    std::vector<CursorFrame> cursor_frames;
    std::vector<std::unique_ptr<XdgToplevelHooks>> xdg_hooks;
    std::vector<std::unique_ptr<XdgPopupHooks>> xdg_popup_hooks;
    std::vector<std::unique_ptr<XWaylandSurfaceHooks>> xwayland_hooks;
    std::vector<std::unique_ptr<ConstraintHooks>> constraint_hooks;
    std::vector<std::unique_ptr<LayerSurfaceHooks>> layer_hooks;
    wlr_layer_shell_v1* layer_shell = nullptr;
    wlr_linux_drm_syncobj_manager_v1* syncobj_manager = nullptr;
    wlr_drm_format present_format{};
    std::string wayland_socket_name;
    mutable std::mutex hooks_mutex;
    mutable std::mutex present_mutex;
    std::optional<util::ExternalImageFrame> presented_frame;
    RuntimeMetricsState runtime_metrics;
    Listeners listeners;
    uint32_t present_width = 0;
    uint32_t present_height = 0;
    util::UniqueFd event_fd;
    uint32_t next_surface_id = 1;
    static constexpr uint32_t NO_FOCUS_TARGET = 0;
    std::atomic<uint32_t> pending_focus_target{NO_FOCUS_TARGET};
    std::atomic<bool> cursor_visible{true};
    bool cursor_initialized = false;
    std::atomic<bool> pointer_locked{false};
    std::atomic<bool> present_reset_requested{false};

    CompositorState();

    [[nodiscard]] auto setup_base_components() -> Result<void>;
    [[nodiscard]] auto create_allocator() -> Result<void>;
    [[nodiscard]] auto create_compositor() -> Result<void>;
    [[nodiscard]] auto create_output_layout() -> Result<void>;
    [[nodiscard]] auto setup_xdg_shell() -> Result<void>;
    [[nodiscard]] auto setup_layer_shell() -> Result<void>;
    [[nodiscard]] auto setup_input_devices() -> Result<void>;
    [[nodiscard]] auto setup_event_loop_fd() -> Result<void>;
    [[nodiscard]] auto bind_wayland_socket() -> Result<void>;
    [[nodiscard]] auto setup_xwayland() -> Result<void>;
    [[nodiscard]] auto x11_display_name() const -> std::string;
    [[nodiscard]] auto start_backend() -> Result<void>;
    [[nodiscard]] auto setup_output() -> Result<void>;
    [[nodiscard]] auto initialize_present_output() -> Result<void>;
    [[nodiscard]] auto setup_cursor_theme() -> Result<void>;
    void start_compositor_thread();
    void run_compositor_display_loop();
    void teardown();

    bool wake_event_loop();
    void request_focus_target(uint32_t surface_id);
    void request_surface_resize(uint32_t surface_id, const SurfaceResizeInfo& resize);
    void process_input_events();
    void handle_focus_request();
    void handle_surface_resize_requests();
    void handle_key_event(const InputEvent& event, uint32_t time);
    void handle_pointer_motion_event(const InputEvent& event, uint32_t time);
    void handle_pointer_button_event(const InputEvent& event, uint32_t time);
    void handle_pointer_axis_event(const InputEvent& event, uint32_t time);
    void prepare_keyboard_dispatch(const InputTarget& target);
    void prepare_pointer_motion_dispatch(const InputTarget& target, double local_x, double local_y);
    void prepare_pointer_button_dispatch(const InputTarget& target, double local_x, double local_y);
    void prepare_pointer_axis_dispatch(const InputTarget& target, double local_x, double local_y);

    void handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel);
    void handle_new_xdg_popup(wlr_xdg_popup* popup);
    void handle_xdg_surface_commit(XdgToplevelHooks* hooks);
    void handle_xdg_surface_map(XdgToplevelHooks* hooks);
    void handle_xdg_surface_destroy(XdgToplevelHooks* hooks);
    void handle_xdg_surface_ack_configure(XdgToplevelHooks* hooks);
    void handle_xdg_popup_commit(XdgPopupHooks* hooks);
    void handle_xdg_popup_map(XdgPopupHooks* hooks);
    void handle_xdg_popup_destroy(XdgPopupHooks* hooks);
    void handle_xdg_popup_ack_configure(XdgPopupHooks* hooks);

    void handle_new_xwayland_surface(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface);
    void handle_xwayland_surface_map_request(XWaylandSurfaceHooks* hooks);
    void handle_xwayland_surface_commit(XWaylandSurfaceHooks* hooks);
    void handle_xwayland_surface_destroy(wlr_xwayland_surface* xsurface);

    void handle_new_pointer_constraint(wlr_pointer_constraint_v1* constraint);
    void handle_constraint_set_region(ConstraintHooks* hooks);
    void handle_constraint_destroy(ConstraintHooks* hooks);
    void activate_constraint(wlr_pointer_constraint_v1* constraint);
    void deactivate_constraint();
    void focus_surface(wlr_surface* surface);
    void focus_xwayland_surface(wlr_xwayland_surface* xsurface);
    bool focus_surface_by_id(uint32_t surface_id);
    void apply_surface_resize_request(const SurfaceResizeRequest& request);
    void reset_cursor_for_surface(wlr_surface* surface);
    void apply_cursor_hint_if_needed();
    void auto_focus_next_surface();
    void update_cursor_position(const InputEvent& event, const InputTarget& root_target);
    [[nodiscard]] auto get_surfaces_snapshot() const -> std::vector<SurfaceInfo>;

    void handle_new_layer_surface(wlr_layer_surface_v1* layer_surface);
    void handle_layer_surface_commit(LayerSurfaceHooks* hooks);
    void handle_layer_surface_map(LayerSurfaceHooks* hooks);
    void handle_layer_surface_unmap(LayerSurfaceHooks* hooks);
    void handle_layer_surface_destroy(LayerSurfaceHooks* hooks);
    void render_layer_surfaces(wlr_render_pass* pass, uint32_t target_layer);

    void clear_presented_frame();
    void request_present_reset();
    void update_presented_frame(wlr_surface* surface);
    void refresh_presented_frame();
    void note_active_surface_commit(wlr_surface* surface);
    void reset_runtime_metrics_for_target(wlr_surface* root_surface);
    [[nodiscard]] auto get_runtime_metrics_snapshot() const
        -> util::CompositorRuntimeMetricsSnapshot;
    void render_root_surface_tree(wlr_render_pass* pass, wlr_surface* root_surface);
    void render_xwayland_popup_surfaces(wlr_render_pass* pass, const InputTarget& target);
    void render_cursor_overlay(wlr_render_pass* pass) const;
    bool render_surface_to_frame(const InputTarget& target);

    void clear_cursor_theme();
    [[nodiscard]] auto get_cursor_frame(uint32_t time_msec) const -> const CursorFrame*;
    void set_cursor_visible(bool visible);
};

struct CompositorServer::Impl {
    CompositorState state;
};

} // namespace goggles::input
