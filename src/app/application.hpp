#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <util/config.hpp>
#include <util/error.hpp>
#include <util/paths.hpp>

struct SDL_Window;
union SDL_Event;

namespace goggles {

class CaptureReceiver;
struct Config;

namespace input {
class InputForwarder;
}

namespace render {
class VulkanBackend;
}

namespace ui {
class ImGuiLayer;
}

namespace app {

class Application {
public:
    [[nodiscard]] static auto create(const Config& config, const util::AppDirs& app_dirs,
                                     bool enable_input_forwarding) -> ResultPtr<Application>;

    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void shutdown();

    void run();
    void pump_events();
    void handle_event(const SDL_Event& event);
    void tick_frame();

    [[nodiscard]] auto is_running() const -> bool { return m_running; }
    [[nodiscard]] auto x11_display() const -> std::string;
    [[nodiscard]] auto wayland_display() const -> std::string;
    [[nodiscard]] auto gpu_index() const -> uint32_t;
    [[nodiscard]] auto gpu_uuid() const -> std::string;

private:
    Application() = default;

    void forward_input_event(const SDL_Event& event);
    void handle_sync_semaphores();
    void update_pointer_lock_mirror();
    void sync_prechain_ui();

    SDL_Window* m_window = nullptr;
    bool m_sdl_initialized = false;
    std::unique_ptr<render::VulkanBackend> m_vulkan_backend;
    std::unique_ptr<ui::ImGuiLayer> m_imgui_layer;
    std::unique_ptr<CaptureReceiver> m_capture_receiver;
    std::unique_ptr<input::InputForwarder> m_input_forwarder;

    bool m_running = true;
    bool m_window_resized = false;
    bool m_initial_resolution_sent = false;
    bool m_last_shader_enabled = false;
    bool m_pointer_lock_override = false;
    bool m_pointer_lock_mirrored = false;
    uint32_t m_pending_format = 0;
    uint64_t m_last_source_frame_number = UINT64_MAX;
    ScaleMode m_scale_mode = ScaleMode::fill;
};

} // namespace app
} // namespace goggles
