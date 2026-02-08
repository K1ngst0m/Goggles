#pragma once

#include "pass.hpp"

#include <vector>

namespace goggles::render {

/// @brief Configuration for creating a `DownsamplePass`.
struct DownsamplePassConfig {
    vk::Format target_format = vk::Format::eR8G8B8A8Unorm;
    uint32_t num_sync_indices = 2;
    std::filesystem::path shader_dir;
};

/// @brief Pre-chain pass that downsamples captured frames using area filtering.
class DownsamplePass : public Pass {
public:
    /// @brief Creates a downsample pass for the given target format.
    /// @return A pass or an error.
    [[nodiscard]] static auto create(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                                     const DownsamplePassConfig& config)
        -> ResultPtr<DownsamplePass>;

    ~DownsamplePass() override;

    DownsamplePass(const DownsamplePass&) = delete;
    DownsamplePass& operator=(const DownsamplePass&) = delete;
    DownsamplePass(DownsamplePass&&) = delete;
    DownsamplePass& operator=(DownsamplePass&&) = delete;

    /// @brief Releases GPU resources owned by this pass.
    void shutdown() override;
    /// @brief Records commands to downsample the source texture.
    void record(vk::CommandBuffer cmd, const PassContext& ctx) override;

    [[nodiscard]] auto get_shader_parameters() const -> std::vector<ShaderParameter> override;
    void set_shader_parameter(const std::string& name, float value) override;

private:
    DownsamplePass() = default;
    [[nodiscard]] auto create_descriptor_resources() -> Result<void>;
    [[nodiscard]] auto create_pipeline_layout() -> Result<void>;
    [[nodiscard]] auto create_pipeline(ShaderRuntime& shader_runtime,
                                       const std::filesystem::path& shader_dir) -> Result<void>;
    [[nodiscard]] auto create_sampler() -> Result<void>;

    void update_descriptor(uint32_t frame_index, vk::ImageView source_view);

    vk::Device m_device;
    vk::Format m_target_format = vk::Format::eUndefined;
    uint32_t m_num_sync_indices = 0;

    vk::PipelineLayout m_pipeline_layout;
    vk::Pipeline m_pipeline;

    vk::DescriptorSetLayout m_descriptor_layout;
    vk::DescriptorPool m_descriptor_pool;
    std::vector<vk::DescriptorSet> m_descriptor_sets;

    vk::Sampler m_sampler;

    float m_filter_type = 0.0F;
};

} // namespace goggles::render
