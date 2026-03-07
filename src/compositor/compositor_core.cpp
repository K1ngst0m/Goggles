#include "compositor_state.hpp"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <sys/eventfd.h>
#include <unistd.h>

extern "C" {
#include <wlr/backend/headless.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

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

} // namespace

CompositorState::CompositorState() {
    listeners.state = this;
    wl_list_init(&listeners.new_xdg_toplevel.link);
    wl_list_init(&listeners.new_xdg_popup.link);
    wl_list_init(&listeners.new_xwayland_surface.link);
    wl_list_init(&listeners.new_pointer_constraint.link);
    wl_list_init(&listeners.new_layer_surface.link);
}

auto CompositorState::setup_base_components() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
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

auto CompositorState::create_allocator() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create allocator");
    }
    return {};
}

auto CompositorState::create_compositor() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    compositor = wlr_compositor_create(display, 6, renderer);
    if (!compositor) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create compositor");
    }

    int drm_fd = wlr_renderer_get_drm_fd(renderer);
    if (drm_fd >= 0) {
        syncobj_manager = wlr_linux_drm_syncobj_manager_v1_create(display, 1, drm_fd);
        if (syncobj_manager) {
            GOGGLES_LOG_INFO("Compositor: explicit sync (wp_linux_drm_syncobj_v1) enabled");
        }
    }

    return {};
}

auto CompositorState::create_output_layout() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    output_layout = wlr_output_layout_create(display);
    if (!output_layout) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create output layout");
    }
    return {};
}

auto CompositorState::setup_event_loop_fd() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to create eventfd");
    }
    event_fd = util::UniqueFd(efd);

    event_source = wl_event_loop_add_fd(
        event_loop, event_fd.get(), WL_EVENT_READABLE,
        [](int /*fd*/, uint32_t /*mask*/, void* data) -> int {
            auto* state = static_cast<CompositorState*>(data);
            uint64_t value = 0;
            (void)read(state->event_fd.get(), &value, sizeof(value));
            state->process_input_events();
            return 0;
        },
        this);

    if (!event_source) {
        return make_error<void>(ErrorCode::input_init_failed,
                                "Failed to add eventfd to event loop");
    }

    return {};
}

auto CompositorState::bind_wayland_socket() -> Result<void> {
    for (int display_num = 0; display_num < 10; ++display_num) {
        std::array<char, 32> socket_name{};
        std::snprintf(socket_name.data(), socket_name.size(), "goggles-%d", display_num);
        if (wl_display_add_socket(display, socket_name.data()) == 0) {
            wayland_socket_name = socket_name.data();
            return {};
        }
    }

    return make_error<void>(ErrorCode::input_init_failed,
                            "No available goggles sockets (goggles-0..9 all bound)");
}

auto CompositorState::start_backend() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    if (!wlr_backend_start(backend)) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to start wlroots backend");
    }
    return {};
}

auto CompositorState::setup_output() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
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

    return {};
}

void CompositorState::start_compositor_thread() {
    compositor_thread = std::jthread([this] {
        GOGGLES_PROFILE_FUNCTION();
        run_compositor_display_loop();
    });
}

void CompositorState::teardown() {
    GOGGLES_PROFILE_FUNCTION();

    if (!display) {
        return;
    }

    wl_display_terminate(display);

    if (compositor_thread.joinable()) {
        compositor_thread.join();
    }

    if (event_source) {
        wl_event_source_remove(event_source);
        event_source = nullptr;
    }

    {
        std::scoped_lock lock(hooks_mutex);
        focused_surface = nullptr;
        focused_xsurface = nullptr;
    }
    keyboard_entered_surface = nullptr;
    pointer_entered_surface = nullptr;
    clear_presented_frame();
    clear_cursor_theme();

    detach_listener(listeners.new_xwayland_surface);
    detach_listener(listeners.new_pointer_constraint);
    detach_listener(listeners.new_xdg_popup);
    detach_listener(listeners.new_xdg_toplevel);
    detach_listener(listeners.new_layer_surface);

    {
        std::scoped_lock lock(hooks_mutex);
        for (auto& hooks : constraint_hooks) {
            detach_listener(hooks->set_region);
            detach_listener(hooks->destroy);
        }
        constraint_hooks.clear();
    }

    if (xwayland) {
        wlr_xwayland_destroy(xwayland);
        xwayland = nullptr;
    }

    if (keyboard) {
        keyboard.reset();
    }

    if (xkb_ctx) {
        xkb_context_unref(xkb_ctx);
        xkb_ctx = nullptr;
    }

    if (seat) {
        wlr_seat_destroy(seat);
        seat = nullptr;
    }

    xdg_shell = nullptr;
    layer_shell = nullptr;
    compositor = nullptr;
    output = nullptr;
    if (present_swapchain) {
        wlr_swapchain_destroy(present_swapchain);
        present_swapchain = nullptr;
    }

    if (output_layout) {
        wlr_output_layout_destroy(output_layout);
        output_layout = nullptr;
    }

    if (allocator) {
        wlr_allocator_destroy(allocator);
        allocator = nullptr;
    }

    if (renderer) {
        wlr_renderer_destroy(renderer);
        renderer = nullptr;
    }

    if (backend) {
        wlr_backend_destroy(backend);
        backend = nullptr;
    }

    if (display) {
        wl_display_destroy(display);
        display = nullptr;
    }

    event_loop = nullptr;
    wayland_socket_name.clear();
}

} // namespace goggles::input
