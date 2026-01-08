#pragma once

#include <memory>
#include <util/error.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles {

struct Config;

namespace ui {
class ImGuiLayer;
}

namespace render {
class VulkanBackend;
}

namespace app {

struct WindowHandle;
struct EventRef;

class UiController {
public:
    [[nodiscard]] static auto create(app::WindowHandle window,
                                     render::VulkanBackend& vulkan_backend, const Config& config)
        -> ResultPtr<UiController>;

    ~UiController();

    UiController(const UiController&) = delete;
    UiController& operator=(const UiController&) = delete;
    UiController(UiController&&) = delete;
    UiController& operator=(UiController&&) = delete;

    void shutdown();

    [[nodiscard]] auto enabled() const -> bool;

    void process_event(app::EventRef event);
    [[nodiscard]] auto wants_capture_keyboard() const -> bool;
    [[nodiscard]] auto wants_capture_mouse() const -> bool;

    void toggle_visibility();

    void apply_state(render::VulkanBackend& vulkan_backend);
    void begin_frame();
    void end_frame();
    void record(vk::CommandBuffer cmd, vk::ImageView target_view, vk::Extent2D extent);

    void sync_from_backend(render::VulkanBackend& vulkan_backend);

    void rebuild_for_format(vk::Format new_format);

private:
    UiController() = default;

    std::unique_ptr<ui::ImGuiLayer> m_imgui_layer;
    bool m_last_shader_enabled = false;
};

} // namespace app
} // namespace goggles
