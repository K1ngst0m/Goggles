#pragma once

#include "pass.hpp"

#include <vector>

namespace goggles::render {

struct OutputPassConfig {
    vk::Format target_format = vk::Format::eUndefined;
    uint32_t num_sync_indices = 2;
    std::filesystem::path shader_dir;
};

class OutputPass : public Pass {
public:
    [[nodiscard]] static auto create(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                                     const OutputPassConfig& config) -> ResultPtr<OutputPass>;

    ~OutputPass() override;

    OutputPass(const OutputPass&) = delete;
    OutputPass& operator=(const OutputPass&) = delete;
    OutputPass(OutputPass&&) = delete;
    OutputPass& operator=(OutputPass&&) = delete;

    void shutdown() override;
    void record(vk::CommandBuffer cmd, const PassContext& ctx) override;

private:
    OutputPass() = default;
    [[nodiscard]] auto create_descriptor_resources() -> Result<void>;
    [[nodiscard]] auto create_pipeline_layout() -> Result<void>;
    [[nodiscard]] auto create_pipeline(ShaderRuntime& shader_runtime,
                                       const std::filesystem::path& shader_dir) -> Result<void>;
    [[nodiscard]] auto create_sampler() -> Result<void>;

    void update_descriptor(uint32_t frame_index, vk::ImageView source_view);

    vk::Device m_device;
    vk::Format m_target_format = vk::Format::eUndefined;
    uint32_t m_num_sync_indices = 0;

    vk::UniquePipelineLayout m_pipeline_layout;
    vk::UniquePipeline m_pipeline;

    vk::UniqueDescriptorSetLayout m_descriptor_layout;
    vk::UniqueDescriptorPool m_descriptor_pool;
    std::vector<vk::DescriptorSet> m_descriptor_sets;

    vk::UniqueSampler m_sampler;
};

} // namespace goggles::render
