#include "application.hpp"

#include "sdl_platform.hpp"
#include "ui_controller.hpp"

#include <SDL3/SDL.h>
#include <capture/capture_receiver.hpp>
#include <cstdlib>
#include <input/input_forwarder.hpp>
#include <render/backend/vulkan_backend.hpp>
#include <string>
#include <util/config.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>
#include <util/unique_fd.hpp>
#include <utility>

namespace goggles::app {

static auto to_sdl_event(EventRef event) -> const SDL_Event* {
    return event.ptr ? static_cast<const SDL_Event*>(event.ptr) : nullptr;
}

static auto to_sdl_window(WindowHandle window) -> SDL_Window* {
    return window.ptr ? static_cast<SDL_Window*>(window.ptr) : nullptr;
}

auto Application::create(const Config& config, bool enable_input_forwarding)
    -> ResultPtr<Application> {
    auto app = std::unique_ptr<Application>(new Application());

    app->m_platform = GOGGLES_TRY(SdlPlatform::create({
        .title = "Goggles",
        .width = 1280,
        .height = 720,
        .enable_vulkan = true,
        .resizable = true,
    }));
    GOGGLES_LOG_INFO("SDL3 initialized");

    auto* sdl_window = to_sdl_window(app->m_platform->window());

    app->m_vulkan_backend = GOGGLES_TRY(
        render::VulkanBackend::create(sdl_window, config.render.enable_validation, "shaders",
                                      config.render.scale_mode, config.render.integer_scale));

    app->m_vulkan_backend->load_shader_preset(config.shader.preset);

    app->m_ui_controller = GOGGLES_TRY(
        UiController::create(app->m_platform->window(), *app->m_vulkan_backend, config));

    auto receiver_result = CaptureReceiver::create();
    if (!receiver_result) {
        GOGGLES_LOG_WARN("Capture disabled - running in viewer-only mode ({})",
                         receiver_result.error().message);
    } else {
        app->m_capture_receiver = std::move(receiver_result.value());
    }

    if (enable_input_forwarding) {
        GOGGLES_LOG_INFO("Initializing input forwarding...");
        auto input_forwarder_result = input::InputForwarder::create();

        if (!input_forwarder_result) {
            GOGGLES_LOG_WARN("Input forwarding disabled: {}",
                             input_forwarder_result.error().message);
        } else {
            app->m_input_forwarder = std::move(input_forwarder_result.value());
            GOGGLES_LOG_INFO("Input forwarding: DISPLAY={} WAYLAND_DISPLAY={}",
                             app->m_input_forwarder->x11_display(),
                             app->m_input_forwarder->wayland_display());
        }
    } else {
        GOGGLES_LOG_INFO("Input forwarding disabled");
    }

    return make_result_ptr(std::move(app));
}

Application::~Application() {
    shutdown();
}

void Application::shutdown() {
    if (m_ui_controller) {
        m_ui_controller.reset();
    }

    if (m_capture_receiver) {
        m_capture_receiver.reset();
    }

    m_input_forwarder.reset();

    if (m_vulkan_backend) {
        m_vulkan_backend.reset();
    }

    if (m_platform) {
        m_platform.reset();
    }
}

void Application::run() {
    while (m_running) {
        GOGGLES_PROFILE_FRAME("Main");
        pump_events();
        tick_frame();
    }
}

auto Application::x11_display() const -> std::string {
    if (!m_input_forwarder) {
        return "";
    }
    return m_input_forwarder->x11_display();
}

auto Application::wayland_display() const -> std::string {
    if (!m_input_forwarder) {
        return "";
    }
    return m_input_forwarder->wayland_display();
}

auto Application::gpu_index() const -> uint32_t {
    return m_vulkan_backend ? m_vulkan_backend->gpu_index() : 0;
}

auto Application::gpu_uuid() const -> std::string {
    return m_vulkan_backend ? m_vulkan_backend->gpu_uuid() : "";
}

void Application::pump_events() {
    SDL_Event event;
    {
        GOGGLES_PROFILE_SCOPE("EventProcessing");
        while (SDL_PollEvent(&event)) {
            handle_event({.ptr = &event});
        }
    }
}

void Application::handle_event(EventRef event) {
    const auto* sdl_event = to_sdl_event(event);
    if (sdl_event == nullptr) {
        GOGGLES_LOG_ERROR("handle_event: null SDL_Event (EventRef.ptr is null)");
        return;
    }
    if (m_ui_controller) {
        m_ui_controller->process_event(event);
    }

    if (sdl_event->type == SDL_EVENT_QUIT) {
        GOGGLES_LOG_INFO("Quit event received");
        m_running = false;
        return;
    }

    if (sdl_event->type == SDL_EVENT_WINDOW_RESIZED) {
        m_window_resized = true;
        return;
    }
    if (sdl_event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        sdl_event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        m_window_resized = true;
        return;
    }

    if (sdl_event->type == SDL_EVENT_KEY_DOWN && sdl_event->key.key == SDLK_F1 && m_ui_controller &&
        m_ui_controller->enabled()) {
        m_ui_controller->toggle_visibility();
        return;
    }

    forward_input_event(event);
}

void Application::forward_input_event(EventRef event) {
    if (!m_input_forwarder) {
        return;
    }

    const auto* sdl_event = to_sdl_event(event);
    if (sdl_event == nullptr) {
        GOGGLES_LOG_ERROR("forward_input_event: null SDL_Event (EventRef.ptr is null)");
        return;
    }

    bool capture_kb = m_ui_controller && m_ui_controller->wants_capture_keyboard();
    bool capture_mouse = m_ui_controller && m_ui_controller->wants_capture_mouse();

    switch (sdl_event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        if (capture_kb) {
            return;
        }
        auto result = m_input_forwarder->forward_key(sdl_event->key);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (capture_mouse) {
            return;
        }
        auto result = m_input_forwarder->forward_mouse_button(sdl_event->button);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (capture_mouse) {
            return;
        }
        auto result = m_input_forwarder->forward_mouse_motion(sdl_event->motion);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        if (capture_mouse) {
            return;
        }
        auto result = m_input_forwarder->forward_mouse_wheel(sdl_event->wheel);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    default:
        return;
    }
}

void Application::handle_sync_semaphores() {
    if (!m_capture_receiver || !m_vulkan_backend) {
        return;
    }

    if (!m_capture_receiver->semaphores_updated()) {
        return;
    }

    auto ready_fd = util::UniqueFd::dup_from(m_capture_receiver->get_frame_ready_fd());
    auto consumed_fd = util::UniqueFd::dup_from(m_capture_receiver->get_frame_consumed_fd());

    if (!ready_fd || !consumed_fd) {
        GOGGLES_LOG_ERROR("Failed to dup semaphore fds (ready_fd={}, consumed_fd={})",
                          m_capture_receiver->get_frame_ready_fd(),
                          m_capture_receiver->get_frame_consumed_fd());
        return;
    }

    auto import_result =
        m_vulkan_backend->import_sync_semaphores(std::move(ready_fd), std::move(consumed_fd));
    if (!import_result) {
        GOGGLES_LOG_ERROR("Failed to import sync semaphores: {}", import_result.error().message);
        m_capture_receiver->clear_sync_semaphores();
    } else {
        GOGGLES_LOG_INFO("Sync semaphores imported successfully");
        m_capture_receiver->clear_sync_semaphores();
    }

    m_capture_receiver->clear_semaphores_updated();
}

void Application::tick_frame() {
    if (!m_vulkan_backend) {
        return;
    }

    if (m_pending_format != 0 || m_window_resized) {
        GOGGLES_PROFILE_SCOPE("SwapchainRebuild");
        m_vulkan_backend->wait_all_frames();

        if (m_pending_format != 0) {
            auto fmt = static_cast<vk::Format>(m_pending_format);
            m_pending_format = 0;
            m_window_resized = false;
            auto result = m_vulkan_backend->rebuild_for_format(fmt);
            if (result) {
                if (m_ui_controller) {
                    m_ui_controller->rebuild_for_format(m_vulkan_backend->swapchain_format());
                }
            } else {
                GOGGLES_LOG_ERROR("Format rebuild failed: {}", result.error().message);
            }
        } else {
            m_window_resized = false;
            auto result = m_vulkan_backend->handle_resize();
            if (!result) {
                GOGGLES_LOG_ERROR("Resize failed: {}", result.error().message);
            }
        }
    }

    if (m_capture_receiver) {
        GOGGLES_PROFILE_SCOPE("CaptureReceive");
        m_capture_receiver->poll_frame();
    }

    handle_sync_semaphores();

    if (m_capture_receiver && m_capture_receiver->has_frame()) {
        auto& frame = m_capture_receiver->get_frame();
        auto source_format = static_cast<vk::Format>(frame.format);
        if (m_vulkan_backend->needs_format_rebuild(source_format)) {
            m_pending_format = frame.format;
            return;
        }
    }

    if (m_ui_controller && m_ui_controller->enabled()) {
        m_ui_controller->apply_state(*m_vulkan_backend);
        m_ui_controller->begin_frame();
    }

    bool needs_resize = false;
    auto ui_callback = [this](vk::CommandBuffer cmd, vk::ImageView view, vk::Extent2D extent) {
        if (m_ui_controller && m_ui_controller->enabled()) {
            m_ui_controller->end_frame();
            m_ui_controller->record(cmd, view, extent);
        }
    };

    if (m_capture_receiver && m_capture_receiver->has_frame()) {
        GOGGLES_PROFILE_SCOPE("RenderFrame");
        auto render_result = (m_ui_controller && m_ui_controller->enabled())
                                 ? m_vulkan_backend->render_frame_with_ui(
                                       m_capture_receiver->get_frame(), ui_callback)
                                 : m_vulkan_backend->render_frame(m_capture_receiver->get_frame());
        if (!render_result) {
            GOGGLES_LOG_ERROR("Render failed: {}", render_result.error().message);
        } else {
            needs_resize = !*render_result;
        }
    } else {
        GOGGLES_PROFILE_SCOPE("RenderClear");
        auto render_result = (m_ui_controller && m_ui_controller->enabled())
                                 ? m_vulkan_backend->render_clear_with_ui(ui_callback)
                                 : m_vulkan_backend->render_clear();
        if (!render_result) {
            GOGGLES_LOG_ERROR("Clear failed: {}", render_result.error().message);
        } else {
            needs_resize = !*render_result;
        }
    }

    if (m_ui_controller && m_vulkan_backend->consume_chain_swapped()) {
        m_ui_controller->sync_from_backend(*m_vulkan_backend);
    }

    if (needs_resize) {
        GOGGLES_PROFILE_SCOPE("HandleResize");
        auto resize_result = m_vulkan_backend->handle_resize();
        if (!resize_result) {
            GOGGLES_LOG_ERROR("Resize failed: {}", resize_result.error().message);
        }
    }
}

} // namespace goggles::app
