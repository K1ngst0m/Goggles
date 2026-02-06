#include "compositor_server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <limits>
#include <linux/input-event-codes.h>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/xcursor.h>
#include <xkbcommon/xkbcommon.h>

// xwayland.h contains 'char *class' which conflicts with C++ keyword
#define class class_
#include <wlr/xwayland/xwayland.h>
#undef class

#include <wlr/types/wlr_xdg_shell.h>
}

#include <util/drm_format.hpp>
#include <util/drm_fourcc.hpp>
#include <util/logging.hpp>
#include <util/queues.hpp>
#include <util/unique_fd.hpp>

namespace goggles::input {

namespace {

auto get_time_msec() -> uint32_t {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

auto sdl_to_linux_keycode(SDL_Scancode scancode) -> uint32_t {
    switch (scancode) {
    case SDL_SCANCODE_A:
        return KEY_A;
    case SDL_SCANCODE_B:
        return KEY_B;
    case SDL_SCANCODE_C:
        return KEY_C;
    case SDL_SCANCODE_D:
        return KEY_D;
    case SDL_SCANCODE_E:
        return KEY_E;
    case SDL_SCANCODE_F:
        return KEY_F;
    case SDL_SCANCODE_G:
        return KEY_G;
    case SDL_SCANCODE_H:
        return KEY_H;
    case SDL_SCANCODE_I:
        return KEY_I;
    case SDL_SCANCODE_J:
        return KEY_J;
    case SDL_SCANCODE_K:
        return KEY_K;
    case SDL_SCANCODE_L:
        return KEY_L;
    case SDL_SCANCODE_M:
        return KEY_M;
    case SDL_SCANCODE_N:
        return KEY_N;
    case SDL_SCANCODE_O:
        return KEY_O;
    case SDL_SCANCODE_P:
        return KEY_P;
    case SDL_SCANCODE_Q:
        return KEY_Q;
    case SDL_SCANCODE_R:
        return KEY_R;
    case SDL_SCANCODE_S:
        return KEY_S;
    case SDL_SCANCODE_T:
        return KEY_T;
    case SDL_SCANCODE_U:
        return KEY_U;
    case SDL_SCANCODE_V:
        return KEY_V;
    case SDL_SCANCODE_W:
        return KEY_W;
    case SDL_SCANCODE_X:
        return KEY_X;
    case SDL_SCANCODE_Y:
        return KEY_Y;
    case SDL_SCANCODE_Z:
        return KEY_Z;
    case SDL_SCANCODE_1:
        return KEY_1;
    case SDL_SCANCODE_2:
        return KEY_2;
    case SDL_SCANCODE_3:
        return KEY_3;
    case SDL_SCANCODE_4:
        return KEY_4;
    case SDL_SCANCODE_5:
        return KEY_5;
    case SDL_SCANCODE_6:
        return KEY_6;
    case SDL_SCANCODE_7:
        return KEY_7;
    case SDL_SCANCODE_8:
        return KEY_8;
    case SDL_SCANCODE_9:
        return KEY_9;
    case SDL_SCANCODE_0:
        return KEY_0;
    case SDL_SCANCODE_ESCAPE:
        return KEY_ESC;
    case SDL_SCANCODE_RETURN:
        return KEY_ENTER;
    case SDL_SCANCODE_BACKSPACE:
        return KEY_BACKSPACE;
    case SDL_SCANCODE_TAB:
        return KEY_TAB;
    case SDL_SCANCODE_SPACE:
        return KEY_SPACE;
    case SDL_SCANCODE_UP:
        return KEY_UP;
    case SDL_SCANCODE_DOWN:
        return KEY_DOWN;
    case SDL_SCANCODE_LEFT:
        return KEY_LEFT;
    case SDL_SCANCODE_RIGHT:
        return KEY_RIGHT;
    case SDL_SCANCODE_LCTRL:
        return KEY_LEFTCTRL;
    case SDL_SCANCODE_LSHIFT:
        return KEY_LEFTSHIFT;
    case SDL_SCANCODE_LALT:
        return KEY_LEFTALT;
    case SDL_SCANCODE_RCTRL:
        return KEY_RIGHTCTRL;
    case SDL_SCANCODE_RSHIFT:
        return KEY_RIGHTSHIFT;
    case SDL_SCANCODE_RALT:
        return KEY_RIGHTALT;
    default:
        return 0;
    }
}

auto sdl_to_linux_button(uint8_t sdl_button) -> uint32_t {
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        return BTN_LEFT;
    case SDL_BUTTON_MIDDLE:
        return BTN_MIDDLE;
    case SDL_BUTTON_RIGHT:
        return BTN_RIGHT;
    case SDL_BUTTON_X1:
        return BTN_SIDE;
    case SDL_BUTTON_X2:
        return BTN_EXTRA;
    default:
        // SDL buttons beyond X2: map to BTN_FORWARD, BTN_BACK, BTN_TASK
        if (sdl_button == 6) {
            return BTN_FORWARD;
        }
        if (sdl_button == 7) {
            return BTN_BACK;
        }
        if (sdl_button == 8) {
            return BTN_TASK;
        }
        // Fallback: BTN_MISC + offset for unknown buttons
        if (sdl_button > 8) {
            GOGGLES_LOG_TRACE("Unmapped SDL button {} -> BTN_MISC+{}", sdl_button, sdl_button - 8);
            return BTN_MISC + (sdl_button - 8);
        }
        return 0;
    }
}

struct RenderSurfaceContext {
    wlr_render_pass* pass = nullptr;
    int32_t offset_x = 0;
    int32_t offset_y = 0;
};

enum class WlrLogFormatStatus : std::uint8_t { ok, null_format, format_error };

struct FormattedWlrMessage {
    std::string message;
    WlrLogFormatStatus status = WlrLogFormatStatus::ok;
};

auto format_wlr_message(const char* format, va_list args) -> FormattedWlrMessage {
    if (!format) {
        return {.message = {}, .status = WlrLogFormatStatus::null_format};
    }

    std::array<char, 512> buffer{};
    va_list args_copy;
    va_copy(args_copy, args);
    int length = std::vsnprintf(buffer.data(), buffer.size(), format, args_copy);
    va_end(args_copy);

    if (length < 0) {
        return {.message = {}, .status = WlrLogFormatStatus::format_error};
    }

    if (static_cast<size_t>(length) < buffer.size()) {
        std::string message(buffer.data(), static_cast<size_t>(length));
        while (!message.empty() && message.back() == '\n') {
            message.pop_back();
        }
        return {.message = std::move(message), .status = WlrLogFormatStatus::ok};
    }

    std::string message(static_cast<size_t>(length) + 1, '\0');
    va_copy(args_copy, args);
    std::vsnprintf(message.data(), message.size(), format, args_copy);
    va_end(args_copy);
    message.resize(static_cast<size_t>(length));
    while (!message.empty() && message.back() == '\n') {
        message.pop_back();
    }
    return {.message = std::move(message), .status = WlrLogFormatStatus::ok};
}

auto wlr_importance_from_log_level(spdlog::level::level_enum level) -> wlr_log_importance {
    if (level <= spdlog::level::debug) {
        return WLR_DEBUG;
    }
    if (level <= spdlog::level::info) {
        return WLR_INFO;
    }
    if (level <= spdlog::level::critical) {
        return WLR_ERROR;
    }
    return WLR_SILENT;
}

void wlr_log_bridge(wlr_log_importance importance, const char* format, va_list args) {
    const FormattedWlrMessage formatted = format_wlr_message(format, args);
    if (formatted.status != WlrLogFormatStatus::ok) {
        if (formatted.status == WlrLogFormatStatus::null_format) {
            GOGGLES_LOG_WARN("[wlr] log formatting failed: null format string");
        } else {
            GOGGLES_LOG_WARN("[wlr] log formatting failed for format '{}'",
                             format ? format : "<null>");
        }
        return;
    }

    if (formatted.message.empty()) {
        return;
    }

    switch (importance) {
    case WLR_ERROR:
        GOGGLES_LOG_ERROR("[wlr] {}", formatted.message);
        return;
    case WLR_INFO:
        GOGGLES_LOG_INFO("[wlr] {}", formatted.message);
        return;
    case WLR_DEBUG:
        GOGGLES_LOG_DEBUG("[wlr] {}", formatted.message);
        return;
    case WLR_SILENT:
    case WLR_LOG_IMPORTANCE_LAST:
        return;
    }
}

auto initialize_wlroots_logging() -> void {
    const auto level = goggles::get_logger()->level();
    wlr_log_init(wlr_importance_from_log_level(level), wlr_log_bridge);
}

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

    wlr_render_texture_options tex_opts{};
    tex_opts.texture = texture;
    tex_opts.src_box = wlr_fbox{
        .x = 0.0,
        .y = 0.0,
        .width = static_cast<double>(texture->width),
        .height = static_cast<double>(texture->height),
    };
    tex_opts.dst_box = wlr_box{
        .x = context->offset_x + sx,
        .y = context->offset_y + sy,
        .width = static_cast<int>(texture->width),
        .height = static_cast<int>(texture->height),
    };
    tex_opts.filter_mode = WLR_SCALE_FILTER_BILINEAR;
    tex_opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
    wlr_render_pass_add_texture(context->pass, &tex_opts);
}

// XWayland/helper tools emit stderr warnings (xkbcomp, event loop errors).
// Suppress at info+ levels; wlroots logs use the project logger.
class StderrSuppressor {
public:
    StderrSuppressor() {
        if (goggles::get_logger()->level() <= spdlog::level::debug) {
            return;
        }
        m_saved_stderr = dup(STDERR_FILENO);
        if (m_saved_stderr < 0) {
            return;
        }
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        } else {
            close(m_saved_stderr);
            m_saved_stderr = -1;
        }
    }
    ~StderrSuppressor() {
        if (m_saved_stderr >= 0) {
            dup2(m_saved_stderr, STDERR_FILENO);
            close(m_saved_stderr);
        }
    }
    StderrSuppressor(const StderrSuppressor&) = delete;
    StderrSuppressor& operator=(const StderrSuppressor&) = delete;

private:
    int m_saved_stderr = -1;
};

auto bind_wayland_socket(wl_display* display) -> Result<std::string> {
    for (int display_num = 0; display_num < 10; ++display_num) {
        std::array<char, 32> socket_name{};
        std::snprintf(socket_name.data(), socket_name.size(), "goggles-%d", display_num);
        if (wl_display_add_socket(display, socket_name.data()) == 0) {
            return std::string(socket_name.data());
        }
    }
    return make_error<std::string>(ErrorCode::input_init_failed,
                                   "No available goggles sockets (goggles-0..9 all bound)");
}

struct KeyboardDeleter {
    void operator()(wlr_keyboard* kb) const {
        if (kb) {
            wlr_keyboard_finish(kb);
            delete kb;
        }
    }
};
using UniqueKeyboard = std::unique_ptr<wlr_keyboard, KeyboardDeleter>;

} // anonymous namespace

struct CompositorServer::Impl {
    struct SurfaceResizeRequest {
        uint32_t surface_id = 0;
        SurfaceResizeInfo resize;
    };

    struct XWaylandSurfaceHooks {
        Impl* impl = nullptr;
        wlr_xwayland_surface* xsurface = nullptr;
        uint32_t id = 0;
        std::string title;
        std::string class_name;
        // XWayland map_request can arrive before associate (surface becomes available).
        bool map_requested = false;
        bool mapped = false;
        bool override_redirect = false;
        wl_listener associate{};
        wl_listener map_request{};
        wl_listener commit{};
        wl_listener destroy{};
    };

    struct XdgPopupHooks {
        Impl* impl = nullptr;
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
        Impl* impl = nullptr;
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

    struct Listeners {
        Impl* impl = nullptr;

        wl_listener new_xdg_toplevel{};
        wl_listener new_xdg_popup{};
        wl_listener new_xwayland_surface{};
        wl_listener new_pointer_constraint{};
    };

    struct ConstraintHooks {
        Impl* impl = nullptr;
        wlr_pointer_constraint_v1* constraint = nullptr;
        wl_listener set_region{};
        wl_listener destroy{};
    };

    struct CursorFrame {
        wlr_texture* texture = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t hotspot_x = 0;
        uint32_t hotspot_y = 0;
        uint32_t delay_ms = 0;
    };

    struct InputTarget {
        wlr_surface* surface = nullptr;
        wlr_xwayland_surface* xsurface = nullptr;
        wlr_surface* root_surface = nullptr;
        wlr_xwayland_surface* root_xsurface = nullptr;
        double offset_x = 0.0;
        double offset_y = 0.0;
    };
    // Fields ordered for optimal padding
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
    std::array<uint64_t, 1> present_modifiers{};
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
    std::vector<XdgToplevelHooks*> xdg_hooks;
    std::vector<std::unique_ptr<XdgPopupHooks>> xdg_popup_hooks;
    std::vector<XWaylandSurfaceHooks*> xwayland_hooks;
    wlr_drm_format present_format{};
    std::string wayland_socket_name;
    mutable std::mutex hooks_mutex;
    mutable std::mutex present_mutex;
    std::optional<util::ExternalImageFrame> presented_frame;
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

    Impl() { listeners.impl = this; }

    [[nodiscard]] auto setup_base_components() -> Result<void>;
    [[nodiscard]] auto create_allocator() -> Result<void>;
    [[nodiscard]] auto create_compositor() -> Result<void>;
    [[nodiscard]] auto create_output_layout() -> Result<void>;
    [[nodiscard]] auto setup_xdg_shell() -> Result<void>;
    [[nodiscard]] auto setup_input_devices() -> Result<void>;
    [[nodiscard]] auto setup_event_loop_fd() -> Result<void>;
    [[nodiscard]] auto setup_xwayland() -> Result<void>;
    [[nodiscard]] auto start_backend() -> Result<void>;
    [[nodiscard]] auto setup_output() -> Result<void>;
    [[nodiscard]] auto setup_cursor_theme() -> Result<void>;
    void start_compositor_thread();

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
    void update_presented_frame(wlr_surface* surface);
    void refresh_presented_frame();
    void clear_presented_frame();
    void request_present_reset();
    bool render_surface_to_frame(const InputTarget& target);
    void render_root_surface_tree(wlr_render_pass* pass, wlr_surface* root_surface);
    void render_xwayland_popup_surfaces(wlr_render_pass* pass, const InputTarget& target);
    void render_cursor_overlay(wlr_render_pass* pass) const;
    void set_cursor_visible(bool visible);
    void clear_cursor_theme();
    [[nodiscard]] auto get_cursor_frame(uint32_t time_msec) const -> const CursorFrame*;

    [[nodiscard]] auto get_root_input_target() -> InputTarget;
    [[nodiscard]] auto resolve_input_target(const InputTarget& root_target,
                                            bool use_pointer_hit_test) -> InputTarget;
    [[nodiscard]] auto get_input_target() -> InputTarget;
    [[nodiscard]] auto get_root_xdg_surface(wlr_surface* surface) const -> wlr_xdg_surface*;
    [[nodiscard]] auto get_xdg_popup_position(const XdgPopupHooks* hooks) const
        -> std::pair<double, double>;
    [[nodiscard]] auto get_cursor_bounds(const InputTarget& root_target) const
        -> std::optional<std::pair<double, double>>;
    [[nodiscard]] auto get_surface_local_coords(const InputTarget& target) const
        -> std::pair<double, double>;
    [[nodiscard]] auto get_surface_extent(wlr_surface* surface) const
        -> std::optional<std::pair<uint32_t, uint32_t>>;
    void reset_cursor_for_surface(wlr_surface* surface);
    void apply_cursor_hint_if_needed();
    void auto_focus_next_surface();
    void update_cursor_position(const InputEvent& event, const InputTarget& root_target);
    void apply_surface_resize_request(const SurfaceResizeRequest& request);
};

CompositorServer::CompositorServer() : m_impl(std::make_unique<Impl>()) {}

CompositorServer::~CompositorServer() {
    stop();
}

auto CompositorServer::create() -> ResultPtr<CompositorServer> {
    auto server = std::make_unique<CompositorServer>();

    auto start_result = server->start();
    if (!start_result) {
        return make_result_ptr_error<CompositorServer>(start_result.error().code,
                                                       start_result.error().message);
    }

    return make_result_ptr(std::move(server));
}

auto CompositorServer::x11_display() const -> std::string {
    if (m_impl->xwayland && m_impl->xwayland->display_name) {
        return m_impl->xwayland->display_name;
    }
    return "";
}

auto CompositorServer::wayland_display() const -> std::string {
    return m_impl->wayland_socket_name;
}

auto CompositorServer::forward_key(const SDL_KeyboardEvent& event) -> Result<void> {
    uint32_t linux_keycode = sdl_to_linux_keycode(event.scancode);
    if (linux_keycode == 0) {
        GOGGLES_LOG_TRACE("Unmapped key scancode={}, down={}", static_cast<int>(event.scancode),
                          event.down);
        return {};
    }

    GOGGLES_LOG_TRACE("Forward key scancode={}, down={} -> linux_keycode={}",
                      static_cast<int>(event.scancode), event.down, linux_keycode);
    InputEvent input_event{};
    input_event.type = InputEventType::key;
    input_event.code = linux_keycode;
    input_event.pressed = event.down;
    if (!inject_event(input_event)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped key event");
    }
    return {};
}

auto CompositorServer::forward_mouse_button(const SDL_MouseButtonEvent& event) -> Result<void> {
    uint32_t button = sdl_to_linux_button(event.button);
    if (button == 0) {
        GOGGLES_LOG_TRACE("Unmapped mouse button {}", event.button);
        return {};
    }

    InputEvent input_event{};
    input_event.type = InputEventType::pointer_button;
    input_event.code = button;
    input_event.pressed = event.down;
    if (!inject_event(input_event)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped button event");
    }
    return {};
}

auto CompositorServer::forward_mouse_motion(const SDL_MouseMotionEvent& event) -> Result<void> {
    InputEvent input_event{};
    input_event.type = InputEventType::pointer_motion;
    input_event.dx = static_cast<double>(event.xrel);
    input_event.dy = static_cast<double>(event.yrel);
    if (!inject_event(input_event)) {
        GOGGLES_LOG_DEBUG("Input queue full, dropped motion event");
    }
    return {};
}

auto CompositorServer::forward_mouse_wheel(const SDL_MouseWheelEvent& event) -> Result<void> {
    if (event.y != 0) {
        // SDL: positive = up, Wayland: positive = down; negate to match
        InputEvent input_event{};
        input_event.type = InputEventType::pointer_axis;
        input_event.value = static_cast<double>(-event.y) * 15.0;
        input_event.horizontal = false;
        if (!inject_event(input_event)) {
            GOGGLES_LOG_DEBUG("Input queue full, dropped axis event");
        }
    }

    if (event.x != 0) {
        InputEvent input_event{};
        input_event.type = InputEventType::pointer_axis;
        input_event.value = static_cast<double>(event.x) * 15.0;
        input_event.horizontal = true;
        if (!inject_event(input_event)) {
            GOGGLES_LOG_DEBUG("Input queue full, dropped axis event");
        }
    }

    return {};
}

auto CompositorServer::Impl::setup_base_components() -> Result<void> {
    initialize_wlroots_logging();

    display = wl_display_create();
    if (!display) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create Wayland display");
    }

    event_loop = wl_display_get_event_loop(display);
    if (!event_loop) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to get event loop");
    }

    backend = wlr_headless_backend_create(event_loop);
    if (!backend) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create headless backend");
    }

    renderer = wlr_renderer_autocreate(backend);
    if (!renderer) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create renderer");
    }

    if (!wlr_renderer_init_wl_display(renderer, display)) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to initialize renderer protocols");
    }

    return {};
}

auto CompositorServer::Impl::create_allocator() -> Result<void> {
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create allocator");
    }
    return {};
}

auto CompositorServer::Impl::create_compositor() -> Result<void> {
    compositor = wlr_compositor_create(display, 6, renderer);
    if (!compositor) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create compositor");
    }
    return {};
}

auto CompositorServer::Impl::create_output_layout() -> Result<void> {
    output_layout = wlr_output_layout_create(display);
    if (!output_layout) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create output layout");
    }
    return {};
}

auto CompositorServer::Impl::setup_xdg_shell() -> Result<void> {
    xdg_shell = wlr_xdg_shell_create(display, 3);
    if (!xdg_shell) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xdg-shell");
    }

    wl_list_init(&listeners.new_xdg_toplevel.link);
    listeners.new_xdg_toplevel.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_xdg_toplevel));
        list->impl->handle_new_xdg_toplevel(static_cast<wlr_xdg_toplevel*>(data));
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &listeners.new_xdg_toplevel);

    wl_list_init(&listeners.new_xdg_popup.link);
    listeners.new_xdg_popup.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Impl::Listeners*>(reinterpret_cast<char*>(listener) -
                                                        offsetof(Impl::Listeners, new_xdg_popup));
        list->impl->handle_new_xdg_popup(static_cast<wlr_xdg_popup*>(data));
    };
    wl_signal_add(&xdg_shell->events.new_popup, &listeners.new_xdg_popup);

    return {};
}

auto CompositorServer::Impl::setup_input_devices() -> Result<void> {
    seat = wlr_seat_create(display, "seat0");
    if (!seat) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create seat");
    }

    wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkb_ctx) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xkb context");
    }

    xkb_keymap* keymap = xkb_keymap_new_from_names(xkb_ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create xkb keymap");
    }

    keyboard = UniqueKeyboard(new wlr_keyboard{});
    wlr_keyboard_init(keyboard.get(), nullptr, "virtual-keyboard");
    wlr_keyboard_set_keymap(keyboard.get(), keymap);
    xkb_keymap_unref(keymap);

    wlr_seat_set_keyboard(seat, keyboard.get());

    relative_pointer_manager = wlr_relative_pointer_manager_v1_create(display);
    if (!relative_pointer_manager) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to create relative pointer manager");
    }

    pointer_constraints = wlr_pointer_constraints_v1_create(display);
    if (!pointer_constraints) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to create pointer constraints");
    }

    wl_list_init(&listeners.new_pointer_constraint.link);
    listeners.new_pointer_constraint.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_pointer_constraint));
        list->impl->handle_new_pointer_constraint(static_cast<wlr_pointer_constraint_v1*>(data));
    };
    wl_signal_add(&pointer_constraints->events.new_constraint, &listeners.new_pointer_constraint);

    return {};
}

auto CompositorServer::Impl::setup_event_loop_fd() -> Result<void> {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create eventfd");
    }
    event_fd = util::UniqueFd(efd);

    event_source = wl_event_loop_add_fd(
        event_loop, event_fd.get(), WL_EVENT_READABLE,
        [](int /*fd*/, uint32_t /*mask*/, void* data) -> int {
            auto* impl = static_cast<Impl*>(data);
            uint64_t val = 0;
            // eventfd guarantees 8-byte atomic read when readable
            (void)read(impl->event_fd.get(), &val, sizeof(val));
            impl->process_input_events();
            return 0;
        },
        this);

    if (!event_source) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to add eventfd to event loop");
    }

    return {};
}

auto CompositorServer::Impl::setup_xwayland() -> Result<void> {
    {
        StderrSuppressor suppress;
        xwayland = wlr_xwayland_create(display, compositor, false);
    }
    if (!xwayland) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create XWayland server");
    }

    listeners.new_xwayland_surface.notify = [](wl_listener* listener, void* data) {
        auto* list = reinterpret_cast<Impl::Listeners*>(
            reinterpret_cast<char*>(listener) - offsetof(Impl::Listeners, new_xwayland_surface));
        list->impl->handle_new_xwayland_surface(static_cast<wlr_xwayland_surface*>(data));
    };
    wl_signal_add(&xwayland->events.new_surface, &listeners.new_xwayland_surface);

    // wlr_xwm translates seat events to X11 KeyPress/MotionNotify
    wlr_xwayland_set_seat(xwayland, seat);

    return {};
}

auto CompositorServer::Impl::start_backend() -> Result<void> {
    if (!wlr_backend_start(backend)) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to start wlroots backend");
    }
    return {};
}

auto CompositorServer::Impl::setup_output() -> Result<void> {
    // Create headless output for native Wayland clients
    output = wlr_headless_add_output(backend, 1920, 1080);
    if (!output) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create headless output");
    }
    wlr_output_init_render(output, allocator, renderer);
    wlr_output_layout_add_auto(output_layout, output);

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);

    present_modifiers = {util::DRM_FORMAT_MOD_LINEAR};
    present_format.format = util::DRM_FORMAT_XRGB8888;
    present_format.len = present_modifiers.size();
    present_format.capacity = present_modifiers.size();
    present_format.modifiers = present_modifiers.data();

    present_swapchain =
        wlr_swapchain_create(allocator, output->width, output->height, &present_format);
    if (!present_swapchain) {
        GOGGLES_LOG_WARN("Compositor present swapchain unavailable; non-Vulkan presentation "
                         "disabled");
    } else {
        present_width = static_cast<uint32_t>(output->width);
        present_height = static_cast<uint32_t>(output->height);
    }

    return {};
}

auto CompositorServer::Impl::setup_cursor_theme() -> Result<void> {
    constexpr int CURSOR_SIZE = 64;
    cursor_theme = wlr_xcursor_theme_load("cursor", CURSOR_SIZE);
    if (!cursor_theme) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to load cursor theme");
    }

    cursor_shape = wlr_xcursor_theme_get_cursor(cursor_theme, "left_ptr");
    if (!cursor_shape) {
        cursor_shape = wlr_xcursor_theme_get_cursor(cursor_theme, "default");
    }
    if (!cursor_shape) {
        clear_cursor_theme();
        return make_error<void>(ErrorCode::input_init_failed,
                                "Cursor theme missing default cursor images");
    }

    cursor_frames.clear();
    cursor_frames.reserve(cursor_shape->image_count);
    for (unsigned int i = 0; i < cursor_shape->image_count; ++i) {
        const auto* image = cursor_shape->images[i];
        if (!image || !image->buffer || image->width == 0 || image->height == 0) {
            clear_cursor_theme();
            return make_error<void>(ErrorCode::input_init_failed,
                                    "Cursor theme contains invalid image data");
        }
        wlr_texture* texture =
            wlr_texture_from_pixels(renderer, util::DRM_FORMAT_ARGB8888, image->width * 4,
                                    image->width, image->height, image->buffer);
        if (!texture) {
            clear_cursor_theme();
            return make_error<void>(ErrorCode::input_init_failed,
                                    "Failed to create cursor texture");
        }
        CursorFrame frame{};
        frame.texture = texture;
        frame.width = image->width;
        frame.height = image->height;
        frame.hotspot_x = image->hotspot_x;
        frame.hotspot_y = image->hotspot_y;
        frame.delay_ms = image->delay;
        cursor_frames.push_back(frame);
    }

    if (cursor_frames.empty()) {
        clear_cursor_theme();
        return make_error<void>(ErrorCode::input_init_failed,
                                "Cursor theme provided no usable images");
    }

    return {};
}

void CompositorServer::Impl::clear_cursor_theme() {
    for (auto& frame : cursor_frames) {
        if (frame.texture) {
            wlr_texture_destroy(frame.texture);
            frame.texture = nullptr;
        }
    }
    cursor_frames.clear();
    cursor_shape = nullptr;
    if (cursor_theme) {
        wlr_xcursor_theme_destroy(cursor_theme);
        cursor_theme = nullptr;
    }
}

auto CompositorServer::Impl::get_cursor_frame(uint32_t time_msec) const -> const CursorFrame* {
    if (!cursor_shape || cursor_frames.empty()) {
        return nullptr;
    }
    const int frame_index = wlr_xcursor_frame(cursor_shape, time_msec);
    if (frame_index < 0 || static_cast<size_t>(frame_index) >= cursor_frames.size()) {
        return nullptr;
    }
    return &cursor_frames[static_cast<size_t>(frame_index)];
}

void CompositorServer::Impl::start_compositor_thread() {
    compositor_thread = std::jthread([this] {
        StderrSuppressor suppress;
        wl_display_run(display);
    });
}

auto CompositorServer::start() -> Result<void> {
    auto& impl = *m_impl;
    auto cleanup_on_error = [this](void*) { stop(); };
    std::unique_ptr<void, decltype(cleanup_on_error)> guard(this, cleanup_on_error);

    GOGGLES_TRY(impl.setup_base_components());
    GOGGLES_TRY(impl.create_allocator());
    GOGGLES_TRY(impl.create_compositor());
    GOGGLES_TRY(impl.create_output_layout());
    GOGGLES_TRY(impl.setup_xdg_shell());
    GOGGLES_TRY(impl.setup_input_devices());
    GOGGLES_TRY(impl.setup_event_loop_fd());

    auto socket_result = bind_wayland_socket(impl.display);
    if (!socket_result) {
        return make_error<void>(socket_result.error().code, socket_result.error().message);
    }
    impl.wayland_socket_name = *socket_result;

    GOGGLES_TRY(impl.setup_xwayland());
    GOGGLES_TRY(impl.start_backend());
    GOGGLES_TRY(impl.setup_output());
    auto cursor_result = impl.setup_cursor_theme();
    if (!cursor_result) {
        GOGGLES_LOG_WARN("Compositor cursor theme unavailable: {}", cursor_result.error().message);
    }

    impl.start_compositor_thread();

    guard.release(); // NOLINT(bugprone-unused-return-value)
    return {};
}

void CompositorServer::stop() {
    auto& impl = *m_impl;

    if (!impl.display) {
        return;
    }

    wl_display_terminate(impl.display);

    // Must join before destroying objects thread accesses
    if (impl.compositor_thread.joinable()) {
        impl.compositor_thread.join();
    }

    // Must remove before closing eventfd
    if (impl.event_source) {
        wl_event_source_remove(impl.event_source);
        impl.event_source = nullptr;
    }

    impl.focused_surface = nullptr;
    impl.focused_xsurface = nullptr;
    impl.keyboard_entered_surface = nullptr;
    impl.pointer_entered_surface = nullptr;
    impl.clear_presented_frame();
    impl.clear_cursor_theme();

    // Destruction order: xwayland before compositor, seat before display
    if (impl.xwayland) {
        wlr_xwayland_destroy(impl.xwayland);
        impl.xwayland = nullptr;
    }

    if (impl.keyboard) {
        impl.keyboard.reset();
    }

    if (impl.xkb_ctx) {
        xkb_context_unref(impl.xkb_ctx);
        impl.xkb_ctx = nullptr;
    }

    if (impl.seat) {
        wlr_seat_destroy(impl.seat);
        impl.seat = nullptr;
    }

    impl.xdg_shell = nullptr;
    impl.compositor = nullptr;
    impl.output = nullptr;
    if (impl.present_swapchain) {
        wlr_swapchain_destroy(impl.present_swapchain);
        impl.present_swapchain = nullptr;
    }

    if (impl.output_layout) {
        wlr_output_layout_destroy(impl.output_layout);
        impl.output_layout = nullptr;
    }

    if (impl.allocator) {
        wlr_allocator_destroy(impl.allocator);
        impl.allocator = nullptr;
    }

    if (impl.renderer) {
        wlr_renderer_destroy(impl.renderer);
        impl.renderer = nullptr;
    }

    if (impl.backend) {
        wlr_backend_destroy(impl.backend);
        impl.backend = nullptr;
    }

    if (impl.display) {
        wl_display_destroy(impl.display);
        impl.display = nullptr;
    }

    impl.event_loop = nullptr;
    impl.wayland_socket_name.clear();
}

auto CompositorServer::inject_event(const InputEvent& event) -> bool {
    if (!m_impl->event_queue.try_push(event)) {
        return false;
    }
    return m_impl->wake_event_loop();
}

auto CompositorServer::is_pointer_locked() const -> bool {
    return m_impl->pointer_locked.load(std::memory_order_acquire);
}

void CompositorServer::set_cursor_visible(bool visible) {
    m_impl->set_cursor_visible(visible);
}

auto CompositorServer::get_presented_frame(uint64_t after_frame_number) const
    -> std::optional<util::ExternalImageFrame> {
    std::scoped_lock lock(m_impl->present_mutex);
    if (!m_impl->presented_frame) {
        return std::nullopt;
    }
    const auto& stored = *m_impl->presented_frame;
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
    frame.image.handle_type = stored.image.handle_type;
    frame.frame_number = stored.frame_number;
    frame.image.handle = stored.image.handle.dup();
    if (!frame.image.handle) {
        return std::nullopt;
    }
    return frame;
}

bool CompositorServer::Impl::wake_event_loop() {
    if (!event_fd.valid()) {
        return false;
    }
    uint64_t val = 1;
    auto ret = write(event_fd.get(), &val, sizeof(val));
    return ret == sizeof(val);
}

void CompositorServer::Impl::request_focus_target(uint32_t surface_id) {
    if (surface_id == NO_FOCUS_TARGET) {
        return;
    }
    pending_focus_target.store(surface_id, std::memory_order_release);
    wake_event_loop();
}

void CompositorServer::Impl::request_surface_resize(uint32_t surface_id,
                                                    const SurfaceResizeInfo& resize) {
    if (surface_id == NO_FOCUS_TARGET) {
        return;
    }
    SurfaceResizeRequest request{};
    request.surface_id = surface_id;
    request.resize = resize;
    if (!resize_queue.try_push(request)) {
        return;
    }
    wake_event_loop();
}

void CompositorServer::Impl::handle_focus_request() {
    const auto focus_id = pending_focus_target.exchange(NO_FOCUS_TARGET, std::memory_order_acq_rel);
    if (focus_id == NO_FOCUS_TARGET) {
        return;
    }
    focus_surface_by_id(focus_id);
}

void CompositorServer::Impl::handle_surface_resize_requests() {
    while (auto request_opt = resize_queue.try_pop()) {
        apply_surface_resize_request(*request_opt);
    }
}

void CompositorServer::Impl::process_input_events() {
    handle_focus_request();
    handle_surface_resize_requests();
    if (present_reset_requested.exchange(false, std::memory_order_acq_rel)) {
        refresh_presented_frame();
    }

    while (auto event_opt = event_queue.try_pop()) {
        auto& event = *event_opt;
        uint32_t time = get_time_msec();

        switch (event.type) {
        case InputEventType::key:
            handle_key_event(event, time);
            break;
        case InputEventType::pointer_motion:
            handle_pointer_motion_event(event, time);
            break;
        case InputEventType::pointer_button:
            handle_pointer_button_event(event, time);
            break;
        case InputEventType::pointer_axis:
            handle_pointer_axis_event(event, time);
            break;
        }
    }
}

void CompositorServer::Impl::handle_key_event(const InputEvent& event, uint32_t time) {
    auto target = get_input_target();
    wlr_surface* target_surface = target.surface;
    wlr_xwayland_surface* target_xsurface = target.xsurface;

    if (!target_surface) {
        return;
    }

    // XWayland quirk: wlr_xwm requires re-activation and keyboard re-entry before each
    // key event. Without this, X11 clients silently drop input after the first event.
    // Native Wayland clients maintain focus state correctly and only need enter on change.
    if (target_xsurface) {
        wlr_xwayland_surface_activate(target_xsurface, true);
        wlr_seat_set_keyboard(seat, keyboard.get());
        wlr_seat_keyboard_notify_enter(seat, target_surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
    } else if (keyboard_entered_surface != target_surface) {
        wlr_seat_set_keyboard(seat, keyboard.get());
        wlr_seat_keyboard_notify_enter(seat, target_surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
        keyboard_entered_surface = target_surface;
    }
    auto state = event.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
    wlr_seat_keyboard_notify_key(seat, time, event.code, state);
}

void CompositorServer::Impl::handle_pointer_motion_event(const InputEvent& event, uint32_t time) {
    auto root_target = get_root_input_target();
    if (!root_target.root_surface) {
        return;
    }
    // Send relative motion (always, regardless of constraint)
    if (relative_pointer_manager && (event.dx != 0.0 || event.dy != 0.0)) {
        wlr_relative_pointer_manager_v1_send_relative_motion(
            relative_pointer_manager, seat, static_cast<uint64_t>(time) * 1000, event.dx, event.dy,
            event.dx, event.dy);
    }

    // For locked constraints, skip absolute motion update
    if (active_constraint && active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        apply_cursor_hint_if_needed();
        wlr_seat_pointer_notify_frame(seat);
        return;
    }

    // XWayland quirk: requires re-activation and pointer re-entry before each event
    update_cursor_position(event, root_target);

    auto target = resolve_input_target(root_target, true);
    wlr_surface* target_surface = target.surface;
    wlr_xwayland_surface* target_xsurface = target.xsurface;
    if (!target_surface) {
        return;
    }

    const auto [local_x, local_y] = get_surface_local_coords(target);
    if (target_xsurface) {
        wlr_xwayland_surface_activate(target_xsurface, true);
        wlr_seat_pointer_notify_enter(seat, target_surface, local_x, local_y);
    } else if (pointer_entered_surface != target_surface) {
        wlr_seat_pointer_notify_enter(seat, target_surface, local_x, local_y);
        pointer_entered_surface = target_surface;
    }
    wlr_seat_pointer_notify_motion(seat, time, local_x, local_y);
    wlr_seat_pointer_notify_frame(seat);
}

void CompositorServer::Impl::handle_pointer_button_event(const InputEvent& event, uint32_t time) {
    auto root_target = get_root_input_target();
    auto target = resolve_input_target(root_target, true);
    wlr_surface* target_surface = target.surface;
    wlr_xwayland_surface* target_xsurface = target.xsurface;
    wlr_surface* cursor_reference = target.root_surface ? target.root_surface : target_surface;

    if (!target_surface) {
        return;
    }

    if (cursor_surface != cursor_reference || !cursor_initialized) {
        reset_cursor_for_surface(cursor_reference);
    }

    const auto [local_x, local_y] = get_surface_local_coords(target);
    if (target_xsurface) {
        wlr_xwayland_surface_activate(target_xsurface, true);
        if (pointer_entered_surface != target_surface) {
            wlr_seat_pointer_notify_enter(seat, target_surface, local_x, local_y);
            pointer_entered_surface = target_surface;
        }
    } else if (pointer_entered_surface != target_surface) {
        wlr_seat_pointer_notify_enter(seat, target_surface, local_x, local_y);
        pointer_entered_surface = target_surface;
    }
    auto state = event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
    wlr_seat_pointer_notify_button(seat, time, event.code, state);
    wlr_seat_pointer_notify_frame(seat);
}

void CompositorServer::Impl::handle_pointer_axis_event(const InputEvent& event, uint32_t time) {
    auto root_target = get_root_input_target();
    auto target = resolve_input_target(root_target, true);
    wlr_surface* target_surface = target.surface;
    wlr_xwayland_surface* target_xsurface = target.xsurface;
    wlr_surface* cursor_reference = target.root_surface ? target.root_surface : target_surface;

    if (!target_surface) {
        return;
    }

    if (cursor_surface != cursor_reference || !cursor_initialized) {
        reset_cursor_for_surface(cursor_reference);
    }

    const auto [local_x, local_y] = get_surface_local_coords(target);
    if (target_xsurface) {
        wlr_xwayland_surface_activate(target_xsurface, true);
        wlr_seat_pointer_notify_enter(seat, target_surface, local_x, local_y);
    } else if (pointer_entered_surface != target_surface) {
        wlr_seat_pointer_notify_enter(seat, target_surface, local_x, local_y);
        pointer_entered_surface = target_surface;
    }
    auto orientation =
        event.horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL : WL_POINTER_AXIS_VERTICAL_SCROLL;
    wlr_seat_pointer_notify_axis(seat, time, orientation, event.value,
                                 0, // value_discrete (legacy)
                                 WL_POINTER_AXIS_SOURCE_WHEEL,
                                 WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
    wlr_seat_pointer_notify_frame(seat);
}

void CompositorServer::Impl::handle_new_xdg_toplevel(wlr_xdg_toplevel* toplevel) {
    if (!toplevel || !toplevel->base) {
        return;
    }

    GOGGLES_LOG_DEBUG("New XDG toplevel: toplevel={} surface={} title='{}' app_id='{}'",
                      static_cast<void*>(toplevel), static_cast<void*>(toplevel->base->surface),
                      toplevel->title ? toplevel->title : "",
                      toplevel->app_id ? toplevel->app_id : "");

    auto* hooks = new XdgToplevelHooks{};
    hooks->impl = this;
    hooks->toplevel = toplevel;
    hooks->surface = toplevel->base->surface;
    hooks->id = next_surface_id++;
    {
        std::scoped_lock lock(hooks_mutex);
        xdg_hooks.push_back(hooks);
    }

    wl_list_init(&hooks->surface_commit.link);
    hooks->surface_commit.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_commit));
        h->impl->handle_xdg_surface_commit(h);
    };
    wl_signal_add(&hooks->surface->events.commit, &hooks->surface_commit);

    wl_list_init(&hooks->xdg_ack_configure.link);
    hooks->xdg_ack_configure.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XdgToplevelHooks, xdg_ack_configure));
        h->impl->handle_xdg_surface_ack_configure(h);
    };
    wl_signal_add(&toplevel->base->events.ack_configure, &hooks->xdg_ack_configure);

    wl_list_init(&hooks->surface_map.link);
    hooks->surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_map));
        h->impl->handle_xdg_surface_map(h);
    };
    wl_signal_add(&hooks->surface->events.map, &hooks->surface_map);

    wl_list_init(&hooks->surface_destroy.link);
    hooks->surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, surface_destroy));
        h->impl->handle_xdg_surface_destroy(h);
    };
    wl_signal_add(&hooks->surface->events.destroy, &hooks->surface_destroy);

    wl_list_init(&hooks->toplevel_destroy.link);
    hooks->toplevel_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgToplevelHooks*>(reinterpret_cast<char*>(listener) -
                                                      offsetof(XdgToplevelHooks, toplevel_destroy));
        wl_list_remove(&h->toplevel_destroy.link);
        wl_list_init(&h->toplevel_destroy.link);
        wl_list_remove(&h->xdg_ack_configure.link);
        wl_list_init(&h->xdg_ack_configure.link);
        h->toplevel = nullptr;
    };
    wl_signal_add(&toplevel->events.destroy, &hooks->toplevel_destroy);
}

void CompositorServer::Impl::handle_new_xdg_popup(wlr_xdg_popup* popup) {
    if (!popup || !popup->base || !popup->base->surface) {
        return;
    }

    GOGGLES_LOG_DEBUG("New XDG popup: popup={} surface={} parent={}", static_cast<void*>(popup),
                      static_cast<void*>(popup->base->surface), static_cast<void*>(popup->parent));

    auto hooks = std::make_unique<XdgPopupHooks>();
    auto* hooks_ptr = hooks.get();
    hooks_ptr->impl = this;
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
        h->impl->handle_xdg_popup_commit(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.commit, &hooks_ptr->surface_commit);

    wl_list_init(&hooks_ptr->xdg_ack_configure.link);
    hooks_ptr->xdg_ack_configure.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, xdg_ack_configure));
        h->impl->handle_xdg_popup_ack_configure(h);
    };
    wl_signal_add(&popup->base->events.ack_configure, &hooks_ptr->xdg_ack_configure);

    wl_list_init(&hooks_ptr->surface_map.link);
    hooks_ptr->surface_map.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, surface_map));
        h->impl->handle_xdg_popup_map(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.map, &hooks_ptr->surface_map);

    wl_list_init(&hooks_ptr->surface_destroy.link);
    hooks_ptr->surface_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, surface_destroy));
        h->impl->handle_xdg_popup_destroy(h);
    };
    wl_signal_add(&hooks_ptr->surface->events.destroy, &hooks_ptr->surface_destroy);

    wl_list_init(&hooks_ptr->popup_destroy.link);
    hooks_ptr->popup_destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XdgPopupHooks*>(reinterpret_cast<char*>(listener) -
                                                   offsetof(XdgPopupHooks, popup_destroy));
        h->impl->handle_xdg_popup_destroy(h);
    };
    wl_signal_add(&popup->events.destroy, &hooks_ptr->popup_destroy);
}

void CompositorServer::Impl::handle_xdg_popup_commit(XdgPopupHooks* hooks) {
    if (!hooks || !hooks->popup || !hooks->popup->base || !hooks->popup->base->initialized) {
        return;
    }

    if (!hooks->sent_configure) {
        wlr_xdg_surface_schedule_configure(hooks->popup->base);
        hooks->sent_configure = true;

        auto* root = get_root_xdg_surface(hooks->surface);
        if (root && root->surface) {
            auto extent_opt = get_surface_extent(root->surface);
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

    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->surface, &now);

    update_presented_frame(hooks->surface);
}

void CompositorServer::Impl::handle_xdg_popup_ack_configure(XdgPopupHooks* hooks) {
    if (!hooks || hooks->acked_configure) {
        return;
    }

    hooks->acked_configure = true;

    wl_list_remove(&hooks->xdg_ack_configure.link);
    wl_list_init(&hooks->xdg_ack_configure.link);
}

void CompositorServer::Impl::handle_xdg_popup_map(XdgPopupHooks* hooks) {
    if (!hooks || hooks->mapped) {
        return;
    }

    hooks->mapped = true;

    GOGGLES_LOG_DEBUG("XDG popup mapped: id={} surface={} parent={}", hooks->id,
                      static_cast<void*>(hooks->surface),
                      static_cast<void*>(hooks->parent_surface));

    wl_list_remove(&hooks->surface_map.link);
    wl_list_init(&hooks->surface_map.link);
    request_present_reset();
}

void CompositorServer::Impl::handle_xdg_popup_destroy(XdgPopupHooks* hooks) {
    if (!hooks || hooks->destroyed) {
        return;
    }

    hooks->destroyed = true;

    GOGGLES_LOG_DEBUG("XDG popup destroyed: id={} surface={} parent={}", hooks->id,
                      static_cast<void*>(hooks->surface),
                      static_cast<void*>(hooks->parent_surface));

    wl_list_remove(&hooks->surface_destroy.link);
    wl_list_init(&hooks->surface_destroy.link);
    wl_list_remove(&hooks->surface_commit.link);
    wl_list_init(&hooks->surface_commit.link);
    wl_list_remove(&hooks->surface_map.link);
    wl_list_init(&hooks->surface_map.link);
    wl_list_remove(&hooks->xdg_ack_configure.link);
    wl_list_init(&hooks->xdg_ack_configure.link);
    wl_list_remove(&hooks->popup_destroy.link);
    wl_list_init(&hooks->popup_destroy.link);

    {
        std::scoped_lock lock(hooks_mutex);
        auto hook_it = std::find_if(
            xdg_popup_hooks.begin(), xdg_popup_hooks.end(),
            [hooks](const std::unique_ptr<XdgPopupHooks>& entry) { return entry.get() == hooks; });
        if (hook_it != xdg_popup_hooks.end()) {
            xdg_popup_hooks.erase(hook_it);
        }
    }

    if (keyboard_entered_surface == hooks->surface) {
        keyboard_entered_surface = nullptr;
    }
    if (pointer_entered_surface == hooks->surface) {
        pointer_entered_surface = nullptr;
    }

    request_present_reset();
}

void CompositorServer::Impl::handle_xdg_surface_commit(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || !hooks->toplevel->base || !hooks->toplevel->base->initialized) {
        return;
    }

    // Only do initial setup on first commit, but keep listening for all commits
    if (!hooks->sent_configure) {
        wlr_xdg_surface_schedule_configure(hooks->toplevel->base);
        hooks->sent_configure = true;
    }

    // Release buffer to allow swapchain image reuse
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->surface, &now);

    update_presented_frame(hooks->surface);
}

void CompositorServer::Impl::handle_xdg_surface_ack_configure(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || hooks->acked_configure) {
        return;
    }

    hooks->acked_configure = true;

    wl_list_remove(&hooks->xdg_ack_configure.link);
    wl_list_init(&hooks->xdg_ack_configure.link);

    if (!hooks->sent_configure) {
        return;
    }

    wlr_xdg_toplevel_set_activated(hooks->toplevel, true);
    focus_surface(hooks->surface);
}

void CompositorServer::Impl::handle_xdg_surface_map(XdgToplevelHooks* hooks) {
    if (!hooks->toplevel || hooks->mapped) {
        return;
    }

    hooks->mapped = true;

    GOGGLES_LOG_DEBUG("XDG surface mapped: id={} surface={} title='{}' app_id='{}' size={}x{}",
                      hooks->id, static_cast<void*>(hooks->surface),
                      hooks->toplevel->title ? hooks->toplevel->title : "",
                      hooks->toplevel->app_id ? hooks->toplevel->app_id : "",
                      hooks->toplevel->current.width, hooks->toplevel->current.height);

    wl_list_remove(&hooks->surface_map.link);
    wl_list_init(&hooks->surface_map.link);
}

void CompositorServer::Impl::handle_xdg_surface_destroy(XdgToplevelHooks* hooks) {
    wl_list_remove(&hooks->surface_destroy.link);
    wl_list_init(&hooks->surface_destroy.link);
    wl_list_remove(&hooks->surface_commit.link);
    wl_list_init(&hooks->surface_commit.link);
    wl_list_remove(&hooks->surface_map.link);
    wl_list_init(&hooks->surface_map.link);
    wl_list_remove(&hooks->xdg_ack_configure.link);
    wl_list_init(&hooks->xdg_ack_configure.link);
    wl_list_remove(&hooks->toplevel_destroy.link);
    wl_list_init(&hooks->toplevel_destroy.link);

    if (!focused_xsurface && focused_surface == hooks->surface) {
        focused_surface = nullptr;
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
    if (presented_surface == hooks->surface) {
        clear_presented_frame();
    }

    {
        std::scoped_lock lock(hooks_mutex);
        auto hook_it = std::find(xdg_hooks.begin(), xdg_hooks.end(), hooks);
        if (hook_it != xdg_hooks.end()) {
            xdg_hooks.erase(hook_it);
        }
    }

    delete hooks;
}

void CompositorServer::Impl::handle_new_xwayland_surface(wlr_xwayland_surface* xsurface) {
    GOGGLES_LOG_DEBUG("New XWayland surface: window_id={} ptr={}",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface));

    auto* hooks = new XWaylandSurfaceHooks{};
    hooks->impl = this;
    hooks->xsurface = xsurface;
    hooks->id = next_surface_id++;
    hooks->override_redirect = xsurface->override_redirect;
    {
        std::scoped_lock lock(hooks_mutex);
        xwayland_hooks.push_back(hooks);
    }

    wl_list_init(&hooks->associate.link);
    wl_list_init(&hooks->map_request.link);
    wl_list_init(&hooks->commit.link);
    wl_list_init(&hooks->destroy.link);

    hooks->associate.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XWaylandSurfaceHooks, associate));
        h->impl->handle_xwayland_surface_associate(h->xsurface);

        // Register commit listener now that surface is available
        if (h->xsurface->surface && h->commit.link.next == &h->commit.link) {
            h->commit.notify = [](wl_listener* l, void* /*data*/) {
                auto* hk = reinterpret_cast<XWaylandSurfaceHooks*>(
                    reinterpret_cast<char*>(l) - offsetof(XWaylandSurfaceHooks, commit));
                hk->impl->handle_xwayland_surface_commit(hk);
            };
            wl_signal_add(&h->xsurface->surface->events.commit, &h->commit);
        }
    };
    wl_signal_add(&xsurface->events.associate, &hooks->associate);

    hooks->map_request.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(
            reinterpret_cast<char*>(listener) - offsetof(XWaylandSurfaceHooks, map_request));
        h->impl->handle_xwayland_surface_map_request(h);
    };
    wl_signal_add(&xsurface->events.map_request, &hooks->map_request);

    hooks->destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<XWaylandSurfaceHooks*>(reinterpret_cast<char*>(listener) -
                                                          offsetof(XWaylandSurfaceHooks, destroy));
        h->impl->handle_xwayland_surface_destroy(h->xsurface);
        wl_list_remove(&h->associate.link);
        wl_list_remove(&h->map_request.link);
        if (h->commit.link.next != nullptr && h->commit.link.next != &h->commit.link) {
            wl_list_remove(&h->commit.link);
        }
        wl_list_remove(&h->destroy.link);
        delete h;
    };
    wl_signal_add(&xsurface->events.destroy, &hooks->destroy);
}

void CompositorServer::Impl::handle_xwayland_surface_associate(wlr_xwayland_surface* xsurface) {
    if (!xsurface->surface) {
        return;
    }

    XWaylandSurfaceHooks* hooks = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        auto hook_it = std::find_if(
            xwayland_hooks.begin(), xwayland_hooks.end(),
            [xsurface](const XWaylandSurfaceHooks* h) { return h->xsurface == xsurface; });
        if (hook_it != xwayland_hooks.end()) {
            hooks = *hook_it;
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
    if (hooks && hooks->map_requested && !hooks->mapped) {
        hooks->mapped = true;

        if (!hooks->override_redirect) {
            focus_xwayland_surface(xsurface);
        } else {
            request_present_reset();
        }
    }
}

void CompositorServer::Impl::handle_xwayland_surface_map_request(XWaylandSurfaceHooks* hooks) {
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

void CompositorServer::Impl::handle_xwayland_surface_commit(XWaylandSurfaceHooks* hooks) {
    if (!hooks->xsurface || !hooks->xsurface->surface) {
        return;
    }

    // Release buffer to allow swapchain image reuse
    // Without this, X11 clients block on vkQueuePresentKHR
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(hooks->xsurface->surface, &now);

    if (hooks->mapped) {
        update_presented_frame(hooks->xsurface->surface);
    }
}

void CompositorServer::Impl::handle_xwayland_surface_destroy(wlr_xwayland_surface* xsurface) {
    if (xsurface && xsurface->override_redirect) {
        GOGGLES_LOG_DEBUG("XWayland override-redirect destroyed: window_id={} ptr={}",
                          static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface));
    }
    {
        std::scoped_lock lock(hooks_mutex);
        auto hook_it = std::find_if(
            xwayland_hooks.begin(), xwayland_hooks.end(),
            [xsurface](const XWaylandSurfaceHooks* h) { return h->xsurface == xsurface; });
        if (hook_it != xwayland_hooks.end()) {
            xwayland_hooks.erase(hook_it);
        }
    }

    if (keyboard_entered_surface == xsurface->surface) {
        keyboard_entered_surface = nullptr;
    }
    if (pointer_entered_surface == xsurface->surface) {
        pointer_entered_surface = nullptr;
    }

    if (presented_surface == xsurface->surface) {
        clear_presented_frame();
    }

    if (xsurface && xsurface->override_redirect) {
        request_present_reset();
    }

    if (focused_xsurface != xsurface) {
        return;
    }

    GOGGLES_LOG_DEBUG("Focused XWayland surface destroyed: ptr={}", static_cast<void*>(xsurface));
    deactivate_constraint();
    focused_xsurface = nullptr;
    focused_surface = nullptr;
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

void CompositorServer::Impl::handle_new_pointer_constraint(wlr_pointer_constraint_v1* constraint) {
    wlr_surface* target_surface = focused_surface;
    if (focused_xsurface && focused_xsurface->surface) {
        target_surface = focused_xsurface->surface;
    }

    if (constraint->surface == target_surface) {
        activate_constraint(constraint);
    }

    auto* hooks = new ConstraintHooks{};
    hooks->impl = this;
    hooks->constraint = constraint;

    wl_list_init(&hooks->set_region.link);
    hooks->set_region.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<ConstraintHooks*>(reinterpret_cast<char*>(listener) -
                                                     offsetof(ConstraintHooks, set_region));
        h->impl->handle_constraint_set_region(h);
    };
    wl_signal_add(&constraint->events.set_region, &hooks->set_region);

    wl_list_init(&hooks->destroy.link);
    hooks->destroy.notify = [](wl_listener* listener, void* /*data*/) {
        auto* h = reinterpret_cast<ConstraintHooks*>(reinterpret_cast<char*>(listener) -
                                                     offsetof(ConstraintHooks, destroy));
        h->impl->handle_constraint_destroy(h);
    };
    wl_signal_add(&constraint->events.destroy, &hooks->destroy);
}

void CompositorServer::Impl::handle_constraint_set_region(ConstraintHooks* hooks) {
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
            const double clamped_x =
                std::clamp(cursor_x, static_cast<double>(box.x1), static_cast<double>(box.x2 - 1));
            const double clamped_y =
                std::clamp(cursor_y, static_cast<double>(box.y1), static_cast<double>(box.y2 - 1));
            cursor_x = clamped_x;
            cursor_y = clamped_y;
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

void CompositorServer::Impl::handle_constraint_destroy(ConstraintHooks* hooks) {
    if (active_constraint == hooks->constraint) {
        active_constraint = nullptr;
        pointer_locked.store(false, std::memory_order_release);
        request_present_reset();
    }
    wl_list_remove(&hooks->set_region.link);
    wl_list_remove(&hooks->destroy.link);
    delete hooks;
}

void CompositorServer::Impl::activate_constraint(wlr_pointer_constraint_v1* constraint) {
    if (active_constraint == constraint) {
        return;
    }
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

void CompositorServer::Impl::deactivate_constraint() {
    if (!active_constraint) {
        return;
    }
    wlr_pointer_constraint_v1_send_deactivated(active_constraint);
    GOGGLES_LOG_DEBUG("Pointer constraint deactivated");
    active_constraint = nullptr;
    pointer_locked.store(false, std::memory_order_release);
    request_present_reset();
}

void CompositorServer::Impl::focus_surface(wlr_surface* surface) {
    if (focused_surface == surface) {
        return;
    }

    uint32_t focused_id = 0;
    std::string title;
    std::string app_id;
    int width = 0;
    int height = 0;
    {
        std::scoped_lock lock(hooks_mutex);
        for (const auto* hooks : xdg_hooks) {
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

    // Deactivate any constraint on the previous surface
    deactivate_constraint();

    // Clear stale pointers BEFORE any wlroots calls that might access them
    // This prevents crashes when switching from XWayland to native Wayland
    focused_xsurface = nullptr;
    focused_surface = surface;

    GOGGLES_LOG_DEBUG("Focused XDG: id={} surface={} title='{}' app_id='{}' size={}x{}", focused_id,
                      static_cast<void*>(surface), title, app_id, width, height);

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes,
                                   &keyboard->modifiers);
    reset_cursor_for_surface(surface);
    wlr_seat_pointer_notify_enter(seat, surface, cursor_x, cursor_y);
    keyboard_entered_surface = surface;
    pointer_entered_surface = surface;

    // Check for existing constraint on the new surface
    if (pointer_constraints) {
        auto* constraint =
            wlr_pointer_constraints_v1_constraint_for_surface(pointer_constraints, surface, seat);
        if (constraint) {
            activate_constraint(constraint);
        }
    }

    refresh_presented_frame();
}

void CompositorServer::Impl::focus_xwayland_surface(wlr_xwayland_surface* xsurface) {
    if (focused_xsurface == xsurface) {
        return;
    }

    // Deactivate any constraint on the previous surface
    deactivate_constraint();

    // Clear seat focus first to prevent wlroots from sending leave events to stale surface
    wlr_seat_keyboard_clear_focus(seat);
    wlr_seat_pointer_clear_focus(seat);
    keyboard_entered_surface = nullptr;
    pointer_entered_surface = nullptr;

    focused_xsurface = xsurface;
    focused_surface = xsurface->surface;

    GOGGLES_LOG_DEBUG("Focused XWayland: window_id={} ptr={} surface={} title='{}'",
                      static_cast<uint32_t>(xsurface->window_id), static_cast<void*>(xsurface),
                      static_cast<void*>(xsurface->surface),
                      xsurface->title ? xsurface->title : "");

    // Activate the X11 window - required for wlr_xwm to send focus events
    wlr_xwayland_surface_activate(xsurface, true);

    wlr_seat_set_keyboard(seat, keyboard.get());
    wlr_seat_keyboard_notify_enter(seat, xsurface->surface, keyboard->keycodes,
                                   keyboard->num_keycodes, &keyboard->modifiers);
    reset_cursor_for_surface(xsurface->surface);
    wlr_seat_pointer_notify_enter(seat, xsurface->surface, cursor_x, cursor_y);
    keyboard_entered_surface = xsurface->surface;
    pointer_entered_surface = xsurface->surface;

    // Check for existing constraint on the new surface
    if (pointer_constraints && xsurface->surface) {
        auto* constraint = wlr_pointer_constraints_v1_constraint_for_surface(
            pointer_constraints, xsurface->surface, seat);
        if (constraint) {
            activate_constraint(constraint);
        }
    }

    refresh_presented_frame();
}

bool CompositorServer::Impl::focus_surface_by_id(uint32_t surface_id) {
    wlr_xwayland_surface* xwayland_target = nullptr;
    wlr_surface* xdg_surface_target = nullptr;
    wlr_xdg_toplevel* xdg_toplevel_target = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        for (auto* hooks : xwayland_hooks) {
            if (hooks->override_redirect) {
                continue;
            }
            if (hooks->id == surface_id && hooks->xsurface && hooks->xsurface->surface) {
                xwayland_target = hooks->xsurface;
                break;
            }
        }
        if (!xwayland_target) {
            for (auto* hooks : xdg_hooks) {
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

void CompositorServer::Impl::clear_presented_frame() {
    std::scoped_lock lock(present_mutex);
    if (presented_buffer) {
        wlr_buffer_unlock(presented_buffer);
        presented_buffer = nullptr;
    }
    presented_frame.reset();
    presented_surface = nullptr;
}

void CompositorServer::Impl::request_present_reset() {
    if (!present_reset_requested.exchange(true, std::memory_order_acq_rel)) {
        wake_event_loop();
    }
}

void CompositorServer::Impl::apply_surface_resize_request(const SurfaceResizeRequest& request) {
    if (request.surface_id == NO_FOCUS_TARGET) {
        return;
    }

    XdgToplevelHooks* xdg_hooks_entry = nullptr;
    XWaylandSurfaceHooks* xwayland_hooks_entry = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        for (auto* hooks : xdg_hooks) {
            if (hooks && hooks->id == request.surface_id) {
                xdg_hooks_entry = hooks;
                break;
            }
        }
        if (!xdg_hooks_entry) {
            for (auto* hooks : xwayland_hooks) {
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
        wlr_xwayland_surface_set_maximized(xsurface, request.resize.maximized);
        if (request.resize.width > 0 && request.resize.height > 0) {
            const uint16_t width = static_cast<uint16_t>(
                std::min<uint32_t>(request.resize.width, std::numeric_limits<uint16_t>::max()));
            const uint16_t height = static_cast<uint16_t>(
                std::min<uint32_t>(request.resize.height, std::numeric_limits<uint16_t>::max()));
            wlr_xwayland_surface_configure(xsurface, static_cast<int16_t>(xsurface->x),
                                           static_cast<int16_t>(xsurface->y), width, height);
        }
        request_present_reset();
    }
}

void CompositorServer::Impl::set_cursor_visible(bool visible) {
    bool previous = cursor_visible.exchange(visible, std::memory_order_acq_rel);
    if (previous != visible) {
        request_present_reset();
    }
}

auto CompositorServer::Impl::get_surface_extent(wlr_surface* surface) const
    -> std::optional<std::pair<uint32_t, uint32_t>> {
    if (surface && surface->current.width > 0 && surface->current.height > 0) {
        return std::pair<uint32_t, uint32_t>(static_cast<uint32_t>(surface->current.width),
                                             static_cast<uint32_t>(surface->current.height));
    }

    return std::nullopt;
}

auto CompositorServer::Impl::get_root_xdg_surface(wlr_surface* surface) const -> wlr_xdg_surface* {
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

auto CompositorServer::Impl::get_xdg_popup_position(const XdgPopupHooks* hooks) const
    -> std::pair<double, double> {
    if (!hooks || !hooks->popup) {
        return {0.0, 0.0};
    }

    double popup_x = 0.0;
    double popup_y = 0.0;
    wlr_xdg_popup_get_position(hooks->popup, &popup_x, &popup_y);
    return {popup_x, popup_y};
}

auto CompositorServer::Impl::get_cursor_bounds(const InputTarget& root_target) const
    -> std::optional<std::pair<double, double>> {
    if (!root_target.root_surface) {
        return std::nullopt;
    }

    auto extent_opt = get_surface_extent(root_target.root_surface);
    if (!extent_opt) {
        return std::nullopt;
    }

    auto width = static_cast<double>(extent_opt->first);
    auto height = static_cast<double>(extent_opt->second);

    if (!root_target.root_xsurface) {
        std::scoped_lock lock(hooks_mutex);
        for (const auto& hooks : xdg_popup_hooks) {
            auto* popup_hooks = hooks.get();
            if (!popup_hooks || !popup_hooks->mapped || !popup_hooks->popup ||
                !popup_hooks->surface) {
                continue;
            }
            if (!popup_hooks->acked_configure) {
                continue;
            }
            auto* root = get_root_xdg_surface(popup_hooks->surface);
            if (!root || root->surface != root_target.root_surface) {
                continue;
            }

            auto popup_extent = get_surface_extent(popup_hooks->surface);
            if (!popup_extent) {
                continue;
            }

            auto [popup_x, popup_y] = get_xdg_popup_position(popup_hooks);
            width = std::max(width, popup_x + static_cast<double>(popup_extent->first));
            height = std::max(height, popup_y + static_cast<double>(popup_extent->second));
        }

        return std::pair<double, double>(width, height);
    }

    std::scoped_lock lock(hooks_mutex);
    for (const auto* hooks : xwayland_hooks) {
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
        width = std::max(width, popup_x + static_cast<double>(popup_extent->first));
        height = std::max(height, popup_y + static_cast<double>(popup_extent->second));
    }

    return std::pair<double, double>(width, height);
}

auto CompositorServer::Impl::get_surface_local_coords(const InputTarget& target) const
    -> std::pair<double, double> {
    if (!target.surface) {
        return {0.0, 0.0};
    }

    double local_x = cursor_x - target.offset_x;
    double local_y = cursor_y - target.offset_y;

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

void CompositorServer::Impl::reset_cursor_for_surface(wlr_surface* surface) {
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

void CompositorServer::Impl::apply_cursor_hint_if_needed() {
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

void CompositorServer::Impl::auto_focus_next_surface() {
    XWaylandSurfaceHooks* last_xwayland = nullptr;
    for (auto* hooks : xwayland_hooks) {
        if (hooks->mapped && !hooks->override_redirect && hooks->xsurface &&
            hooks->xsurface->surface) {
            last_xwayland = hooks;
        }
    }
    if (last_xwayland) {
        focus_xwayland_surface(last_xwayland->xsurface);
        return;
    }

    XdgToplevelHooks* last_xdg = nullptr;
    for (auto* hooks : xdg_hooks) {
        if (hooks->mapped && hooks->surface && hooks->toplevel) {
            last_xdg = hooks;
        }
    }
    if (last_xdg) {
        wlr_xdg_toplevel_set_activated(last_xdg->toplevel, true);
        focus_surface(last_xdg->surface);
        return;
    }

    clear_presented_frame();
}

void CompositorServer::Impl::update_cursor_position(const InputEvent& event,
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

    auto bounds_opt = get_cursor_bounds(root_target);
    if (bounds_opt) {
        const auto [width, height] = *bounds_opt;
        if (width > 0 && height > 0) {
            next_x = std::clamp(next_x, 0.0, width - 1.0);
            next_y = std::clamp(next_y, 0.0, height - 1.0);
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

void CompositorServer::Impl::update_presented_frame(wlr_surface* surface) {
    auto target = get_input_target();
    if (!target.root_surface || !surface) {
        return;
    }

    if (target.surface != surface && target.root_surface != surface) {
        return;
    }

    render_surface_to_frame(target);
}

void CompositorServer::Impl::refresh_presented_frame() {
    auto target = get_input_target();
    if (!target.root_surface) {
        clear_presented_frame();
        return;
    }

    if (!render_surface_to_frame(target) && presented_surface != target.root_surface) {
        clear_presented_frame();
    }
}

void CompositorServer::Impl::render_root_surface_tree(wlr_render_pass* pass,
                                                      wlr_surface* root_surface) {
    RenderSurfaceContext render_context{};
    render_context.pass = pass;

    auto* root_xdg = get_root_xdg_surface(root_surface);
    if (root_xdg && root_xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wlr_xdg_surface_for_each_surface(root_xdg, render_surface_iterator, &render_context);
    } else {
        wlr_surface_for_each_surface(root_surface, render_surface_iterator, &render_context);
    }
}

void CompositorServer::Impl::render_xwayland_popup_surfaces(wlr_render_pass* pass,
                                                            const InputTarget& target) {
    std::scoped_lock lock(hooks_mutex);
    for (const auto* hooks : xwayland_hooks) {
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

        RenderSurfaceContext popup_context{};
        popup_context.pass = pass;
        popup_context.offset_x =
            static_cast<int32_t>(popup->x) - static_cast<int32_t>(target.root_xsurface->x);
        popup_context.offset_y =
            static_cast<int32_t>(popup->y) - static_cast<int32_t>(target.root_xsurface->y);
        wlr_surface_for_each_surface(popup->surface, render_surface_iterator, &popup_context);
    }
}

void CompositorServer::Impl::render_cursor_overlay(wlr_render_pass* pass) const {
    const bool show_cursor =
        cursor_visible.load(std::memory_order_acquire) &&
        (!active_constraint || active_constraint->type != WLR_POINTER_CONSTRAINT_V1_LOCKED);
    if (!show_cursor || !cursor_initialized || present_width == 0 || present_height == 0) {
        return;
    }

    const auto* frame = get_cursor_frame(get_time_msec());
    if (!frame || !frame->texture) {
        return;
    }

    const int center_x = static_cast<int>(std::lround(cursor_x));
    const int center_y = static_cast<int>(std::lround(cursor_y));
    const int min_x = -static_cast<int>(frame->hotspot_x);
    const int min_y = -static_cast<int>(frame->hotspot_y);
    const int max_x = static_cast<int>(present_width - 1) - static_cast<int>(frame->hotspot_x);
    const int max_y = static_cast<int>(present_height - 1) - static_cast<int>(frame->hotspot_y);
    const int draw_x = std::clamp(center_x - static_cast<int>(frame->hotspot_x), min_x, max_x);
    const int draw_y = std::clamp(center_y - static_cast<int>(frame->hotspot_y), min_y, max_y);

    wlr_render_texture_options cursor_opts{};
    cursor_opts.texture = frame->texture;
    cursor_opts.src_box = wlr_fbox{
        .x = 0.0,
        .y = 0.0,
        .width = static_cast<double>(frame->width),
        .height = static_cast<double>(frame->height),
    };
    cursor_opts.dst_box = wlr_box{
        .x = draw_x,
        .y = draw_y,
        .width = static_cast<int>(frame->width),
        .height = static_cast<int>(frame->height),
    };
    cursor_opts.filter_mode = WLR_SCALE_FILTER_NEAREST;
    cursor_opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
    wlr_render_pass_add_texture(pass, &cursor_opts);
}

bool CompositorServer::Impl::render_surface_to_frame(const InputTarget& target) {
    wlr_surface* root_surface = target.root_surface ? target.root_surface : target.surface;
    if (!present_swapchain || !root_surface) {
        return false;
    }

    wlr_texture* root_texture = wlr_surface_get_texture(root_surface);
    if (!root_texture) {
        return false;
    }

    // Capture at surface-native size; fixed-size output pre-scales and breaks viewer scale modes.
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

    int age = 0;
    wlr_buffer* buffer = wlr_swapchain_acquire(present_swapchain, &age);
    if (!buffer) {
        return false;
    }

    wlr_render_pass* pass = wlr_renderer_begin_buffer_pass(renderer, buffer, nullptr);
    if (!pass) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    render_root_surface_tree(pass, root_surface);
    if (target.root_xsurface) {
        render_xwayland_popup_surfaces(pass, target);
    }
    render_cursor_overlay(pass);

    if (!wlr_render_pass_submit(pass)) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    wlr_swapchain_set_buffer_submitted(present_swapchain, buffer);

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

    std::scoped_lock lock(present_mutex);
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
    frame.image.handle_type = util::ExternalHandleType::dmabuf;
    frame.frame_number = ++presented_frame_number;

    presented_frame = std::move(frame);
    presented_surface = root_surface;
    return true;
}

auto CompositorServer::Impl::get_root_input_target() -> InputTarget {
    InputTarget target{};

    wlr_surface* root_surface = nullptr;
    wlr_xwayland_surface* root_xsurface = nullptr;

    if (focused_xsurface && focused_xsurface->surface) {
        root_xsurface = focused_xsurface;
        root_surface = focused_xsurface->surface;
    } else if (focused_surface) {
        root_surface = focused_surface;
    }

    if (!root_surface) {
        return target;
    }

    target.surface = root_surface;
    target.xsurface = root_xsurface;
    target.root_surface = root_surface;
    target.root_xsurface = root_xsurface;
    return target;
}

auto CompositorServer::Impl::resolve_input_target(const InputTarget& root_target,
                                                  bool use_pointer_hit_test) -> InputTarget {
    InputTarget target = root_target;
    if (!root_target.root_surface) {
        return target;
    }

    if (!root_target.root_xsurface) {
        wlr_surface* hit_surface = nullptr;
        double sub_x = cursor_x;
        double sub_y = cursor_y;

        if (use_pointer_hit_test) {
            auto* root_xdg = get_root_xdg_surface(root_target.root_surface);
            if (root_xdg && root_xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
                hit_surface =
                    wlr_xdg_surface_surface_at(root_xdg, cursor_x, cursor_y, &sub_x, &sub_y);
            } else {
                hit_surface = wlr_surface_surface_at(root_target.root_surface, cursor_x, cursor_y,
                                                     &sub_x, &sub_y);
            }
        }

        XdgPopupHooks* topmost_popup = nullptr;
        {
            std::scoped_lock lock(hooks_mutex);
            for (const auto& hooks : xdg_popup_hooks) {
                auto* popup_hooks = hooks.get();
                if (!popup_hooks || !popup_hooks->mapped || !popup_hooks->popup ||
                    !popup_hooks->surface) {
                    continue;
                }
                if (!popup_hooks->acked_configure || popup_hooks->popup->seat != seat) {
                    continue;
                }
                auto* root = get_root_xdg_surface(popup_hooks->surface);
                if (root && root->surface == root_target.root_surface) {
                    topmost_popup = popup_hooks;
                }
            }
        }

        if (topmost_popup) {
            if (use_pointer_hit_test && hit_surface) {
                wlr_surface* hit_root = wlr_surface_get_root_surface(hit_surface);
                if (hit_root == topmost_popup->surface) {
                    target.surface = hit_surface;
                    target.offset_x = cursor_x - sub_x;
                    target.offset_y = cursor_y - sub_y;
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
            target.offset_x = cursor_x - sub_x;
            target.offset_y = cursor_y - sub_y;
            return target;
        }

        target.surface = root_target.root_surface;
        target.offset_x = 0.0;
        target.offset_y = 0.0;
        return target;
    }

    XWaylandSurfaceHooks* topmost_popup = nullptr;
    {
        std::scoped_lock lock(hooks_mutex);
        for (auto* hooks : xwayland_hooks) {
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

auto CompositorServer::Impl::get_input_target() -> InputTarget {
    auto root_target = get_root_input_target();
    return resolve_input_target(root_target, false);
}

auto CompositorServer::get_surfaces() const -> std::vector<SurfaceInfo> {
    std::scoped_lock lock(m_impl->hooks_mutex);
    std::vector<SurfaceInfo> result;

    uint32_t target_id = 0;
    if (m_impl->focused_xsurface && m_impl->focused_xsurface->surface) {
        for (auto* hooks : m_impl->xwayland_hooks) {
            if (!hooks->override_redirect && hooks->xsurface == m_impl->focused_xsurface) {
                target_id = hooks->id;
                break;
            }
        }
    }
    if (target_id == 0 && m_impl->focused_surface) {
        for (auto* hooks : m_impl->xdg_hooks) {
            if (hooks->surface == m_impl->focused_surface) {
                target_id = hooks->id;
                break;
            }
        }
    }

    for (const auto* hooks : m_impl->xwayland_hooks) {
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
        info.capture_path = SurfaceCapturePath::compositor;
        result.push_back(std::move(info));
    }

    for (const auto* hooks : m_impl->xdg_hooks) {
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
        info.capture_path = SurfaceCapturePath::compositor;
        result.push_back(std::move(info));
    }

    return result;
}

void CompositorServer::set_input_target(uint32_t surface_id) {
    m_impl->request_focus_target(surface_id);
}

void CompositorServer::request_surface_resize(uint32_t surface_id,
                                              const SurfaceResizeInfo& resize) {
    m_impl->request_surface_resize(surface_id, resize);
}

} // namespace goggles::input
