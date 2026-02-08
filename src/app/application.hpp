#pragma once

#include <compositor/compositor_server.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/external_image.hpp>
#include <util/paths.hpp>

struct SDL_Window;
union SDL_Event;

namespace goggles {

class CaptureReceiver;
struct Config;

namespace render {
class VulkanBackend;
}

namespace ui {
class ImGuiLayer;
}

namespace app {

class Application {
public:
    [[nodiscard]] static auto create(const Config& config, const util::AppDirs& app_dirs)
        -> ResultPtr<Application>;

    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void shutdown();

    void run();
    void process_event();
    void tick_frame();

    [[nodiscard]] auto is_running() const -> bool { return m_running; }
    [[nodiscard]] auto x11_display() const -> std::string;
    [[nodiscard]] auto wayland_display() const -> std::string;
    [[nodiscard]] auto gpu_index() const -> uint32_t;
    [[nodiscard]] auto gpu_uuid() const -> std::string;

private:
    enum class SessionCaptureMode : uint8_t {
        direct_vulkan,
        compositor,
    };

    Application() = default;

    void forward_input_event(const SDL_Event& event);
    [[nodiscard]] auto init_sdl() -> Result<void>;
    [[nodiscard]] auto init_vulkan_backend(const Config& config, const util::AppDirs& app_dirs)
        -> Result<void>;
    [[nodiscard]] auto init_imgui_layer(const util::AppDirs& app_dirs) -> Result<void>;
    [[nodiscard]] auto init_shader_system(const Config& config, const util::AppDirs& app_dirs)
        -> Result<void>;
    [[nodiscard]] auto init_capture_receiver() -> Result<void>;
    [[nodiscard]] auto init_compositor_server(const util::AppDirs& app_dirs) -> Result<void>;
    void handle_swapchain_changes();
    void update_frame_sources();
    void sync_ui_state();
    void render_frame();
    void handle_sync_semaphores();
    void update_pointer_lock_mirror();
    void update_cursor_visibility();
    void update_mouse_grab();
    void sync_prechain_ui();
    void sync_surface_filters(std::vector<input::SurfaceInfo>& surfaces);
    void update_surface_resize_for_surfaces(const std::vector<input::SurfaceInfo>& surfaces);
    [[nodiscard]] auto compute_global_filter_chain_enabled() const -> bool;
    [[nodiscard]] auto compute_surface_filter_chain_enabled(uint32_t surface_id) const -> bool;
    struct StagePolicy {
        bool prechain_enabled = true;
        bool effect_stage_enabled = true;
    };
    [[nodiscard]] auto compute_stage_policy(bool using_surface_frame) const -> StagePolicy;
    [[nodiscard]] auto is_direct_vulkan_session() const -> bool;
    void request_surface_resize(uint32_t surface_id, bool maximize);
    void set_surface_filter_enabled(uint32_t surface_id, bool enabled);
    [[nodiscard]] auto is_surface_filter_enabled(uint32_t surface_id) const -> bool;

    SDL_Window* m_window = nullptr;
    bool m_sdl_initialized = false;
    std::unique_ptr<render::VulkanBackend> m_vulkan_backend;
    std::unique_ptr<ui::ImGuiLayer> m_imgui_layer;
    std::unique_ptr<CaptureReceiver> m_capture_receiver;
    std::unique_ptr<input::CompositorServer> m_compositor_server;
    std::optional<util::ExternalImageFrame> m_surface_frame;

    struct SurfaceResizeState {
        bool maximized = false;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    struct SurfaceRuntimeState {
        bool filter_enabled = false;
        bool filter_explicitly_set = false;
        SurfaceResizeState resize;
        bool has_resize_state = false;
        uint32_t restore_width = 0;
        uint32_t restore_height = 0;
        bool has_restore_size = false;
    };
    std::unordered_map<uint32_t, SurfaceRuntimeState> m_surface_state;
    uint32_t m_active_surface_id = 0;

    bool m_running = true;
    bool m_window_resized = false;
    bool m_initial_resolution_sent = false;
    bool m_pointer_lock_mirrored = false;
    bool m_cursor_visible = true;
    bool m_mouse_grabbed = false;
    bool m_skip_frame = false;
    uint32_t m_pending_format = 0;
    uint64_t m_last_source_frame_number = UINT64_MAX;
    SessionCaptureMode m_session_capture_mode = SessionCaptureMode::direct_vulkan;
};

} // namespace app
} // namespace goggles
