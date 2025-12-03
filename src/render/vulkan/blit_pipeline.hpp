#pragma once

#include "vulkan_config.hpp"

#include <pipeline/shader_runtime/shader_runtime.hpp>
#include <util/error.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace goggles::render {

class BlitPipeline {
public:
    BlitPipeline() = default;
    ~BlitPipeline();

    BlitPipeline(const BlitPipeline&) = delete;
    BlitPipeline& operator=(const BlitPipeline&) = delete;
    BlitPipeline(BlitPipeline&&) = delete;
    BlitPipeline& operator=(BlitPipeline&&) = delete;

    [[nodiscard]] auto init(vk::Device device,
                            vk::Format swapchain_format,
                            vk::Extent2D swapchain_extent,
                            const std::vector<vk::ImageView>& swapchain_views,
                            pipeline::ShaderRuntime& shader_runtime,
                            const std::filesystem::path& shader_dir) -> Result<void>;

    void shutdown();

    void recreate_framebuffers(vk::Extent2D swapchain_extent,
                               const std::vector<vk::ImageView>& swapchain_views);

    void update_descriptor(vk::ImageView source_view);

    void record_commands(vk::CommandBuffer cmd,
                         uint32_t framebuffer_index,
                         vk::Extent2D extent);

    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

private:
    [[nodiscard]] auto create_render_pass(vk::Format format) -> Result<void>;
    [[nodiscard]] auto create_descriptor_resources() -> Result<void>;
    [[nodiscard]] auto create_pipeline_layout() -> Result<void>;
    [[nodiscard]] auto create_pipeline(pipeline::ShaderRuntime& shader_runtime,
                                       const std::filesystem::path& shader_dir) -> Result<void>;
    [[nodiscard]] auto create_framebuffers(vk::Extent2D extent,
                                           const std::vector<vk::ImageView>& views) -> Result<void>;
    [[nodiscard]] auto create_sampler() -> Result<void>;

    vk::Device m_device;

    vk::UniqueRenderPass m_render_pass;
    vk::UniquePipelineLayout m_pipeline_layout;
    vk::UniquePipeline m_pipeline;

    vk::UniqueDescriptorSetLayout m_descriptor_layout;
    vk::UniqueDescriptorPool m_descriptor_pool;
    vk::DescriptorSet m_descriptor_set;

    vk::UniqueSampler m_sampler;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;

    bool m_initialized = false;
};

} // namespace goggles::render
