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

/// @brief Settings controlling viewport scaling and pacing.
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

/// @brief Vulkan renderer for presenting captured frames.
class VulkanBackend {
public:
    /// @brief Creates a Vulkan backend bound to an SDL window.
    /// @return A backend or an error.
    [[nodiscard]] static auto create(SDL_Window* window, bool enable_validation = false,
                                     const std::filesystem::path& cache_dir = {},
                                     const RenderSettings& settings = {})
        -> ResultPtr<VulkanBackend>;
    /// @brief Creates a headless Vulkan backend without a window or swapchain.
    [[nodiscard]] static auto create_headless(bool enable_validation = false,
                                              const std::filesystem::path& cache_dir = {},
                                              const RenderSettings& settings = {})
        -> ResultPtr<VulkanBackend>;

    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;
    VulkanBackend(VulkanBackend&&) = delete;
    VulkanBackend& operator=(VulkanBackend&&) = delete;

    /// @brief Releases GPU resources and stops background work.
    void shutdown();

    /// @brief Loads a shader preset from disk (async rebuild).
    void load_shader_preset(const std::filesystem::path& preset_path);
    /// @brief Reloads the current preset immediately.
    [[nodiscard]] auto reload_shader_preset(const std::filesystem::path& preset_path)
        -> Result<void>;
    [[nodiscard]] auto current_preset_path() const -> const std::filesystem::path& {
        return m_filter_chain_controller.current_preset_path();
    }

    void set_filter_chain_policy(const FilterChainStagePolicy& policy);

    using UiRenderCallback = std::function<void(vk::CommandBuffer, vk::ImageView, vk::Extent2D)>;
    /// @brief Renders a captured frame or clears the swapchain when no frame is provided.
    [[nodiscard]] auto render(const util::ExternalImageFrame* frame,
                              const UiRenderCallback& ui_callback = nullptr) -> Result<void>;
    /// @brief Reads back the offscreen image and writes it as PNG.
    [[nodiscard]] auto readback_to_png(const std::filesystem::path& output) -> Result<void>;

    [[nodiscard]] auto needs_resize() const -> bool { return m_render_output.needs_resize; }

    /// @brief Maps a source image format to a swapchain format.
    /// @param source_format Source image format to map.
    /// @return Swapchain format for presenting the source format.
    [[nodiscard]] static auto get_matching_swapchain_format(vk::Format source_format) -> vk::Format;

    /// @brief Recreates swapchain resources for the given size and optional source format.
    /// @param width Swapchain width in pixels.
    /// @param height Swapchain height in pixels.
    /// @param source_format Source image format or vk::Format::eUndefined for resize-only.
    /// @return Success or an error.
    [[nodiscard]] auto recreate_swapchain(uint32_t width, uint32_t height,
                                          vk::Format source_format = vk::Format::eUndefined)
        -> Result<void>;
    void wait_all_frames();

    [[nodiscard]] auto instance() const -> vk::Instance { return m_vulkan_context.instance; }
    [[nodiscard]] auto physical_device() const -> vk::PhysicalDevice {
        return m_vulkan_context.physical_device;
    }
    [[nodiscard]] auto device() const -> vk::Device { return m_vulkan_context.device; }
    [[nodiscard]] auto graphics_queue() const -> vk::Queue {
        return m_vulkan_context.graphics_queue;
    }
    [[nodiscard]] auto graphics_queue_family() const -> uint32_t {
        return m_vulkan_context.graphics_queue_family;
    }
    [[nodiscard]] auto swapchain_format() const -> vk::Format {
        return m_render_output.swapchain_format;
    }
    [[nodiscard]] auto swapchain_extent() const -> vk::Extent2D {
        return m_render_output.swapchain_extent;
    }
    [[nodiscard]] auto swapchain_image_count() const -> uint32_t {
        return m_render_output.image_count();
    }
    [[nodiscard]] auto get_prechain_resolution() const -> vk::Extent2D;
    void set_prechain_resolution(uint32_t width, uint32_t height);

    [[nodiscard]] auto list_filter_controls() const
        -> std::vector<goggles::fc::FilterControlDescriptor>;
    [[nodiscard]] auto list_filter_controls(goggles::fc::FilterControlStage stage) const
        -> std::vector<goggles::fc::FilterControlDescriptor>;
    [[nodiscard]] auto set_filter_control_value(goggles::fc::FilterControlId control_id,
                                                float value) -> bool;
    [[nodiscard]] auto reset_filter_control_value(goggles::fc::FilterControlId control_id) -> bool;
    void reset_filter_controls();

    [[nodiscard]] auto get_captured_extent() const -> vk::Extent2D {
        return m_external_frame_importer.import_extent;
    }
    [[nodiscard]] auto target_fps() const -> uint32_t { return m_render_output.target_fps; }
    [[nodiscard]] auto get_scale_mode() const -> ScaleMode { return m_scale_mode; }
    [[nodiscard]] auto get_integer_scale() const -> uint32_t { return m_integer_scale; }
    void set_target_fps(uint32_t target_fps) { update_target_fps(target_fps); }
    void set_scale_mode(ScaleMode mode) { m_scale_mode = mode; }
    void set_integer_scale(uint32_t scale) { m_integer_scale = scale; }
    [[nodiscard]] auto gpu_index() const -> uint32_t { return m_vulkan_context.gpu_index; }
    [[nodiscard]] auto gpu_uuid() const -> const std::string& { return m_vulkan_context.gpu_uuid; }

    [[nodiscard]] auto consume_chain_swapped() -> bool {
        return m_filter_chain_controller.consume_chain_swapped();
    }

private:
    VulkanBackend() = default;

    void initialize_paths(const std::filesystem::path& cache_dir);
    void initialize_settings(const RenderSettings& settings);
    void update_target_fps(uint32_t target_fps) { m_render_output.set_target_fps(target_fps); }

    [[nodiscard]] auto init_filter_chain() -> Result<void>;
    [[nodiscard]] auto make_filter_chain_build_config() const
        -> backend_internal::FilterChainController::AdapterBuildConfig;

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
