#include "compositor_state.hpp"

#include <goggles/profiling.hpp>
#include <memory>
#include <util/logging.hpp>

namespace goggles::compositor {

CompositorServer::CompositorServer() : m_impl(std::make_unique<Impl>()) {}

CompositorServer::~CompositorServer() {
    stop();
}

auto CompositorServer::create() -> ResultPtr<CompositorServer> {
    GOGGLES_PROFILE_FUNCTION();
    auto server = std::make_unique<CompositorServer>();

    auto start_result = server->start();
    if (!start_result) {
        return nonstd::make_unexpected(
            Error{start_result.error().code, start_result.error().message});
    }

    return {std::move(server)};
}

auto CompositorServer::start() -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();
    auto& state = m_impl->state;
    auto cleanup_on_error = [this](void*) { stop(); };
    std::unique_ptr<void, decltype(cleanup_on_error)> guard(this, cleanup_on_error);

    auto run_step = [](auto&& step) -> Result<void> {
        auto result = step();
        if (!result) {
            return make_error<void>(result.error().code, result.error().message);
        }
        return {};
    };

    if (auto result = run_step([&] { return state.setup_base_components(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.create_allocator(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.create_compositor(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.create_output_layout(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.setup_xdg_shell(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.setup_layer_shell(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.setup_input_devices(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.setup_event_loop_fd(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.bind_wayland_socket(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.setup_xwayland(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.start_backend(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.setup_output(); }); !result) {
        return result;
    }
    if (auto result = run_step([&] { return state.initialize_present_output(); }); !result) {
        return result;
    }

    auto cursor_result = state.setup_cursor_theme();
    if (!cursor_result) {
        GOGGLES_LOG_WARN("Compositor cursor theme unavailable: {}", cursor_result.error().message);
    }

    state.start_compositor_thread();

    guard.release(); // NOLINT(bugprone-unused-return-value)
    return {};
}

void CompositorServer::stop() {
    GOGGLES_PROFILE_FUNCTION();
    m_impl->state.teardown();
}

auto CompositorServer::x11_display() const -> std::string {
    return m_impl->state.x11_display_name();
}

auto CompositorServer::wayland_display() const -> std::string {
    return m_impl->state.wayland_socket_name;
}

auto CompositorServer::target_fps() const -> uint32_t {
    return m_impl->state.target_fps.load(std::memory_order_acquire);
}

void CompositorServer::set_target_fps(uint32_t target_fps) {
    m_impl->state.target_fps.store(target_fps, std::memory_order_release);
    m_impl->state.wake_event_loop();
}

auto CompositorServer::get_runtime_metrics_snapshot() const
    -> util::CompositorRuntimeMetricsSnapshot {
    return m_impl->state.get_runtime_metrics_snapshot();
}

} // namespace goggles::compositor
