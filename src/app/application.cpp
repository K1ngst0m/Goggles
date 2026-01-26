#include "application.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <capture/capture_receiver.hpp>
#include <compositor/compositor_server.hpp>
#include <cstdlib>
#include <filesystem>
#include <ranges>
#include <render/backend/vulkan_backend.hpp>
#include <render/chain/filter_chain.hpp>
#include <string>
#include <string_view>
#include <ui/imgui_layer.hpp>
#include <util/config.hpp>
#include <util/drm_format.hpp>
#include <util/logging.hpp>
#include <util/paths.hpp>
#include <util/profiling.hpp>
#include <util/unique_fd.hpp>
#include <utility>
#include <vector>

namespace goggles::app {

// =============================================================================
// Helper Functions
// =============================================================================

static auto scan_presets(const std::filesystem::path& dir) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> presets;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) {
        return presets;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
         it != std::filesystem::recursive_directory_iterator() && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec) && !ec && it->path().extension() == ".slangp") {
            presets.push_back(it->path());
        }
    }
    std::ranges::sort(presets);
    return presets;
}

static void update_ui_parameters(render::VulkanBackend& vulkan_backend,
                                 ui::ImGuiLayer& imgui_layer) {
    auto* chain = vulkan_backend.filter_chain();
    if (chain == nullptr) {
        return;
    }

    auto params = chain->get_all_parameters();
    std::vector<ui::ParameterState> ui_params;
    ui_params.reserve(params.size());

    for (const auto& p : params) {
        ui_params.push_back({
            .pass_index = 0,
            .info =
                {
                    .name = p.name,
                    .description = p.description,
                    .default_value = p.default_value,
                    .min_value = p.min_value,
                    .max_value = p.max_value,
                    .step = p.step,
                },
            .current_value = p.current_value,
        });
    }
    imgui_layer.set_parameters(std::move(ui_params));
}

// =============================================================================
// Lifecycle
// =============================================================================

// NOLINTNEXTLINE(readability-function-size) TODO(kingstom): refactor to smaller units.
auto Application::create(const Config& config, const util::AppDirs& app_dirs,
                         bool enable_input_forwarding) -> ResultPtr<Application> {
    auto app = std::unique_ptr<Application>(new Application());

    // SDL initialization
    {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            return make_result_ptr_error<Application>(ErrorCode::unknown_error,
                                                      "Failed to initialize SDL3: " +
                                                          std::string(SDL_GetError()));
        }
        app->m_sdl_initialized = true;

        auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                                         SDL_WINDOW_HIGH_PIXEL_DENSITY);
        app->m_window = SDL_CreateWindow("Goggles", 1280, 720, window_flags);
        if (app->m_window == nullptr) {
            return make_result_ptr_error<Application>(ErrorCode::unknown_error,
                                                      "Failed to create window: " +
                                                          std::string(SDL_GetError()));
        }
        GOGGLES_LOG_INFO("SDL3 initialized");
    }

    // Vulkan backend
    {
        render::RenderSettings render_settings{
            .scale_mode = config.render.scale_mode,
            .integer_scale = config.render.integer_scale,
            .target_fps = config.render.target_fps,
            .source_width = config.render.source_width,
            .source_height = config.render.source_height,
        };

        app->m_scale_mode = config.render.scale_mode;
        GOGGLES_LOG_INFO("Scale mode: {}", to_string(app->m_scale_mode));

        app->m_vulkan_backend = GOGGLES_TRY(
            render::VulkanBackend::create(app->m_window, config.render.enable_validation,
                                          util::resource_path(app_dirs, "shaders"),
                                          util::cache_path(app_dirs, "shaders"), render_settings));

        app->m_vulkan_backend->load_shader_preset(config.shader.preset);
    }

    // ImGui layer
    {
        ui::ImGuiConfig imgui_config{
            .instance = app->m_vulkan_backend->instance(),
            .physical_device = app->m_vulkan_backend->physical_device(),
            .device = app->m_vulkan_backend->device(),
            .queue_family = app->m_vulkan_backend->graphics_queue_family(),
            .queue = app->m_vulkan_backend->graphics_queue(),
            .swapchain_format = app->m_vulkan_backend->swapchain_format(),
            .image_count = app->m_vulkan_backend->swapchain_image_count(),
        };

        app->m_imgui_layer =
            GOGGLES_TRY(ui::ImGuiLayer::create(app->m_window, imgui_config, app_dirs));
        GOGGLES_LOG_INFO("ImGui layer initialized");
    }

    // Shader preset catalog
    {
        std::filesystem::path preset_dir = util::data_path(app_dirs, "shaders/retroarch");
        std::error_code ec;
        if (!std::filesystem::exists(preset_dir, ec) || ec) {
            preset_dir = util::resource_path(app_dirs, "shaders/retroarch");
        }
        GOGGLES_LOG_INFO("Preset catalog directory: {}", preset_dir.string());

        app->m_imgui_layer->set_preset_catalog(scan_presets(preset_dir));
        app->m_imgui_layer->set_current_preset(app->m_vulkan_backend->current_preset_path());
        app->m_imgui_layer->state().shader_enabled = !config.shader.preset.empty();

        app->m_imgui_layer->set_parameter_change_callback(
            [&backend = *app->m_vulkan_backend](size_t /*pass_index*/, const std::string& name,
                                                float value) {
                if (auto* chain = backend.filter_chain()) {
                    chain->set_parameter(name, value);
                }
            });
        app->m_imgui_layer->set_parameter_reset_callback(
            [&backend = *app->m_vulkan_backend, layer = app->m_imgui_layer.get()]() {
                if (auto* chain = backend.filter_chain()) {
                    chain->clear_parameter_overrides();
                    update_ui_parameters(backend, *layer);
                }
            });
        app->m_imgui_layer->set_prechain_change_callback(
            [&backend = *app->m_vulkan_backend](uint32_t width, uint32_t height) {
                backend.set_prechain_resolution(width, height);
            });
        app->m_imgui_layer->set_prechain_parameter_callback(
            [&backend = *app->m_vulkan_backend](const std::string& name, float value) {
                backend.set_prechain_parameter(name, value);
            });

        auto prechain_res = app->m_vulkan_backend->get_prechain_resolution();
        app->m_imgui_layer->set_prechain_state(prechain_res);
        app->m_imgui_layer->set_prechain_parameters(
            app->m_vulkan_backend->get_prechain_parameters());

        update_ui_parameters(*app->m_vulkan_backend, *app->m_imgui_layer);
    }

    // Capture receiver (optional - viewer-only mode if unavailable)
    {
        auto receiver_result = CaptureReceiver::create();
        if (!receiver_result) {
            GOGGLES_LOG_WARN("Capture disabled - running in viewer-only mode ({})",
                             receiver_result.error().message);
        } else {
            app->m_capture_receiver = std::move(receiver_result.value());
        }
    }

    // Input forwarding (optional)
    if (enable_input_forwarding) {
        GOGGLES_LOG_INFO("Initializing input forwarding...");
        auto compositor_result = input::CompositorServer::create();

        if (!compositor_result) {
            GOGGLES_LOG_WARN("Input forwarding disabled: {}", compositor_result.error().message);
        } else {
            app->m_compositor_server = std::move(compositor_result.value());
            GOGGLES_LOG_INFO("Input forwarding: DISPLAY={} WAYLAND_DISPLAY={}",
                             app->m_compositor_server->x11_display(),
                             app->m_compositor_server->wayland_display());

            app->m_imgui_layer->set_surface_select_callback(
                [app_ptr = app.get(),
                 compositor = app->m_compositor_server.get()](uint32_t surface_id) {
                    compositor->set_input_target(surface_id);
                    app_ptr->m_surface_frame.reset();
                    app_ptr->m_last_surface_frame_number = 0;
                    app_ptr->m_last_source_frame_number = UINT64_MAX;
                });
            app->m_imgui_layer->set_surface_reset_callback(
                [app_ptr = app.get(), compositor = app->m_compositor_server.get()]() {
                    compositor->clear_input_override();
                    app_ptr->m_surface_frame.reset();
                    app_ptr->m_last_surface_frame_number = 0;
                    app_ptr->m_last_source_frame_number = UINT64_MAX;
                });
            app->m_imgui_layer->set_pointer_lock_override_callback(
                [app_ptr = app.get()](bool override_active) {
                    app_ptr->m_pointer_lock_override = override_active;
                    app_ptr->update_pointer_lock_mirror();
                });
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
    // Destroy in reverse order of creation
    m_imgui_layer.reset();
    m_capture_receiver.reset();
    m_compositor_server.reset();
    m_vulkan_backend.reset();

    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdl_initialized) {
        SDL_Quit();
        m_sdl_initialized = false;
    }
}

// =============================================================================
// Run Loop
// =============================================================================

void Application::run() {
    while (m_running) {
        GOGGLES_PROFILE_FRAME("Main");
        pump_events();
        tick_frame();
    }
}

void Application::pump_events() {
    GOGGLES_PROFILE_SCOPE("EventProcessing");
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handle_event(event);
    }

    // Poll compositor for pointer lock state changes
    update_pointer_lock_mirror();
}

// =============================================================================
// Event Handling
// =============================================================================

void Application::handle_event(const SDL_Event& event) {
    m_imgui_layer->process_event(event);

    switch (event.type) {
    case SDL_EVENT_QUIT:
        GOGGLES_LOG_INFO("Quit event received");
        m_running = false;
        return;

    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        m_window_resized = true;
        return;

    case SDL_EVENT_KEY_DOWN: {
        bool has_ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
        bool has_alt = (event.key.mod & SDL_KMOD_ALT) != 0;
        bool has_shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
        if (has_ctrl && has_alt && has_shift && event.key.key == SDLK_Q) {
            m_imgui_layer->toggle_global_visibility();
            return;
        }
        break;
    }

    default:
        break;
    }

    forward_input_event(event);
}

void Application::forward_input_event(const SDL_Event& event) {
    if (!m_compositor_server) {
        return;
    }

    // Block input to target app when ImGui has focus
    bool capture_kb = m_imgui_layer->wants_capture_keyboard();
    bool capture_mouse = m_imgui_layer->wants_capture_mouse();

    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        if (capture_kb) {
            return;
        }
        auto result = m_compositor_server->forward_key(event.key);
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
        auto result = m_compositor_server->forward_mouse_button(event.button);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (capture_mouse) {
            return;
        }
        auto result = m_compositor_server->forward_mouse_motion(event.motion);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        if (capture_mouse) {
            return;
        }
        auto result = m_compositor_server->forward_mouse_wheel(event.wheel);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    default:
        return;
    }
}

// =============================================================================
// Frame Processing
// =============================================================================

void Application::handle_sync_semaphores() {
    if (!m_capture_receiver || !m_vulkan_backend) {
        return;
    }
    if (!m_capture_receiver->semaphores_updated()) {
        return;
    }

    // Dup fds because Vulkan import takes ownership
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
    } else {
        GOGGLES_LOG_INFO("Sync semaphores imported successfully");
    }
    m_capture_receiver->clear_sync_semaphores();
    m_capture_receiver->clear_semaphores_updated();
}

void Application::sync_prechain_ui() {
    auto& prechain = m_imgui_layer->state().prechain;
    if (prechain.target_width == 0 && prechain.target_height == 0) {
        auto captured = m_vulkan_backend->get_captured_extent();
        if (captured.width > 0 && captured.height > 0) {
            m_imgui_layer->set_prechain_state(captured);
            m_vulkan_backend->set_prechain_resolution(captured.width, captured.height);
        }
    }

    if (prechain.pass_parameters.empty()) {
        auto params = m_vulkan_backend->get_prechain_parameters();
        if (!params.empty()) {
            m_imgui_layer->set_prechain_parameters(std::move(params));
        }
    }
}

// NOLINTNEXTLINE(readability-function-size) TODO(kingstom): refactor to smaller units.
void Application::tick_frame() {
    if (!m_vulkan_backend) {
        return;
    }

    // Swapchain rebuild (format change or window resize)
    if (m_pending_format != 0 || m_window_resized) {
        GOGGLES_PROFILE_SCOPE("SwapchainRebuild");
        m_vulkan_backend->wait_all_frames();

        if (m_pending_format != 0) {
            auto fmt = static_cast<vk::Format>(m_pending_format);
            m_pending_format = 0;
            m_window_resized = false;
            auto result = m_vulkan_backend->rebuild_for_format(fmt);
            if (result) {
                m_imgui_layer->rebuild_for_format(m_vulkan_backend->swapchain_format());
            } else {
                GOGGLES_LOG_ERROR("Format rebuild failed: {}", result.error().message);
            }
        } else {
            m_window_resized = false;
            auto result = m_vulkan_backend->handle_resize();
            if (!result) {
                GOGGLES_LOG_ERROR("Resize failed: {}", result.error().message);
            }
            // Notify capture layer of new resolution in dynamic mode
            if (m_scale_mode == ScaleMode::dynamic && m_capture_receiver &&
                m_capture_receiver->is_connected()) {
                auto extent = m_vulkan_backend->swapchain_extent();
                if (extent.width > 0 && extent.height > 0) {
                    m_capture_receiver->request_resolution(extent.width, extent.height);
                }
            }
        }
    }

    // Capture polling
    if (m_capture_receiver) {
        GOGGLES_PROFILE_SCOPE("CaptureReceive");
        m_capture_receiver->poll_frame();

        // Send initial resolution request once connected
        if (m_scale_mode == ScaleMode::dynamic && !m_initial_resolution_sent &&
            m_capture_receiver->is_connected()) {
            auto extent = m_vulkan_backend->swapchain_extent();
            if (extent.width > 0 && extent.height > 0) {
                m_capture_receiver->request_resolution(extent.width, extent.height);
                m_initial_resolution_sent = true;
            }
        }
    }

    handle_sync_semaphores();

    if ((!m_capture_receiver || !m_capture_receiver->has_frame()) && m_compositor_server) {
        auto surface_frame = m_compositor_server->get_presented_frame(m_last_surface_frame_number);
        if (surface_frame) {
            m_surface_frame = std::move(*surface_frame);
            m_last_surface_frame_number = m_surface_frame->frame_number;
        }
    }

    // Check if captured frame requires format rebuild
    if (m_capture_receiver && m_capture_receiver->has_frame()) {
        auto& frame = m_capture_receiver->get_frame();
        auto source_format = static_cast<vk::Format>(frame.format);
        if (m_vulkan_backend->needs_format_rebuild(source_format)) {
            m_pending_format = frame.format;
            return;
        }
    } else if (m_surface_frame) {
        auto vk_format = util::drm_to_vk_format(m_surface_frame->format);
        if (vk_format != vk::Format::eUndefined) {
            if (m_vulkan_backend->needs_format_rebuild(vk_format)) {
                m_pending_format = static_cast<uint32_t>(vk_format);
                return;
            }
        }
    }

    // UI state sync (shader enable/disable, preset reload, surface selector)
    {
        auto& state = m_imgui_layer->state();
        if (state.shader_enabled != m_last_shader_enabled) {
            m_vulkan_backend->set_shader_enabled(state.shader_enabled);
            m_last_shader_enabled = state.shader_enabled;
        }
        if (state.reload_requested) {
            state.reload_requested = false;
            if (state.selected_preset_index >= 0 &&
                std::cmp_less(state.selected_preset_index, state.preset_catalog.size())) {
                const auto& preset =
                    state.preset_catalog[static_cast<size_t>(state.selected_preset_index)];
                if (auto result = m_vulkan_backend->reload_shader_preset(preset); !result) {
                    GOGGLES_LOG_ERROR("Failed to load preset '{}': {}", preset.string(),
                                      result.error().message);
                }
            }
        }
        if (m_imgui_layer->is_app_management_visible() && m_compositor_server) {
            m_imgui_layer->set_surfaces(m_compositor_server->get_surfaces());
            m_imgui_layer->set_manual_override_active(
                m_compositor_server->is_manual_override_active());
        }
        m_imgui_layer->begin_frame();
    }

    // Render
    bool needs_resize = false;
    {
        auto ui_callback = [this](vk::CommandBuffer cmd, vk::ImageView view, vk::Extent2D extent) {
            m_imgui_layer->end_frame();
            m_imgui_layer->record(cmd, view, extent);
        };

        const ::goggles::CaptureFrame* source_frame = nullptr;
        ::goggles::CaptureFrame surface_capture{};
        bool using_capture_receiver = false;
        uint64_t source_frame_number = 0;

        if (m_capture_receiver && m_capture_receiver->has_frame()) {
            source_frame = &m_capture_receiver->get_frame();
            using_capture_receiver = true;
            source_frame_number = source_frame->frame_number;
        } else if (m_surface_frame) {
            auto vk_format = util::drm_to_vk_format(m_surface_frame->format);
            if (vk_format == vk::Format::eUndefined) {
                GOGGLES_LOG_DEBUG("Skipping surface frame with unsupported DRM format");
            } else if (m_surface_frame->modifier == util::DRM_FORMAT_MOD_INVALID) {
                GOGGLES_LOG_DEBUG("Skipping surface frame with invalid DMA-BUF modifier");
            } else {
                surface_capture.width = m_surface_frame->width;
                surface_capture.height = m_surface_frame->height;
                surface_capture.stride = m_surface_frame->stride;
                surface_capture.offset = m_surface_frame->offset;
                surface_capture.format = static_cast<uint32_t>(vk_format);
                surface_capture.modifier = m_surface_frame->modifier;
                surface_capture.frame_number = m_surface_frame->frame_number;
                surface_capture.dmabuf_fd = m_surface_frame->dmabuf_fd.dup();
                if (surface_capture.dmabuf_fd) {
                    source_frame = &surface_capture;
                    source_frame_number = m_surface_frame->frame_number;
                }
            }
        }

        if (source_frame) {
            GOGGLES_PROFILE_SCOPE("RenderFrame");
            if (source_frame_number != m_last_source_frame_number) {
                m_last_source_frame_number = source_frame_number;
                m_imgui_layer->notify_source_frame();
            }
            auto render_result = m_vulkan_backend->render_frame_with_ui(*source_frame, ui_callback);
            if (!render_result) {
                GOGGLES_LOG_ERROR("Render failed: {}", render_result.error().message);
            } else {
                needs_resize = !*render_result;
            }

            if (using_capture_receiver) {
                sync_prechain_ui();
            }
        } else {
            GOGGLES_PROFILE_SCOPE("RenderClear");
            auto render_result = m_vulkan_backend->render_clear_with_ui(ui_callback);
            if (!render_result) {
                GOGGLES_LOG_ERROR("Clear failed: {}", render_result.error().message);
            } else {
                needs_resize = !*render_result;
            }
        }
    }

    // Post-render sync (update UI after shader chain swap)
    if (m_vulkan_backend->consume_chain_swapped()) {
        m_imgui_layer->state().current_preset = m_vulkan_backend->current_preset_path();
        update_ui_parameters(*m_vulkan_backend, *m_imgui_layer);
    }

    if (needs_resize) {
        GOGGLES_PROFILE_SCOPE("HandleResize");
        auto resize_result = m_vulkan_backend->handle_resize();
        if (!resize_result) {
            GOGGLES_LOG_ERROR("Resize failed: {}", resize_result.error().message);
        }
    }
}

// =============================================================================
// Accessors
// =============================================================================

auto Application::x11_display() const -> std::string {
    return m_compositor_server ? m_compositor_server->x11_display() : "";
}

auto Application::wayland_display() const -> std::string {
    return m_compositor_server ? m_compositor_server->wayland_display() : "";
}

auto Application::gpu_index() const -> uint32_t {
    return m_vulkan_backend ? m_vulkan_backend->gpu_index() : 0;
}

auto Application::gpu_uuid() const -> std::string {
    return m_vulkan_backend ? m_vulkan_backend->gpu_uuid() : "";
}

void Application::update_pointer_lock_mirror() {
    if (!m_compositor_server) {
        return;
    }

    bool should_lock = m_pointer_lock_override || m_compositor_server->is_pointer_locked();
    if (m_imgui_layer && m_imgui_layer->is_globally_visible()) {
        should_lock = false;
    }
    if (should_lock != m_pointer_lock_mirrored) {
        SDL_SetWindowRelativeMouseMode(m_window, should_lock);
        m_pointer_lock_mirrored = should_lock;
        GOGGLES_LOG_DEBUG("Pointer lock mirror: {}", should_lock ? "ON" : "OFF");
    }
}

} // namespace goggles::app
