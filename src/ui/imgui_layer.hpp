#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <goggles/error.hpp>
#include <goggles/filter_chain/filter_controls.hpp>
#include <map>
#include <string>
#include <util/config.hpp>
#include <util/runtime_metrics.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

struct SDL_Window;
union SDL_Event;

namespace goggles::util {
struct AppDirs;
}

namespace goggles::compositor {
struct SurfaceInfo;
}

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
    goggles::fc::FilterControlDescriptor descriptor;
    float current_value;
};

enum class ResolutionProfile : std::uint8_t {
    disabled = 0, // Pre-chain disabled (pass-through)
    p240 = 1,     // NES, SNES, Genesis, N64, PS1, Saturn
    p288 = 2,     // PS2 240p mode, Wii VC
    p480 = 3,     // Dreamcast, GameCube, PS2, Xbox, Wii
    i480 = 4,     // Interlaced variant
    p720 = 5,     // Xbox 360, PS3, Wii U era
    p1080 = 6,    // PS3/360+, modern HD
    custom = 7,   // User-defined resolution
};

struct PreChainState {
    ScaleMode scale_mode = ScaleMode::stretch;
    uint32_t integer_scale = 0;
    ResolutionProfile profile = ResolutionProfile::disabled;
    uint32_t target_width = 0;
    uint32_t target_height = 0;
    bool dirty = false;
    std::vector<goggles::fc::FilterControlDescriptor> pass_parameters;
};

struct ShaderControlState {
    std::filesystem::path current_preset;
    std::vector<std::filesystem::path> preset_catalog;
    std::vector<ParameterState> parameters;
    std::array<char, 256> search_filter{};
    bool shader_enabled = false;
    bool window_filter_chain_enabled = true;
    int selected_preset_index = -1;
    bool reload_requested = false;
    bool parameters_dirty = false;
    PreChainState prechain;
};

class ImGuiLayer {
public:
    [[nodiscard]] static auto create(SDL_Window* window, const ImGuiConfig& config,
                                     const util::AppDirs& app_dirs) -> ResultPtr<ImGuiLayer>;

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

    void set_parameter_change_callback(
        std::function<void(goggles::fc::FilterControlId, float)> callback);
    void set_parameter_reset_callback(std::function<void()> callback);
    void set_prechain_change_callback(std::function<void(uint32_t, uint32_t)> callback);
    void set_prechain_state(vk::Extent2D resolution, ScaleMode scale_mode, uint32_t integer_scale);
    void set_prechain_parameters(std::vector<goggles::fc::FilterControlDescriptor> params);
    void set_prechain_parameter_callback(
        std::function<void(goggles::fc::FilterControlId, float)> callback);
    void set_prechain_scale_mode_callback(std::function<void(ScaleMode, uint32_t)> callback);
    void set_runtime_metrics(util::CompositorRuntimeMetricsSnapshot metrics);
    void set_target_fps(uint32_t target_fps);
    void set_target_fps_change_callback(std::function<void(uint32_t)> callback);

    [[nodiscard]] auto state() -> ShaderControlState& { return m_state; }
    [[nodiscard]] auto state() const -> const ShaderControlState& { return m_state; }
    [[nodiscard]] auto wants_capture_keyboard() const -> bool;
    [[nodiscard]] auto wants_capture_mouse() const -> bool;

    void toggle_global_visibility() { m_global_visible = !m_global_visible; }
    [[nodiscard]] auto is_globally_visible() const -> bool { return m_global_visible; }

    void set_surfaces(std::vector<compositor::SurfaceInfo> surfaces);
    void set_surface_select_callback(std::function<void(uint32_t)> callback);
    void set_surface_filter_toggle_callback(std::function<void(uint32_t, bool)> callback);

    void rebuild_for_format(vk::Format new_format);

private:
    ImGuiLayer() = default;
    void draw_shader_controls();
    void draw_prechain_stage_controls();
    void draw_prechain_scale_and_profile_controls(PreChainState& prechain);
    void draw_prechain_pass_parameter_controls(PreChainState& prechain);
    void draw_effect_stage_controls();
    void draw_postchain_stage_controls();
    void draw_parameter_controls();
    void draw_preset_tree(const PresetTreeNode& node);
    void draw_filtered_presets();
    void draw_app_management();
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
    std::function<void(goggles::fc::FilterControlId, float)> m_on_parameter_change;
    std::function<void()> m_on_parameter_reset;
    std::function<void(uint32_t, uint32_t)> m_on_prechain_change;
    std::function<void(goggles::fc::FilterControlId, float)> m_on_prechain_parameter;
    std::function<void(ScaleMode, uint32_t)> m_on_prechain_scale_mode;
    std::function<void(uint32_t)> m_on_surface_select;
    std::function<void(uint32_t, bool)> m_on_surface_filter_toggle;
    std::function<void(uint32_t)> m_on_target_fps_change;
    std::vector<compositor::SurfaceInfo> m_surfaces;
    util::CompositorRuntimeMetricsSnapshot m_runtime_metrics;
    uint32_t m_target_fps = 60;
    uint32_t m_last_capped_target_fps = 60;
    float m_last_display_scale = 1.0F;
    bool m_global_visible = true;
    bool m_initialized = false;
};

} // namespace goggles::ui
