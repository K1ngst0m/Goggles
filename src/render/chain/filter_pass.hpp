#pragma once

#include "pass.hpp"
#include "preset_parser.hpp"
#include "semantic_binder.hpp"

#include <render/shader/retroarch_preprocessor.hpp>
#include <render/shader/slang_reflect.hpp>
#include <vector>

namespace goggles::render {

// FilterPass renders a RetroArch shader pass (single pass from a .slangp preset)
class FilterPass : public Pass {
public:
    FilterPass() = default;
    ~FilterPass() override;

    FilterPass(const FilterPass&) = delete;
    FilterPass& operator=(const FilterPass&) = delete;
    FilterPass(FilterPass&&) = delete;
    FilterPass& operator=(FilterPass&&) = delete;

    // Initialize from a ShaderPassConfig (from preset parser)
    [[nodiscard]] auto init(vk::Device device, vk::Format target_format, uint32_t num_sync_indices,
                            ShaderRuntime& shader_runtime,
                            const std::filesystem::path& shader_dir) -> Result<void> override;

    // Initialize directly from preprocessed shader sources
    [[nodiscard]] auto init_from_sources(vk::Device device, vk::Format target_format,
                                          uint32_t num_sync_indices,
                                          ShaderRuntime& shader_runtime,
                                          const std::string& vertex_source,
                                          const std::string& fragment_source,
                                          const std::string& shader_name,
                                          FilterMode filter_mode = FilterMode::LINEAR)
        -> Result<void>;

    void shutdown() override;
    void record(vk::CommandBuffer cmd, const PassContext& ctx) override;

    // Update semantic values for the current frame
    void set_source_size(uint32_t width, uint32_t height) {
        m_binder.set_source_size(width, height);
    }
    void set_output_size(uint32_t width, uint32_t height) {
        m_binder.set_output_size(width, height);
    }
    void set_original_size(uint32_t width, uint32_t height) {
        m_binder.set_original_size(width, height);
    }
    void set_frame_count(uint32_t count) { m_binder.set_frame_count(count); }

    [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

private:
    [[nodiscard]] auto create_descriptor_resources() -> Result<void>;
    [[nodiscard]] auto create_pipeline_layout() -> Result<void>;
    [[nodiscard]] auto create_pipeline(const std::vector<uint32_t>& vertex_spirv,
                                       const std::vector<uint32_t>& fragment_spirv)
        -> Result<void>;
    [[nodiscard]] auto create_sampler(FilterMode filter_mode) -> Result<void>;

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

    // Semantic binding for RetroArch uniforms
    SemanticBinder m_binder;

    // Reflection data from shader compilation
    ReflectionData m_vertex_reflection;
    ReflectionData m_fragment_reflection;

    bool m_has_push_constants = false;
    bool m_initialized = false;
};

} // namespace goggles::render
