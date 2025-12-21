#pragma once

#include "filter_pass.hpp"
#include "framebuffer.hpp"
#include "output_pass.hpp"
#include "preset_parser.hpp"

#include <memory>
#include <render/texture/texture_loader.hpp>
#include <unordered_map>
#include <vector>

namespace goggles::render {

struct LoadedTexture {
    TextureData data;
    vk::UniqueSampler sampler;
};

struct FramebufferExtents {
    vk::Extent2D viewport;
    vk::Extent2D source;
};

class FilterChain {
public:
    FilterChain() = default;
    ~FilterChain();

    FilterChain(const FilterChain&) = delete;
    FilterChain& operator=(const FilterChain&) = delete;
    FilterChain(FilterChain&&) = delete;
    FilterChain& operator=(FilterChain&&) = delete;

    [[nodiscard]] auto init(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                            uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                            const std::filesystem::path& shader_dir) -> Result<void>;

    [[nodiscard]] auto load_preset(const std::filesystem::path& preset_path) -> Result<void>;

    void record(vk::CommandBuffer cmd, vk::ImageView original_view, vk::Extent2D original_extent,
                vk::ImageView swapchain_view, vk::Extent2D viewport_extent, uint32_t frame_index,
                ScaleMode scale_mode = ScaleMode::STRETCH, uint32_t integer_scale = 0);

    [[nodiscard]] auto handle_resize(vk::Extent2D new_viewport_extent) -> Result<void>;

    void shutdown();

    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }
    [[nodiscard]] auto pass_count() const -> size_t { return m_passes.size(); }

    [[nodiscard]] static auto calculate_pass_output_size(const ShaderPassConfig& pass_config,
                                                         vk::Extent2D source_extent,
                                                         vk::Extent2D viewport_extent)
        -> vk::Extent2D;

private:
    [[nodiscard]] auto ensure_framebuffers(const FramebufferExtents& extents,
                                           vk::Extent2D viewport_extent) -> Result<void>;

    [[nodiscard]] auto load_preset_textures() -> Result<void>;
    [[nodiscard]] auto create_texture_sampler(const TextureConfig& config) -> Result<vk::UniqueSampler>;

    VulkanContext m_vk_ctx;
    vk::Format m_swapchain_format = vk::Format::eUndefined;
    uint32_t m_num_sync_indices = 0;
    ShaderRuntime* m_shader_runtime = nullptr;
    std::filesystem::path m_shader_dir;

    std::vector<std::unique_ptr<FilterPass>> m_passes;
    std::vector<Framebuffer> m_framebuffers;
    OutputPass m_output_pass;

    PresetConfig m_preset;
    uint32_t m_frame_count = 0;

    std::unique_ptr<TextureLoader> m_texture_loader;
    std::unordered_map<std::string, LoadedTexture> m_texture_registry;
    std::unordered_map<std::string, size_t> m_alias_to_pass_index;

    ScaleMode m_last_scale_mode = ScaleMode::STRETCH;
    uint32_t m_last_integer_scale = 0;
    vk::Extent2D m_last_source_extent;

    bool m_initialized = false;
};

} // namespace goggles::render
