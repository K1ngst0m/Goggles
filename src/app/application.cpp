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
#include <unordered_set>
#include <util/config.hpp>
#include <util/drm_fourcc.hpp>
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

static auto configure_cursor_theme_env(const util::AppDirs& app_dirs) -> Result<void> {
    auto assets_dir = util::resource_path(app_dirs, "assets");
    std::string cursor_path = assets_dir.string();
    if (setenv("XCURSOR_PATH", cursor_path.c_str(), 1) != 0) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to set XCURSOR_PATH");
    }
    if (setenv("XCURSOR_SIZE", "64", 1) != 0) {
        return make_error<void>(ErrorCode::input_init_failed, "Failed to set XCURSOR_SIZE");
    }
    return Result<void>{};
}

// =============================================================================
// Lifecycle
// =============================================================================

auto Application::init_sdl() -> Result<void> {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return make_error<void>(ErrorCode::unknown_error,
                                "Failed to initialize SDL3: " + std::string(SDL_GetError()));
    }
    m_sdl_initialized = true;

    auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                                     SDL_WINDOW_HIGH_PIXEL_DENSITY);
    m_window = SDL_CreateWindow("Goggles", 1280, 720, window_flags);
    if (m_window == nullptr) {
        return make_error<void>(ErrorCode::unknown_error,
                                "Failed to create window: " + std::string(SDL_GetError()));
    }
    GOGGLES_LOG_INFO("SDL3 initialized");
    return Result<void>{};
}

auto Application::init_vulkan_backend(const Config& config, const util::AppDirs& app_dirs)
    -> Result<void> {
    render::RenderSettings render_settings{
        .scale_mode = config.render.scale_mode,
        .integer_scale = config.render.integer_scale,
        .target_fps = config.render.target_fps,
        .gpu_selector = config.render.gpu_selector,
        .source_width = config.render.source_width,
        .source_height = config.render.source_height,
    };

    GOGGLES_LOG_INFO("Scale mode: {}", to_string(config.render.scale_mode));

    m_vulkan_backend = GOGGLES_MUST(render::VulkanBackend::create(
        m_window, config.render.enable_validation, util::resource_path(app_dirs, "shaders"),
        util::cache_path(app_dirs, "shaders"), render_settings));

    m_vulkan_backend->load_shader_preset(config.shader.preset);
    return Result<void>{};
}

auto Application::init_imgui_layer(const util::AppDirs& app_dirs) -> Result<void> {
    ui::ImGuiConfig imgui_config{
        .instance = m_vulkan_backend->instance(),
        .physical_device = m_vulkan_backend->physical_device(),
        .device = m_vulkan_backend->device(),
        .queue_family = m_vulkan_backend->graphics_queue_family(),
        .queue = m_vulkan_backend->graphics_queue(),
        .swapchain_format = m_vulkan_backend->swapchain_format(),
        .image_count = m_vulkan_backend->swapchain_image_count(),
    };

    m_imgui_layer = GOGGLES_MUST(ui::ImGuiLayer::create(m_window, imgui_config, app_dirs));
    GOGGLES_LOG_INFO("ImGui layer initialized");
    return Result<void>{};
}

auto Application::init_shader_system(const Config& config, const util::AppDirs& app_dirs)
    -> Result<void> {
    std::filesystem::path preset_dir = util::data_path(app_dirs, "shaders/retroarch");
    std::error_code ec;
    if (!std::filesystem::exists(preset_dir, ec) || ec) {
        preset_dir = util::resource_path(app_dirs, "shaders/retroarch");
    }
    GOGGLES_LOG_INFO("Preset catalog directory: {}", preset_dir.string());

    m_imgui_layer->set_preset_catalog(scan_presets(preset_dir));
    m_imgui_layer->set_current_preset(m_vulkan_backend->current_preset_path());
    m_imgui_layer->state().shader_enabled = !config.shader.preset.empty();

    m_imgui_layer->set_parameter_change_callback(
        [&backend = *m_vulkan_backend](size_t /*pass_index*/, const std::string& name,
                                       float value) {
            backend.filter_chain()->set_parameter(name, value);
        });
    m_imgui_layer->set_parameter_reset_callback(
        [&backend = *m_vulkan_backend, layer = m_imgui_layer.get()]() {
            backend.filter_chain()->clear_parameter_overrides();
            update_ui_parameters(backend, *layer);
        });
    m_imgui_layer->set_prechain_change_callback(
        [&backend = *m_vulkan_backend](uint32_t width, uint32_t height) {
            backend.set_prechain_resolution(width, height);
        });
    m_imgui_layer->set_prechain_parameter_callback(
        [&backend = *m_vulkan_backend](const std::string& name, float value) {
            backend.filter_chain()->set_prechain_parameter(name, value);
        });
    m_imgui_layer->set_prechain_scale_mode_callback([this](ScaleMode mode, uint32_t integer_scale) {
        m_vulkan_backend->set_scale_mode(mode);
        m_vulkan_backend->set_integer_scale(integer_scale);

        if (mode != ScaleMode::dynamic) {
            return;
        }

        m_initial_resolution_sent = false;
        if (!m_capture_receiver || !m_capture_receiver->is_connected()) {
            return;
        }

        auto extent = m_vulkan_backend->swapchain_extent();
        if (extent.width > 0 && extent.height > 0) {
            m_capture_receiver->request_resolution(extent.width, extent.height);
            m_initial_resolution_sent = true;
        }
    });

    auto prechain_res = m_vulkan_backend->get_prechain_resolution();
    m_imgui_layer->set_prechain_state(prechain_res, m_vulkan_backend->get_scale_mode(),
                                      m_vulkan_backend->get_integer_scale());
    m_imgui_layer->set_prechain_parameters(
        m_vulkan_backend->filter_chain()->get_prechain_parameters());

    update_ui_parameters(*m_vulkan_backend, *m_imgui_layer);
    return Result<void>{};
}

auto Application::init_capture_receiver() -> Result<void> {
    m_capture_receiver = GOGGLES_TRY(CaptureReceiver::create());
    return Result<void>{};
}

auto Application::init_compositor_server(const util::AppDirs& app_dirs) -> Result<void> {
    GOGGLES_LOG_INFO("Initializing compositor server...");
    auto cursor_env_result = configure_cursor_theme_env(app_dirs);
    if (!cursor_env_result) {
        GOGGLES_LOG_WARN("Cursor theme setup failed: {}", cursor_env_result.error().message);
    }
    m_compositor_server = GOGGLES_MUST(input::CompositorServer::create());
    GOGGLES_LOG_INFO("Compositor server: DISPLAY={} WAYLAND_DISPLAY={}",
                     m_compositor_server->x11_display(), m_compositor_server->wayland_display());

    m_imgui_layer->set_surface_select_callback(
        [app_ptr = this, compositor = m_compositor_server.get()](uint32_t surface_id) {
            compositor->set_input_target(surface_id);
            app_ptr->m_surface_frame.reset();
            app_ptr->m_last_source_frame_number = UINT64_MAX;
        });
    m_imgui_layer->set_surface_filter_toggle_callback([this](uint32_t surface_id, bool enabled) {
        set_surface_filter_enabled(surface_id, enabled);
        request_surface_resize(surface_id, !compute_surface_filter_chain_enabled(surface_id));
    });
    return Result<void>{};
}

auto Application::create(const Config& config, const util::AppDirs& app_dirs)
    -> ResultPtr<Application> {
    auto app = std::unique_ptr<Application>(new Application());

    GOGGLES_MUST(app->init_sdl());
    GOGGLES_MUST(app->init_vulkan_backend(config, app_dirs));
    GOGGLES_MUST(app->init_imgui_layer(app_dirs));
    GOGGLES_MUST(app->init_shader_system(config, app_dirs));
    GOGGLES_MUST(app->init_capture_receiver());
    GOGGLES_MUST(app->init_compositor_server(app_dirs));

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
        process_event();
        tick_frame();
    }
}

void Application::process_event() {
    GOGGLES_PROFILE_SCOPE("EventProcessing");
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
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

    // Poll compositor for pointer lock state changes
    update_pointer_lock_mirror();
    update_cursor_visibility();
    update_mouse_grab();
}

void Application::forward_input_event(const SDL_Event& event) {
    if (!m_compositor_server) {
        return;
    }

    // Block input to target app when ImGui has focus
    const bool ui_visible = m_imgui_layer->is_globally_visible();
    bool capture_kb = ui_visible && m_imgui_layer->wants_capture_keyboard();
    bool capture_mouse = ui_visible && m_imgui_layer->wants_capture_mouse();

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
        if (ui_visible || capture_mouse) {
            return;
        }
        auto result = m_compositor_server->forward_mouse_button(event.button);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (ui_visible || capture_mouse) {
            return;
        }
        auto result = m_compositor_server->forward_mouse_motion(event.motion);
        if (!result) {
            GOGGLES_LOG_ERROR("Failed to forward input: {}", result.error().message);
        }
        return;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        if (ui_visible || capture_mouse) {
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
            m_imgui_layer->set_prechain_state(captured, m_vulkan_backend->get_scale_mode(),
                                              m_vulkan_backend->get_integer_scale());
            m_vulkan_backend->set_prechain_resolution(captured.width, captured.height);
        }
    }

    if (prechain.pass_parameters.empty()) {
        auto params = m_vulkan_backend->filter_chain()->get_prechain_parameters();
        if (!params.empty()) {
            m_imgui_layer->set_prechain_parameters(std::move(params));
        }
    }
}

void Application::sync_surface_filters(std::vector<input::SurfaceInfo>& surfaces) {
    std::unordered_set<uint32_t> seen;
    seen.reserve(surfaces.size());

    for (auto& surface : surfaces) {
        seen.insert(surface.id);
        auto it = m_surface_state.find(surface.id);
        if (it == m_surface_state.end()) {
            SurfaceRuntimeState state{};
            state.filter_enabled = surface.capture_path == input::SurfaceCapturePath::vulkan;
            it = m_surface_state.emplace(surface.id, state).first;
        }
        surface.filter_chain_enabled = it->second.filter_enabled;
        if (surface.width > 0 && surface.height > 0) {
            if (!it->second.has_resize_state || !it->second.resize.maximized) {
                it->second.restore_width = static_cast<uint32_t>(surface.width);
                it->second.restore_height = static_cast<uint32_t>(surface.height);
                it->second.has_restore_size = true;
            }
        }
    }

    for (auto it = m_surface_state.begin(); it != m_surface_state.end();) {
        if (!seen.contains(it->first)) {
            it = m_surface_state.erase(it);
        } else {
            ++it;
        }
    }

    uint32_t active_id = 0;
    for (const auto& surface : surfaces) {
        if (surface.is_input_target) {
            active_id = surface.id;
            break;
        }
    }
    m_active_surface_id = active_id;
}

auto Application::compute_global_filter_chain_enabled() const -> bool {
    if (!m_imgui_layer) {
        return true;
    }
    const auto& state = m_imgui_layer->state();
    return state.window_filter_chain_enabled;
}

auto Application::compute_surface_filter_chain_enabled(uint32_t surface_id) const -> bool {
    if (!compute_global_filter_chain_enabled()) {
        return false;
    }
    if (surface_id == 0) {
        return true;
    }
    return is_surface_filter_enabled(surface_id);
}

auto Application::compute_global_effect_stage_enabled() const -> bool {
    if (!m_imgui_layer) {
        return true;
    }
    const auto& state = m_imgui_layer->state();
    return state.shader_enabled && state.window_filter_chain_enabled;
}

auto Application::compute_surface_effect_stage_enabled(uint32_t surface_id) const -> bool {
    if (!compute_global_effect_stage_enabled()) {
        return false;
    }
    if (surface_id == 0) {
        return true;
    }
    return is_surface_filter_enabled(surface_id);
}

void Application::set_surface_filter_enabled(uint32_t surface_id, bool enabled) {
    if (surface_id == 0) {
        return;
    }
    auto it = m_surface_state.find(surface_id);
    if (it == m_surface_state.end()) {
        return;
    }
    it->second.filter_enabled = enabled;
}

auto Application::is_surface_filter_enabled(uint32_t surface_id) const -> bool {
    auto it = m_surface_state.find(surface_id);
    return it != m_surface_state.end() ? it->second.filter_enabled : false;
}

void Application::request_surface_resize(uint32_t surface_id, bool maximize) {
    if (!m_compositor_server || surface_id == 0) {
        return;
    }

    auto extent = m_vulkan_backend->swapchain_extent();
    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    auto it = m_surface_state.find(surface_id);
    if (it == m_surface_state.end()) {
        return;
    }
    auto& surface_state = it->second;

    SurfaceResizeState desired{};
    desired.maximized = maximize;
    if (maximize) {
        desired.width = extent.width;
        desired.height = extent.height;
    } else if (surface_state.has_restore_size) {
        desired.width = surface_state.restore_width;
        desired.height = surface_state.restore_height;
    }

    if (surface_state.has_resize_state) {
        if (surface_state.resize.maximized == desired.maximized &&
            surface_state.resize.width == desired.width &&
            surface_state.resize.height == desired.height) {
            return;
        }
    }

    surface_state.resize = desired;
    surface_state.has_resize_state = true;
    input::SurfaceResizeInfo resize{};
    resize.width = desired.width;
    resize.height = desired.height;
    resize.maximized = desired.maximized;
    m_compositor_server->request_surface_resize(surface_id, resize);
}

void Application::update_surface_resize_for_surfaces(
    const std::vector<input::SurfaceInfo>& surfaces) {
    const bool global_enabled = compute_global_filter_chain_enabled();
    for (const auto& surface : surfaces) {
        const bool surface_enabled = is_surface_filter_enabled(surface.id);
        const bool should_maximize = !(global_enabled && surface_enabled);
        request_surface_resize(surface.id, should_maximize);
    }
}

void Application::handle_swapchain_changes() {
    m_skip_frame = false;

    if (m_pending_format != 0 || m_window_resized || m_vulkan_backend->needs_resize()) {
        GOGGLES_PROFILE_SCOPE("SwapchainRebuild");
        m_vulkan_backend->wait_all_frames();
        m_window_resized = false;

        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(m_window, &width, &height)) {
            GOGGLES_LOG_ERROR("SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());
            m_skip_frame = true;
            return;
        }

        if (width == 0 || height == 0) {
            m_skip_frame = true;
            return;
        }

        auto fmt = vk::Format::eUndefined;
        if (m_pending_format != 0) {
            fmt = static_cast<vk::Format>(m_pending_format);
            m_pending_format = 0;
        }

        auto result = m_vulkan_backend->recreate_swapchain(static_cast<uint32_t>(width),
                                                           static_cast<uint32_t>(height), fmt);
        if (result) {
            if (fmt != vk::Format::eUndefined) {
                m_imgui_layer->rebuild_for_format(m_vulkan_backend->swapchain_format());
            }
        } else {
            GOGGLES_LOG_ERROR("Swapchain rebuild failed: {}", result.error().message);
        }

        if (fmt == vk::Format::eUndefined &&
            m_vulkan_backend->get_scale_mode() == ScaleMode::dynamic &&
            m_capture_receiver->is_connected()) {
            auto extent = m_vulkan_backend->swapchain_extent();
            if (extent.width > 0 && extent.height > 0) {
                m_capture_receiver->request_resolution(extent.width, extent.height);
            }
        }
    }
}

void Application::update_frame_sources() {
    if (m_skip_frame) {
        return;
    }

    m_capture_receiver->poll_frame();

    // Send initial resolution request once connected
    if (m_vulkan_backend->get_scale_mode() == ScaleMode::dynamic && !m_initial_resolution_sent &&
        m_capture_receiver->is_connected()) {
        auto extent = m_vulkan_backend->swapchain_extent();
        if (extent.width > 0 && extent.height > 0) {
            m_capture_receiver->request_resolution(extent.width, extent.height);
            m_initial_resolution_sent = true;
        }
    }

    handle_sync_semaphores();

    if (!m_capture_receiver->has_frame() && m_compositor_server) {
        uint64_t last_surface_frame_number = m_surface_frame ? m_surface_frame->frame_number : 0;
        auto surface_frame = m_compositor_server->get_presented_frame(last_surface_frame_number);
        if (surface_frame) {
            m_surface_frame = std::move(*surface_frame);
        }
    }

    // Check if captured frame requires format rebuild
    if (m_capture_receiver->has_frame()) {
        auto& frame = m_capture_receiver->get_frame();
        auto target_format =
            render::VulkanBackend::get_matching_swapchain_format(frame.image.format);
        if (target_format != m_vulkan_backend->swapchain_format()) {
            m_pending_format = static_cast<uint32_t>(frame.image.format);
            m_skip_frame = true;
            return;
        }
    } else if (m_surface_frame) {
        if (m_surface_frame->image.format != vk::Format::eUndefined) {
            auto target_format =
                render::VulkanBackend::get_matching_swapchain_format(m_surface_frame->image.format);
            if (target_format != m_vulkan_backend->swapchain_format()) {
                m_pending_format = static_cast<uint32_t>(m_surface_frame->image.format);
                m_skip_frame = true;
                return;
            }
        }
    }
}

void Application::sync_ui_state() {
    if (m_skip_frame) {
        return;
    }

    auto& state = m_imgui_layer->state();
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
    if (m_compositor_server) {
        auto surfaces = m_compositor_server->get_surfaces();
        sync_surface_filters(surfaces);
        update_surface_resize_for_surfaces(surfaces);
        m_imgui_layer->set_surfaces(std::move(surfaces));
    }

    sync_prechain_ui();

    if (m_vulkan_backend->consume_chain_swapped()) {
        m_imgui_layer->state().current_preset = m_vulkan_backend->current_preset_path();
        update_ui_parameters(*m_vulkan_backend, *m_imgui_layer);
    }

    m_imgui_layer->begin_frame();
}

void Application::render_frame() {
    if (m_skip_frame) {
        return;
    }

    const util::ExternalImageFrame* source_frame = nullptr;
    uint64_t source_frame_number = 0;
    bool using_surface_frame = false;

    if (m_capture_receiver->has_frame()) {
        source_frame = &m_capture_receiver->get_frame();
        source_frame_number = source_frame->frame_number;
    } else if (m_surface_frame) {
        if (m_surface_frame->image.format == vk::Format::eUndefined) {
            GOGGLES_LOG_DEBUG("Skipping surface frame with unsupported DRM format");
        } else if (m_surface_frame->image.modifier == util::DRM_FORMAT_MOD_INVALID) {
            GOGGLES_LOG_DEBUG("Skipping surface frame with invalid DMA-BUF modifier");
        } else {
            if (m_surface_frame->image.handle) {
                source_frame = &m_surface_frame.value();
                source_frame_number = m_surface_frame->frame_number;
                using_surface_frame = true;
            }
        }
    }

    if (source_frame && source_frame_number != m_last_source_frame_number) {
        m_last_source_frame_number = source_frame_number;
        m_imgui_layer->notify_source_frame();
    }
    if (source_frame) {
        GOGGLES_PROFILE_VALUE("goggles_source_frame", static_cast<double>(source_frame_number));
    }

    const bool filter_chain_enabled =
        using_surface_frame ? compute_surface_filter_chain_enabled(m_active_surface_id)
                            : compute_global_filter_chain_enabled();
    m_vulkan_backend->set_prechain_enabled(filter_chain_enabled);

    const bool effect_stage_enabled =
        using_surface_frame ? compute_surface_effect_stage_enabled(m_active_surface_id)
                            : compute_global_effect_stage_enabled();
    m_vulkan_backend->set_shader_enabled(effect_stage_enabled);

    auto ui_callback = [this](vk::CommandBuffer cmd, vk::ImageView view, vk::Extent2D extent) {
        m_imgui_layer->end_frame();
        m_imgui_layer->record(cmd, view, extent);
    };
    [[maybe_unused]] const char* scope_name = source_frame ? "RenderFrame" : "RenderClear";
    [[maybe_unused]] const char* error_label = source_frame ? "Render" : "Clear";
    GOGGLES_PROFILE_SCOPE("Render");
    GOGGLES_PROFILE_TAG(scope_name);
    auto render_result = m_vulkan_backend->render(source_frame, ui_callback);
    if (!render_result) {
        GOGGLES_LOG_ERROR("{} failed: {}", error_label, render_result.error().message);
    }
}

void Application::tick_frame() {
    handle_swapchain_changes();
    update_frame_sources();
    sync_ui_state();
    render_frame();
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
    return m_vulkan_backend->gpu_index();
}

auto Application::gpu_uuid() const -> std::string {
    return m_vulkan_backend->gpu_uuid();
}

void Application::update_pointer_lock_mirror() {
    if (!m_compositor_server || !m_imgui_layer) {
        return;
    }

    const bool ui_visible = m_imgui_layer->is_globally_visible();
    bool should_lock = !ui_visible;
    if (should_lock != m_pointer_lock_mirrored) {
        SDL_SetWindowRelativeMouseMode(m_window, should_lock);
        m_pointer_lock_mirrored = should_lock;
        GOGGLES_LOG_DEBUG("Pointer lock mirror: {}", should_lock ? "ON" : "OFF");
    }
}

void Application::update_cursor_visibility() {
    if (!m_window || !m_imgui_layer) {
        return;
    }

    const bool ui_visible = m_imgui_layer->is_globally_visible();
    const bool should_show = ui_visible;

    if (should_show != m_cursor_visible) {
        if (should_show) {
            SDL_ShowCursor();
        } else {
            SDL_HideCursor();
        }
        m_cursor_visible = should_show;
    }

    if (m_compositor_server) {
        m_compositor_server->set_cursor_visible(!should_show);
    }
}

void Application::update_mouse_grab() {
    if (!m_window || !m_imgui_layer) {
        return;
    }

    const bool ui_visible = m_imgui_layer->is_globally_visible();
    const bool should_grab = !ui_visible;
    if (should_grab != m_mouse_grabbed) {
        SDL_SetWindowMouseGrab(m_window, should_grab);
        m_mouse_grabbed = should_grab;
    }
}

} // namespace goggles::app
