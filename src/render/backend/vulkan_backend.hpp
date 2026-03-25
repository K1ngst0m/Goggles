#pragma once

#include "external_frame_importer.hpp"
#include "filter_chain_controller.hpp"
#include "render_output.hpp"
#include "vulkan_context.hpp"

#include <SDL3/SDL.h>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <goggles/filter_chain/filter_controls.hpp>
#include <goggles/filter_chain/scale_mode.hpp>
#include <util/external_image.hpp>
#include <vector>

namespace goggles::render {

struct RenderSettings {
    ScaleMode scale_mode = ScaleMode::stretch;
    uint32_t integer_scale = 0;
    uint32_t target_fps = 60;
    std::string gpu_selector;
    uint32_t source_width = 0;
    uint32_t source_height = 0;
};

struct FilterChainStagePolicy {
    bool prechain_enabled = true;
    bool effect_stage_enabled = true;
};

class VulkanBackend {
public:
    [[nodiscard]] static auto create(SDL_Window* window, bool enable_validation = false,
                                     const std::filesystem::path& cache_dir = {},
                                     const RenderSettings& settings = {})
        -> ResultPtr<VulkanBackend>;
    [[nodiscard]] static auto create_headless(bool enable_validation = false,
                                              const std::filesystem::path& cache_dir = {},
                                              const RenderSettings& settings = {})
        -> ResultPtr<VulkanBackend>;

    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;
    VulkanBackend(VulkanBackend&&) = delete;
    VulkanBackend& operator=(VulkanBackend&&) = delete;

    void shutdown();

    void load_shader_preset(const std::filesystem::path& preset_path);
    [[nodiscard]] auto reload_shader_preset(const std::filesystem::path& preset_path)
        -> Result<void>;
    void set_filter_chain_policy(const FilterChainStagePolicy& policy);

    using UiRenderCallback = std::function<void(vk::CommandBuffer, vk::ImageView, vk::Extent2D)>;
    [[nodiscard]] auto render(const util::ExternalImageFrame* frame,
                              const UiRenderCallback& ui_callback = nullptr) -> Result<void>;
    [[nodiscard]] auto readback_to_png(const std::filesystem::path& output) -> Result<void>;

    [[nodiscard]] auto needs_resize() const -> bool { return m_render_output.needs_resize; }

    [[nodiscard]] static auto get_matching_swapchain_format(vk::Format source_format) -> vk::Format;

    /// `source_format = eUndefined` means resize-only (keep current format).
    [[nodiscard]] auto recreate_swapchain(uint32_t width, uint32_t height,
                                          vk::Format source_format = vk::Format::eUndefined)
        -> Result<void>;
    void wait_all_frames();

    void set_prechain_resolution(uint32_t width, uint32_t height);

    [[nodiscard]] auto vulkan_context() const -> const backend_internal::VulkanContext& {
        return m_vulkan_context;
    }
    [[nodiscard]] auto render_output() const -> const backend_internal::RenderOutput& {
        return m_render_output;
    }
    [[nodiscard]] auto frame_importer() const -> const backend_internal::ExternalFrameImporter& {
        return m_external_frame_importer;
    }
    [[nodiscard]] auto filter_chain_controller() -> backend_internal::FilterChainController& {
        return m_filter_chain_controller;
    }
    [[nodiscard]] auto filter_chain_controller() const
        -> const backend_internal::FilterChainController& {
        return m_filter_chain_controller;
    }

    [[nodiscard]] auto get_scale_mode() const -> ScaleMode { return m_scale_mode; }
    [[nodiscard]] auto get_integer_scale() const -> uint32_t { return m_integer_scale; }
    void set_target_fps(uint32_t target_fps) { update_target_fps(target_fps); }
    void set_scale_mode(ScaleMode mode) { m_scale_mode = mode; }
    void set_integer_scale(uint32_t scale) { m_integer_scale = scale; }

private:
    VulkanBackend() = default;

    void initialize_paths(const std::filesystem::path& cache_dir);
    void initialize_settings(const RenderSettings& settings);
    void update_target_fps(uint32_t target_fps) { m_render_output.set_target_fps(target_fps); }

    [[nodiscard]] auto init_filter_chain() -> Result<void>;
    [[nodiscard]] auto make_device_info() const
        -> backend_internal::FilterChainController::VulkanDeviceInfo;
    [[nodiscard]] auto make_chain_config() const
        -> backend_internal::FilterChainController::ChainConfig;

    [[nodiscard]] auto record_render_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                              const UiRenderCallback& ui_callback = nullptr)
        -> Result<void>;
    [[nodiscard]] auto record_clear_commands(vk::CommandBuffer cmd, uint32_t image_index,
                                             const UiRenderCallback& ui_callback = nullptr)
        -> Result<void>;

    [[nodiscard]] static auto is_srgb_format(vk::Format format) -> bool;

    backend_internal::VulkanContext m_vulkan_context;
    backend_internal::RenderOutput m_render_output;
    backend_internal::ExternalFrameImporter m_external_frame_importer;
    backend_internal::FilterChainController m_filter_chain_controller;

    std::filesystem::path m_cache_dir;
    uint32_t m_integer_scale = 0;
    ScaleMode m_scale_mode = ScaleMode::stretch;

    [[nodiscard]] auto current_filter_target_extent() const -> vk::Extent2D;
};

} // namespace goggles::render
