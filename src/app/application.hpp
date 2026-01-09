#pragma once

#include <memory>
#include <string>
#include <util/error.hpp>

namespace goggles {

class CaptureReceiver;
struct Config;

namespace input {
class InputForwarder;
}

namespace render {
class VulkanBackend;
}

namespace app {

class SdlPlatform;
class UiController;
struct WindowHandle;
struct EventRef;

class Application {
public:
    [[nodiscard]] static auto create(const Config& config, bool enable_input_forwarding)
        -> ResultPtr<Application>;

    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void shutdown();

    void run();
    void pump_events();
    void handle_event(EventRef event);
    void tick_frame();

    [[nodiscard]] auto is_running() const -> bool { return m_running; }
    [[nodiscard]] auto x11_display() const -> std::string;
    [[nodiscard]] auto wayland_display() const -> std::string;
    [[nodiscard]] auto gpu_index() const -> uint32_t;
    [[nodiscard]] auto gpu_uuid() const -> std::string;

private:
    Application() = default;

    void forward_input_event(EventRef event);
    void handle_sync_semaphores();

    std::unique_ptr<SdlPlatform> m_platform;
    std::unique_ptr<render::VulkanBackend> m_vulkan_backend;
    std::unique_ptr<UiController> m_ui_controller;
    std::unique_ptr<CaptureReceiver> m_capture_receiver;
    std::unique_ptr<input::InputForwarder> m_input_forwarder;

    bool m_running = true;
    bool m_window_resized = false;
};

} // namespace app
} // namespace goggles
