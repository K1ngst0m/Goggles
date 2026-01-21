#pragma once

#include "filter_pass.hpp"
#include "frame_history.hpp"
#include "framebuffer.hpp"
#include "output_pass.hpp"
#include "pass.hpp"
#include "preset_parser.hpp"

#include <atomic>
#include <memory>
#include <render/texture/texture_loader.hpp>
#include <unordered_map>
#include <vector>

namespace goggles::render {

/// @brief User-facing parameter state for UI and overrides.
struct ParameterInfo {
    std::string name;
    std::string description;
    float current_value;
    float default_value;
    float min_value;
    float max_value;
    float step;
};

/// @brief Texture plus sampler bound into a filter chain.
struct LoadedTexture {
    TextureData data;
    vk::UniqueSampler sampler;
};

/// @brief Viewport and source extents used for framebuffer sizing.
struct FramebufferExtents {
    vk::Extent2D viewport;
    vk::Extent2D source;
};

/// @brief Multi-pass shader pipeline configured from a preset file.
class FilterChain {
public:
    /// @brief Creates a filter chain and its passes for the given swapchain format.
    /// @param source_resolution Optional pre-chain target resolution (0,0 = disabled).
    /// @return A filter chain or an error.
    [[nodiscard]] static auto create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                                     uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                                     const std::filesystem::path& shader_dir,
                                     vk::Extent2D source_resolution = {0, 0})
        -> ResultPtr<FilterChain>;

    ~FilterChain();

    FilterChain(const FilterChain&) = delete;
    FilterChain& operator=(const FilterChain&) = delete;
    FilterChain(FilterChain&&) = delete;
    FilterChain& operator=(FilterChain&&) = delete;

    /// @brief Loads a preset and rebuilds passes and resources.
    [[nodiscard]] auto load_preset(const std::filesystem::path& preset_path) -> Result<void>;

    /// @brief Records all passes for the current frame.
    void record(vk::CommandBuffer cmd, vk::Image original_image, vk::ImageView original_view,
                vk::Extent2D original_extent, vk::ImageView swapchain_view,
                vk::Extent2D viewport_extent, uint32_t frame_index,
                ScaleMode scale_mode = ScaleMode::stretch, uint32_t integer_scale = 0);

    /// @brief Handles viewport resize and resizes framebuffers as needed.
    [[nodiscard]] auto handle_resize(vk::Extent2D new_viewport_extent) -> Result<void>;

    /// @brief Releases GPU resources and pass state.
    void shutdown();

    [[nodiscard]] auto pass_count() const -> size_t { return m_passes.size(); }

    void set_bypass(bool enabled) { m_bypass_enabled.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] auto is_bypass() const -> bool {
        return m_bypass_enabled.load(std::memory_order_relaxed);
    }

    /// @brief Computes the output extent for a pass given input sizes and scaling rules.
    [[nodiscard]] static auto calculate_pass_output_size(const ShaderPassConfig& pass_config,
                                                         vk::Extent2D source_extent,
                                                         vk::Extent2D viewport_extent)
        -> vk::Extent2D;

    /// @brief Returns all parameters exposed by the chain.
    [[nodiscard]] auto get_all_parameters() const -> std::vector<ParameterInfo>;
    /// @brief Overrides a parameter value by name.
    void set_parameter(const std::string& name, float value);
    /// @brief Resets a parameter override by name.
    void reset_parameter(const std::string& name);
    /// @brief Clears all parameter overrides.
    void clear_parameter_overrides();

private:
    FilterChain() = default;
    [[nodiscard]] auto ensure_framebuffers(const FramebufferExtents& extents,
                                           vk::Extent2D viewport_extent) -> Result<void>;
    [[nodiscard]] auto ensure_frame_history(vk::Extent2D extent) -> Result<void>;

    [[nodiscard]] auto load_preset_textures() -> Result<void>;
    [[nodiscard]] auto create_texture_sampler(const TextureConfig& config) const
        -> Result<vk::UniqueSampler>;

    void bind_pass_textures(FilterPass& pass, size_t pass_index, vk::ImageView original_view,
                            vk::Extent2D original_extent, vk::ImageView source_view);
    void copy_feedback_framebuffers(vk::CommandBuffer cmd);

    [[nodiscard]] auto ensure_prechain_passes(vk::Extent2D captured_extent) -> Result<void>;

    struct ChainResult {
        vk::ImageView view;
        vk::Extent2D extent;
    };
    auto record_prechain(vk::CommandBuffer cmd, vk::ImageView original_view,
                         vk::Extent2D original_extent, uint32_t frame_index) -> ChainResult;
    void record_postchain(vk::CommandBuffer cmd, vk::ImageView source_view,
                          vk::Extent2D source_extent, vk::ImageView target_view,
                          vk::Extent2D target_extent, uint32_t frame_index, ScaleMode scale_mode,
                          uint32_t integer_scale);

    VulkanContext m_vk_ctx;
    vk::Format m_swapchain_format = vk::Format::eUndefined;
    uint32_t m_num_sync_indices = 0;
    ShaderRuntime* m_shader_runtime = nullptr;
    std::filesystem::path m_shader_dir;

    std::vector<std::unique_ptr<FilterPass>> m_passes;
    std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;

    PresetConfig m_preset;
    uint32_t m_frame_count = 0;

    std::unique_ptr<TextureLoader> m_texture_loader;
    std::unordered_map<std::string, LoadedTexture> m_texture_registry;
    std::unordered_map<std::string, size_t> m_alias_to_pass_index;
    std::unordered_map<size_t, std::unique_ptr<Framebuffer>> m_feedback_framebuffers;

    ScaleMode m_last_scale_mode = ScaleMode::stretch;
    uint32_t m_last_integer_scale = 0;
    vk::Extent2D m_last_source_extent;

    FrameHistory m_frame_history;
    uint32_t m_required_history_depth = 0;
    std::atomic<bool> m_bypass_enabled{false};

    // Pre-chain stage
    vk::Extent2D m_source_resolution; // 0,0 = disabled
    std::vector<std::unique_ptr<Pass>> m_prechain_passes;
    std::vector<std::unique_ptr<Framebuffer>> m_prechain_framebuffers;

    // Post-chain stage
    std::vector<std::unique_ptr<Pass>> m_postchain_passes;
    std::vector<std::unique_ptr<Framebuffer>> m_postchain_framebuffers;
};

} // namespace goggles::render
