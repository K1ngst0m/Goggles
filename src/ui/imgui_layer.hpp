#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <render/shader/retroarch_preprocessor.hpp>
#include <string>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

struct SDL_Window;
union SDL_Event;

namespace goggles::ui {

struct PresetTreeNode {
    std::map<std::string, PresetTreeNode> children;
    int preset_index = -1; // -1 for directories, >= 0 for preset files
};

struct ImGuiConfig {
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    uint32_t queue_family;
    vk::Queue queue;
    vk::Format swapchain_format;
    uint32_t image_count;
};

struct ParameterState {
    size_t pass_index;
    render::ShaderParameter info;
    float current_value;
};

struct ShaderControlState {
    std::filesystem::path current_preset;
    std::vector<std::filesystem::path> preset_catalog;
    std::vector<ParameterState> parameters;
    std::array<char, 256> search_filter{};
    bool shader_enabled = false;
    int selected_preset_index = -1;
    bool reload_requested = false;
    bool parameters_dirty = false;
};

using ParameterChangeCallback =
    std::function<void(size_t pass_index, const std::string& name, float value)>;
using ParameterResetCallback = std::function<void()>;

class ImGuiLayer {
public:
    [[nodiscard]] static auto create(SDL_Window* window, const ImGuiConfig& config)
        -> ResultPtr<ImGuiLayer>;

    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    void shutdown();

    void process_event(const SDL_Event& event);
    void begin_frame();
    void end_frame();
    void record(vk::CommandBuffer cmd, vk::ImageView target_view, vk::Extent2D extent);

    void set_preset_catalog(std::vector<std::filesystem::path> presets);
    void set_current_preset(const std::filesystem::path& path);
    void set_parameters(std::vector<ParameterState> params);

    void set_parameter_change_callback(ParameterChangeCallback callback);
    void set_parameter_reset_callback(ParameterResetCallback callback);

    [[nodiscard]] auto state() -> ShaderControlState& { return m_state; }
    [[nodiscard]] auto state() const -> const ShaderControlState& { return m_state; }
    [[nodiscard]] auto wants_capture_keyboard() const -> bool;
    [[nodiscard]] auto wants_capture_mouse() const -> bool;

    void toggle_visibility() { m_visible = !m_visible; }
    [[nodiscard]] auto is_visible() const -> bool { return m_visible; }

    void rebuild_for_format(vk::Format new_format);

private:
    ImGuiLayer() = default;
    void draw_shader_controls();
    void draw_parameter_controls();
    void draw_preset_tree(const PresetTreeNode& node);
    void draw_filtered_presets();
    void rebuild_preset_tree();
    [[nodiscard]] auto matches_filter(const std::filesystem::path& path) const -> bool;

    std::filesystem::path m_font_path;
    float m_font_size_pixels = 17.0F;
    SDL_Window* m_window = nullptr;
    vk::Instance m_instance;
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    uint32_t m_queue_family = 0;
    vk::Queue m_queue;
    vk::DescriptorPool m_descriptor_pool;
    vk::Format m_swapchain_format = vk::Format::eUndefined;
    uint32_t m_image_count = 0;

    ShaderControlState m_state;
    PresetTreeNode m_preset_tree;
    ParameterChangeCallback m_on_parameter_change;
    ParameterResetCallback m_on_parameter_reset;
    float m_last_display_scale = 1.0F;
    bool m_visible = true;
    bool m_initialized = false;
};

} // namespace goggles::ui
