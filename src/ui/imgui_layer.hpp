#pragma once

#include <array>
#include <chrono>
#include <cstdint>
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

namespace goggles::util {
struct AppDirs;
}

namespace goggles::input {
struct SurfaceInfo;
}

namespace goggles::ui {

/// @brief Preset catalog tree node (directory or preset file).
struct PresetTreeNode {
    std::map<std::string, PresetTreeNode> children;
    int preset_index = -1; // -1 for directories, >= 0 for preset files
};

/// @brief Vulkan objects required to initialize ImGui rendering.
struct ImGuiConfig {
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    uint32_t queue_family;
    vk::Queue queue;
    vk::Format swapchain_format;
    uint32_t image_count;
};

/// @brief UI state for a single shader parameter.
struct ParameterState {
    size_t pass_index;
    render::ShaderParameter info;
    float current_value;
};

/// @brief Aggregate UI state for shader controls.
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
using SurfaceSelectCallback = std::function<void(uint32_t surface_id)>;
using SurfaceResetCallback = std::function<void()>;

/// @brief ImGui overlay layer for shader control and debug widgets.
class ImGuiLayer {
public:
    /// @brief Creates an ImGui overlay for `window`.
    /// @param window SDL window receiving input events.
    /// @param config Vulkan objects needed for ImGui rendering.
    /// @param app_dirs Resolved app directories for ini/font and preset discovery.
    /// @return An ImGui layer or an error.
    [[nodiscard]] static auto create(SDL_Window* window, const ImGuiConfig& config,
                                     const util::AppDirs& app_dirs) -> ResultPtr<ImGuiLayer>;

    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    /// @brief Releases ImGui and Vulkan resources owned by this layer.
    void shutdown();

    /// @brief Feeds an SDL event into ImGui input handling.
    void process_event(const SDL_Event& event);
    /// @brief Begins a new ImGui frame.
    void begin_frame();
    /// @brief Ends the frame and updates internal UI state.
    void end_frame();
    /// @brief Records ImGui draw data into `cmd`.
    /// @param cmd Command buffer in recording state.
    /// @param target_view Swapchain image view to render into.
    /// @param extent Target extent in pixels.
    void record(vk::CommandBuffer cmd, vk::ImageView target_view, vk::Extent2D extent);

    /// @brief Sets the list of preset files shown in the UI.
    void set_preset_catalog(std::vector<std::filesystem::path> presets);
    /// @brief Updates the currently selected preset path.
    void set_current_preset(const std::filesystem::path& path);
    /// @brief Updates displayed parameter values.
    void set_parameters(std::vector<ParameterState> params);

    /// @brief Sets a callback invoked when a parameter is changed by the UI.
    void set_parameter_change_callback(ParameterChangeCallback callback);
    /// @brief Sets a callback invoked when parameters should be reset.
    void set_parameter_reset_callback(ParameterResetCallback callback);

    /// @brief Returns mutable UI state (owned by this layer).
    [[nodiscard]] auto state() -> ShaderControlState& { return m_state; }
    /// @brief Returns UI state (owned by this layer).
    [[nodiscard]] auto state() const -> const ShaderControlState& { return m_state; }
    /// @brief Returns true if ImGui wants exclusive keyboard input.
    [[nodiscard]] auto wants_capture_keyboard() const -> bool;
    /// @brief Returns true if ImGui wants exclusive mouse input.
    [[nodiscard]] auto wants_capture_mouse() const -> bool;

    void toggle_visibility() { m_visible = !m_visible; }
    [[nodiscard]] auto is_visible() const -> bool { return m_visible; }
    void toggle_debug_overlay() { m_debug_overlay_visible = !m_debug_overlay_visible; }
    [[nodiscard]] auto is_debug_overlay_visible() const -> bool { return m_debug_overlay_visible; }

    void toggle_surface_selector() { m_surface_selector_visible = !m_surface_selector_visible; }
    [[nodiscard]] auto is_surface_selector_visible() const -> bool {
        return m_surface_selector_visible;
    }
    /// @brief Updates the displayed surface list.
    void set_surfaces(std::vector<input::SurfaceInfo> surfaces);
    /// @brief Updates whether manual override is active.
    void set_manual_override_active(bool active);
    /// @brief Sets the callback invoked when a surface is selected.
    void set_surface_select_callback(SurfaceSelectCallback callback);
    /// @brief Sets the callback invoked when "Reset to Auto" is clicked.
    void set_surface_reset_callback(SurfaceResetCallback callback);

    /// @brief Rebuilds ImGui resources after a swapchain format change.
    void rebuild_for_format(vk::Format new_format);
    /// @brief Records a timing sample for the source (captured) frame cadence.
    void notify_source_frame();

private:
    ImGuiLayer() = default;
    void draw_shader_controls();
    void draw_parameter_controls();
    void draw_preset_tree(const PresetTreeNode& node);
    void draw_filtered_presets();
    void draw_debug_overlay();
    void draw_surface_selector();
    void rebuild_preset_tree();
    [[nodiscard]] auto matches_filter(const std::filesystem::path& path) const -> bool;

    std::filesystem::path m_font_path;
    std::string m_ini_path;
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
    SurfaceSelectCallback m_on_surface_select;
    SurfaceResetCallback m_on_surface_reset;
    std::vector<input::SurfaceInfo> m_surfaces;
    float m_last_display_scale = 1.0F;
    bool m_visible = true;
    bool m_debug_overlay_visible = true;
    bool m_surface_selector_visible = false;
    bool m_manual_override_active = false;
    bool m_initialized = false;

    static constexpr size_t K_FRAME_HISTORY_SIZE = 120;
    std::array<float, K_FRAME_HISTORY_SIZE> m_frame_times{};
    std::array<float, K_FRAME_HISTORY_SIZE> m_source_frame_times{};
    size_t m_frame_idx = 0;
    size_t m_source_frame_idx = 0;
    std::chrono::steady_clock::time_point m_last_frame_time;
    std::chrono::steady_clock::time_point m_last_source_frame_time;
};

} // namespace goggles::ui
